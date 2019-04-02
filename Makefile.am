## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2019, Intel Corporation

SUBDIRS = etc man include lib src plugins tests scripts

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
  --with-systemdsystemunitdir='$$(prefix)/$(systemdsystemunitdir)'

## Doxygen stuff
@DX_RULES@
MOSTLYCLEANFILES = $(DX_CLEANFILES)

EXTRA_DIST = README.md Doxyfile mptcpd.dox LICENSES

README: $(top_srcdir)/README.md
	@test -z "$(PANDOC)" || $(PANDOC) --from markdown_github --to plain --output=$@ --normalize $<

dist-hook: README

MAINTAINERCLEANFILES = README

include $(top_srcdir)/aminclude_static.am

clean-local: code-coverage-clean
