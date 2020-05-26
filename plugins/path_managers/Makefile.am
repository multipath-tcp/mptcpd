## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2019, Intel Corporation

include $(top_srcdir)/aminclude_static.am

pkglib_LTLIBRARIES = sspi.la

sspi_la_SOURCES  = sspi.c
sspi_la_CPPFLAGS = -I$(top_srcdir)/include $(CODE_COVERAGE_CPPFLAGS)
sspi_la_CFLAGS   = 		\
	$(ELL_CFLAGS)		\
	$(CODE_COVERAGE_CFLAGS)
sspi_la_LDFLAGS  =       \
	-no-undefined    \
	-module          \
	-avoid-version
sspi_la_LIBADD   =                       \
        $(top_builddir)/lib/libmptcpd.la \
        $(CODE_COVERAGE_LIBS)

# Make sure mptcpd plugin directory is not world writable.
install-exec-local: installdirs
	chmod o-w $(DESTDIR)$(pkglibdir)

install-exec-hook: installcheck-local

installcheck-local:
	$(top_srcdir)/scripts/check-permissions $(DESTDIR)$(pkglibdir)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean
