## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2018, 2019, 2022, Intel Corporation

SUBDIRS = init.d

EXTRA_DIST = mptcpd.conf.in

pkgsysconf_DATA = mptcpd.conf
pkgsysconfdir = $(sysconfdir)/@PACKAGE@

## The configure script won't fully expand $pkglibdir so leverage
## `make' based variable expansion instead.
mptcpd.conf: Makefile mptcpd.conf.in
	$(AM_V_GEN)rm -f $@ $@.tmp; \
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		sed \
			-e 's,@pkglibdir[@],$(pkglibdir),g' \
			-e 's,@mptcpd_logger[@],$(mptcpd_logger),g' \
			-e 's,@mptcpd_default_pm[@],$(mptcpd_default_pm),g' \
			$${srcdir}$@.in >$@.tmp; \
	chmod 644 $@.tmp; \
	mv $@.tmp $@

CLEANFILES = mptcpd.conf

# Make sure mptcpd system configuration directory is not world
# writable.
install-data-hook: installcheck-local
	chmod o-w $(DESTDIR)$(pkgsysconfdir)

installcheck-local:
	$(top_srcdir)/scripts/check-permissions \
		$(DESTDIR)$(pkgsysconfdir) \
		$(DESTDIR)$(pkgsysconfdir)/mptcpd.conf
