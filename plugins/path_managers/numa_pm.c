// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file numa_pm.c
 *
 * @brief NUMA source-port affinity path manager plugin.
 *
 * On connection establishment the plugin probes the NUMA node of the
 * application process owning the MPTCP socket and adds a subflow whose
 * source port is taken from that node's RSS queue range.  Once the
 * new subflow completes its handshake, the original subflow is set to
 * backup.
 *
 * The probe chain is: original subflow sport -> inet_diag socket
 * inode -> /proc/<pid>/fd -> pid -> Cpus_allowed_list and node cpulist
 * intersection.  It falls back to the current scheduling CPU.
 * Connections whose node cannot be determined are left untouched.
 *
 * At init the plugin reads a single text file holding the remote
 * port filter and the per-node port pools.  The file defaults to
 * /etc/mptcpd/numa_pm.conf and can be overridden with the
 * NUMA_PM_CONF environment variable (used by the test suite).  The
 * remote port filter is mandatory:
 *
 *      # comment
 *      dport = 6379
 *      --- node 0 ---
 *      port=10001
 *      port=10002
 *      --- node 1 ---
 *      port=10011
 *
 * The port pools are deployment specific: for a given network path,
 * each candidate source port maps to the NUMA node that the NIC's
 * RSS steers it to, and must be derived per machine by computation
 * or measurement.
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>

#include <ell/ell.h>

#include <mptcpd/plugin.h>
#include <mptcpd/path_manager.h>

#define MAX_CONNS               4096
#define MAX_RETRY               4
#define MAX_NODE                8
#define POOL_PER_NODE           8192
#define NUMA_PM_CONF_DEFAULT    "/etc/mptcpd/numa_pm.conf"

struct conn {
        mptcpd_token_t token;
        uint16_t f1;            // original subflow sport
        uint16_t cand;          // engineered subflow sport (0 = none)
        uint8_t id_next;        // local addr id, starts at 7, bumped on retry
        int node;               // probed application node (-1 = unknown)
        bool done;              // backup flag already sent
};

static struct {
        int dport;
        struct conn conns[MAX_CONNS];
        int nconn;
        uint16_t pool[MAX_NODE][POOL_PER_NODE];
        int npool[MAX_NODE];
        int used[MAX_NODE];
        uint64_t stat_create, stat_flags, stat_exhausted;
        uint64_t stat_probe_ok, stat_probe_cpu, stat_probe_fail;
} S;

// Parse a cpulist such as "0-3,8,40-47" into a bitmap.
static void parse_cpulist(const char *s, unsigned char *bits)
{
        int a, b;
        const char *p = s;

        while (*p && *p != '\n') {
                if (*p == ',') {
                        p++;
                        continue;
                }

                if (sscanf(p, "%d-%d", &a, &b) == 2) {
                        for (int i = a; i <= b && i < 256; i++)
                                bits[i] = 1;
                        while (*p && *p != ',' && *p != '\n')
                                p++;
                } else if (sscanf(p, "%d", &a) == 1) {
                        if (a >= 0 && a < 256)
                                bits[a] = 1;
                        while (*p && *p != ',' && *p != '\n')
                                p++;
                } else {
                        break;
                }
        }
}

static int cpu_to_node(int cpu)
{
        for (int n = 0; n < MAX_NODE; n++) {
                char path[128];

                snprintf(path, sizeof path,
                         "/sys/devices/system/node/node%d/cpulist", n);

                FILE *f = fopen(path, "r");

                if (!f)
                        continue;

                char line[256];
                int hit = 0;

                if (fgets(line, sizeof line, f)) {
                        unsigned char nb[256] = {0};

                        parse_cpulist(line, nb);
                        hit = (cpu < 256 && nb[cpu]);
                }

                fclose(f);

                if (hit)
                        return n;
        }

        return -1;
}

