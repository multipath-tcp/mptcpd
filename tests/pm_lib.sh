#!/bin/bash
MPTCPD_BIN="${MPTCPD_BIN:-../src/mptcpd}"
CONN_BIN="${CONN_BIN:-./mptcp-conn}"
PLUGIN_DIR="${PLUGIN_DIR:-../plugins/path_managers/.libs}"

PM_NS1="pm_ns1"
PM_NS2="pm_ns2"
PM_NS1_ETH="ns1eth1"
PM_NS2_ETH="ns2eth1"
PM_NS1_ETH2="ns1eth2"
PM_NS2_ETH2="ns2eth2"

PM_SKIP=77
PM_RET=0

pm_pass() { echo "PASS: $*"; }
pm_fail() { echo "FAIL: $*"; PM_RET=1; }
pm_skip() { echo "SKIP: $*"; exit "$PM_SKIP"; }

pm_check_prereqs() {
	[ -d /proc/sys/net/mptcp ] || pm_skip "no MPTCP kernel"
	[ "$(id -u)" = 0 ] || pm_skip "must run as root"
	[ -f /proc/sys/net/mptcp/pm_type ] || pm_skip "no pm_type sysctl"
	[ -x "$MPTCPD_BIN" ] || pm_skip "mptcpd not built"
	[ -x "$CONN_BIN" ] || pm_skip "mptcp-conn not built"
}

cleanup() {
	local ns pid
	# force-kill anything that ignored SIGTERM
	for ns in "$PM_NS1" "$PM_NS2"; do
		for pid in $(ip netns pids "$ns" 2>/dev/null); do
			kill -KILL "$pid" 2>/dev/null
		done
	done

	# removing the namespaces also drops the veth pairs automatically
	for ns in "$PM_NS1" "$PM_NS2"; do
		ip netns del "$ns" 2>/dev/null
	done

	exit "$PM_RET"
}

pm_setup_env() {
	trap cleanup EXIT

	ip netns add "$PM_NS1"
	ip netns add "$PM_NS2"

	ip link add "$PM_NS1_ETH" netns "$PM_NS1" type veth \
		peer name "$PM_NS2_ETH" netns "$PM_NS2"
	ip link add "$PM_NS1_ETH2" netns "$PM_NS1" type veth \
		peer name "$PM_NS2_ETH2" netns "$PM_NS2"

	ip -net "$PM_NS1" link set "$PM_NS1_ETH"  up
	ip -net "$PM_NS2" link set "$PM_NS2_ETH" up
	ip -net "$PM_NS1" link set "$PM_NS1_ETH2"  up
	ip -net "$PM_NS2" link set "$PM_NS2_ETH2" up

	ip -net "$PM_NS1" addr add 10.0.1.1/24 dev "$PM_NS1_ETH"
	ip -net "$PM_NS2" addr add 10.0.1.2/24 dev "$PM_NS2_ETH"
	ip -net "$PM_NS1" addr add 10.0.2.1/24 dev "$PM_NS1_ETH2"
	ip -net "$PM_NS2" addr add 10.0.2.2/24 dev "$PM_NS2_ETH2"
}

pm_start_mptcpd() {
	ip netns exec "$1" "$MPTCPD_BIN" --plugin-dir="$PLUGIN_DIR" \
		--path-manager="$2" ${3:+--addr-flags="$3"} \
	        --load-plugins="$2" --notify-flags=existing 2>&1 &
}

pm_do_conn() {
	 ip netns exec "$PM_NS1" "$CONN_BIN" -l -p "$2" -w 50 2>&1 &
	 PM_SRV_PID=$!

	 sleep 2

	 ip netns exec "$PM_NS2" "$CONN_BIN" -h "$1" -p "$2" -w 40 2>&1 &
	 PM_CLI_PID=$!
}
