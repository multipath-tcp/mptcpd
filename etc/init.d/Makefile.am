## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2022, Intel Corporation

EXTRA_DIST = mptcpd.in

initddir = $(sysconfdir)/init.d
initd_SCRIPTS = mptcpd

## The configure script won't fully expand variables like $libexecdir
## so leverage `make' based variable expansion instead.
if HAVE_SYSTEMD
mptcpdbindir = @libexecdir@
else
mptcpdbindir = @bindir@
endif

mptcpd: Makefile mptcpd.in
	$(AM_V_GEN)rm -f $@ $@.tmp; \
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		sed \
			-e 's,@libdir[@],$(libdir),g' \
			-e 's,@mptcpdbindir[@],$(mptcpdbindir),g' \
			$${srcdir}$@.in >$@.tmp; \
	chmod 755 $@.tmp; \
	mv $@.tmp $@

CLEANFILES = mptcpd

# Make sure the system init.d directory is not world writable.
install-data-hook: installcheck-local
	chmod o-w $(DESTDIR)$(initddir)

installcheck-local:
	$(top_srcdir)/scripts/check-permissions \
		$(DESTDIR)$(initddir)