// MPTCP subflows are invisible in /proc/net/tcp, so resolve the socket
// inode via an inet_diag DUMP and filter locally; a precise sockdiag
// query requires all id fields to match and would never hit.
static int diag_inode_by_sport(uint16_t sport, uint16_t dport,
                               uint32_t *ino_out)
{
        int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);

        if (fd < 0)
                return -1;

        struct {
                struct nlmsghdr nlh;
                struct inet_diag_req_v2 req;
        } req;

        memset(&req, 0, sizeof req);
        req.nlh.nlmsg_len   = NLMSG_LENGTH(sizeof req.req);
        req.nlh.nlmsg_type  = SOCK_DIAG_BY_FAMILY;
        req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
        req.req.sdiag_family   = AF_INET;
        req.req.sdiag_protocol = IPPROTO_TCP;
        req.req.idiag_states   = 1 << TCP_ESTABLISHED;

        struct sockaddr_nl sa = { .nl_family = AF_NETLINK };

        if (sendto(fd, &req, sizeof req, 0,
                   (struct sockaddr *)&sa, sizeof sa) < 0) {
                close(fd);
                return -1;
        }

        char buf[16384];
        ssize_t n;

        while ((n = recv(fd, buf, sizeof buf, 0)) > 0) {
                size_t len = (size_t)n;
                struct nlmsghdr *nh = (struct nlmsghdr *)buf;

                for (; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
                        if (nh->nlmsg_type == NLMSG_DONE ||
                            nh->nlmsg_type == NLMSG_ERROR) {
                                close(fd);
                                return -1;
                        }

                        struct inet_diag_msg *m = NLMSG_DATA(nh);

                        if (m->idiag_family != AF_INET)
                                continue;

                        if (m->id.idiag_sport != htons(sport))
                                continue;

                        if (m->id.idiag_dport != htons(dport))
                                continue;

                        if (!m->idiag_inode)
                                continue;

                        *ino_out = m->idiag_inode;
                        close(fd);
                        return 0;
                }
        }

        close(fd);
        return -1;
}

static pid_t pid_by_inode(uint32_t ino)
{
        char want[64];

        snprintf(want, sizeof want, "socket:[%u]", ino);

        DIR *d = opendir("/proc");

        if (!d)
                return 0;

        struct dirent *e;
        pid_t found = 0;

        while ((e = readdir(d)) != NULL && !found) {
                if (e->d_name[0] < '0' || e->d_name[0] > '9')
                        continue;

                char pfd[288];

                snprintf(pfd, sizeof pfd, "/proc/%s/fd", e->d_name);

                DIR *fd = opendir(pfd);

                if (!fd)
                        continue;

                struct dirent *fe;

                while ((fe = readdir(fd)) != NULL) {
                        char lp[600], tgt[384];
                        ssize_t n;

                        snprintf(lp, sizeof lp, "%s/%s", pfd, fe->d_name);
                        n = readlink(lp, tgt, sizeof tgt - 1);

                        if (n <= 0)
                                continue;

                        tgt[n] = 0;

                        if (strcmp(tgt, want) == 0) {
                                found = atoi(e->d_name);
                                break;
                        }
                }

                closedir(fd);
        }

        closedir(d);
        return found;
}

// Prefer a unique Cpus_allowed_list match, else the current CPU.
static int pid_to_node(pid_t pid)
{
        unsigned char allow[256] = {0};
        char path[128];

        snprintf(path, sizeof path, "/proc/%d/status", pid);

        FILE *f = fopen(path, "r");

        if (f) {
                char line[512];

                while (fgets(line, sizeof line, f)) {
                        if (!strncmp(line, "Cpus_allowed_list:", 18)) {
                                parse_cpulist(line + 18, allow);
                                break;
                        }
                }

                fclose(f);
        }

        int hit = -1, nhit = 0;

        for (int n = 0; n < MAX_NODE; n++) {
                char npath[128];

                snprintf(npath, sizeof npath,
                         "/sys/devices/system/node/node%d/cpulist", n);

                FILE *nf = fopen(npath, "r");

                if (!nf)
                        continue;

                char line[256];

                if (fgets(line, sizeof line, nf)) {
                        unsigned char nb[256] = {0};

                        parse_cpulist(line, nb);
                        for (int c = 0; c < 256; c++) {
                                if (nb[c] && allow[c]) {
                                        hit = n;
                                        nhit++;
                                        break;
                                }
                        }
                }

                fclose(nf);
        }

        if (hit >= 0 && nhit == 1)
                return hit;

        snprintf(path, sizeof path, "/proc/%d/stat", pid);
        f = fopen(path, "r");

        if (f) {
                char buf[2048];

                if (fgets(buf, sizeof buf, f)) {
                        char *p = strrchr(buf, ')');

                        if (p) {
                                int field = 2, cur = -1;

                                for (char *tok = strtok(p + 1, " ");
                                     tok; tok = strtok(NULL, " ")) {
                                        if (++field == 39) {
                                                cur = atoi(tok);
                                                break;
                                        }
                                }

                                if (cur >= 0) {
                                        int n = cpu_to_node(cur);

                                        if (n >= 0) {
                                                fclose(f);
                                                return n;
                                        }
                                }
                        }
                }

                fclose(f);
        }

        return -1;
}

