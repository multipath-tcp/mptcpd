## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2019, Intel Corporation

man_MANS = mptcpd.8

pkgsysconfdir = @sysconfdir@/@PACKAGE@

## The configure script won't fully expand ${prefix} so leverage
## `make' based variable expansion instead.
mptcpd.8: Makefile mptcpd.8.in
	$(AM_V_GEN)rm -f $@ $@.tmp; \
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		sed \
			-e 's,@pkgsysconfdir[@],$(pkgsysconfdir),g' \
			-e 's,@PACKAGE_BUGREPORT[@],$(PACKAGE_BUGREPORT),g' \
			$${srcdir}$@.in >$@.tmp; \
	chmod 644 $@.tmp; \
	mv $@.tmp $@

EXTRA_DIST = mptcpd.8.in

CLEANFILES = $(man_MANS)