static int load_config(const char *path)
{
        FILE *f = fopen(path, "r");

        if (!f)
                return -1;

        char line[128];
        int node = -1;

        while (fgets(line, sizeof line, f)) {
                int n;

                if (sscanf(line, "dport = %d", &n) == 1) {
                        if (n > 0 && n <= 0xffff)
                                S.dport = n;
                        continue;
                }

                if (sscanf(line, "--- node %d ---", &n) == 1) {
                        node = (n >= 0 && n < MAX_NODE) ? n : -1;
                        continue;
                }

                int p;

                if (node >= 0
                    && sscanf(line, "port=%d", &p) == 1
                    && S.npool[node] < POOL_PER_NODE)
                        S.pool[node][S.npool[node]++] = (uint16_t)p;
        }

        fclose(f);

        int total = 0;

        for (int i = 0; i < MAX_NODE; i++)
                total += S.npool[i];

        return total > 0 ? 0 : -1;
}

static struct conn *find_or_add(mptcpd_token_t token)
{
        for (int i = 0; i < S.nconn; i++) {
                if (S.conns[i].token == token)
                        return &S.conns[i];
        }

        if (S.nconn >= MAX_CONNS)
                return NULL;

        struct conn *c = &S.conns[S.nconn++];

        memset(c, 0, sizeof *c);
        c->token = token;
        c->id_next = 7;
        c->node = -1;

        return c;
}

static void drop_conn(mptcpd_token_t token)
{
        for (int i = 0; i < S.nconn; i++) {
                if (S.conns[i].token == token) {
                        memmove(&S.conns[i], &S.conns[i + 1],
                                (S.nconn - i - 1) * sizeof(struct conn));
                        S.nconn--;
                        return;
                }
        }
}

static void numa_new_connection(mptcpd_token_t token,
                                struct sockaddr const *laddr,
                                struct sockaddr const *raddr,
                                bool server_side,
                                bool deny_join_id0,
                                struct mptcpd_pm *pm)
{
        (void) deny_join_id0;
        (void) pm;

        if (server_side)
                return;

        struct sockaddr_in const *ra = (struct sockaddr_in const *)raddr;

        if (ntohs(ra->sin_port) != S.dport)
                return;

        struct conn *c = find_or_add(token);

        if (c)
                c->f1 = ntohs(((struct sockaddr_in const *)laddr)->sin_port);
}

static void do_create(mptcpd_token_t token,
                      struct sockaddr const *laddr,
                      struct sockaddr const *raddr,
                      struct mptcpd_pm *pm)
{
        struct sockaddr_in const *la = (struct sockaddr_in const *)laddr;
        struct sockaddr_in const *ra = (struct sockaddr_in const *)raddr;
        struct conn *c = find_or_add(token);

        if (!c)
                return;

        c->f1 = ntohs(la->sin_port);

        if (c->cand)
                return;         // already handled

        // Probe the application's NUMA node.  The ESTABLISHED event may
        // precede subflow visibility in diag, hence the retries.
        if (c->node < 0) {
                uint32_t ino = 0;
                pid_t pid = 0;
                int got = 0;

                for (int t2 = 0; t2 < 3 && !got; t2++) {
                        if (t2)
                                usleep(20000);

                        got = (diag_inode_by_sport(c->f1, S.dport, &ino) == 0);
                }

                if (got) {
                        pid = pid_by_inode(ino);

                        if (pid > 0) {
                                c->node = pid_to_node(pid);

                                if (c->node >= 0)
                                        S.stat_probe_ok++;
                                else
                                        S.stat_probe_cpu++;
                        } else {
                                S.stat_probe_fail++;
                        }
                } else {
                        S.stat_probe_fail++;
                }

                l_info("token=%u f1=%u probe: inode=%u pid=%d node=%d",
                       token, c->f1, ino, pid, c->node);
        }

        int node = c->node;

        if (node < 0) {
                l_info("token=%u f1=%u node unknown, skip subflow",
                       token, c->f1);
                return;
        }

        if (S.used[node] >= S.npool[node]) {
                S.stat_exhausted++;
                l_warn("token=%u f1=%u oracle pool exhausted (node=%d)",
                       token, c->f1, node);
                return;
        }

        struct sockaddr_in l = *la, r = *ra;
        uint8_t id = c->id_next;
        uint16_t cand = S.pool[node][S.used[node]++];

        c->cand = cand;
        l.sin_port = htons(cand);

        int rc = mptcpd_pm_add_subflow(pm, token, id, 0,
                                       (struct sockaddr *)&l,
                                       (struct sockaddr *)&r, false);

        l_info("token=%u f1=%u node=%d cand=%u id=%u add_subflow rc=%d (%s)",
               token, c->f1, node, cand, id, rc, rc ? strerror(rc) : "OK");

        if (rc) {
                c->cand = 0;
                c->id_next = id + 1 < 7 + MAX_RETRY ? id + 1 : id;
        } else {
                S.stat_create++;
        }
}

static void numa_connection_established(mptcpd_token_t token,
                                        struct sockaddr const *laddr,
                                        struct sockaddr const *raddr,
                                        bool server_side,
                                        bool deny_join_id0,
                                        struct mptcpd_pm *pm)
{
        if (server_side)
                return;

        // A peer that denies joins to the initial address/port rejects
        // the MP_JOIN this plugin sends, so do not create a subflow.
        if (deny_join_id0) {
                l_info("token=%u deny_join_id0 set, skip subflow", token);
                return;
        }

        struct sockaddr_in const *ra = (struct sockaddr_in const *)raddr;

        if (ntohs(ra->sin_port) != S.dport)
                return;

        do_create(token, laddr, raddr, pm);
}

static void numa_new_subflow(mptcpd_token_t token,
                             struct sockaddr const *laddr,
                             struct sockaddr const *raddr,
                             bool backup,
                             struct mptcpd_pm *pm)
{
        (void) backup;

        struct sockaddr_in const *la = (struct sockaddr_in const *)laddr;
        struct sockaddr_in const *ra = (struct sockaddr_in const *)raddr;
        uint16_t sport = ntohs(la->sin_port);
        struct conn *c = find_or_add(token);

        if (!c || sport != c->cand || c->done)
                return;

        struct sockaddr_in fl = *la, fr = *ra;

        fl.sin_port = htons(c->f1);

        int rc = mptcpd_pm_set_backup(pm, token,
                                      (struct sockaddr *)&fl,
                                      (struct sockaddr *)&fr, true);

        c->done = true;

        if (rc) {
                l_error("token=%u cand=%u set_backup(f1=%u) rc=%d FAIL",
                        token, sport, c->f1, rc);
        } else {
                S.stat_flags++;
                l_info("token=%u cand=%u set_backup(f1=%u) OK",
                       token, sport, c->f1);
        }
}

static void numa_connection_closed(mptcpd_token_t token,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        drop_conn(token);
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = numa_new_connection,
        .connection_established = numa_connection_established,
        .new_subflow            = numa_new_subflow,
        .connection_closed      = numa_connection_closed,
};

static int numa_init(struct mptcpd_pm *pm)
{
        (void) pm;

        static char const name[] = "numa_pm";

        char const *conf = getenv("NUMA_PM_CONF");

        if (!conf)
                conf = NUMA_PM_CONF_DEFAULT;

        if (load_config(conf) != 0) {
                l_error("numa_pm init failed: cannot load config %s", conf);
                return -1;
        }

        if (S.dport <= 0) {
                l_error("numa_pm init failed: dport missing in %s", conf);
                return -1;
        }

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("numa_pm: failed to register plugin operations");
                return -1;
        }

        l_info("=== numa_pm init OK: conf=%s pool=%d/%d/%d/%d dport=%d ===",
               conf,
               S.npool[0], S.npool[1], S.npool[2], S.npool[3], S.dport);

        return 0;
}

static void numa_exit(struct mptcpd_pm *pm)
{
        (void) pm;

        l_info("=== numa_pm exit: create=%lu flags=%lu probe_ok=%lu probe_cpu=%lu probe_fail=%lu exhausted=%lu leftover_conn=%d ===",
           (unsigned long)S.stat_create, (unsigned long)S.stat_flags,
           (unsigned long)S.stat_probe_ok, (unsigned long)S.stat_probe_cpu,
           (unsigned long)S.stat_probe_fail,
           (unsigned long)S.stat_exhausted, S.nconn);
}

MPTCPD_PLUGIN_DEFINE(numa_pm,
                     "NUMA source-port affinity path manager",
                     MPTCPD_PLUGIN_PRIORITY_DEFAULT,
                     numa_init,
                     numa_exit)

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
