# This project is best built with mk-configure(7).
# To build the project, type `mkcmake`.

MKC_REQD        = 0.23.0

PROG            = inputplug

MAN             = inputplug.1

CLEANFILES      = ${MAN}

PKG_CONFIG_DEPS = x11 xext xi

MKC_CHECK_HEADERS  = ixp.h
MKC_CHECK_FUNCLIBS = ixp_mount:ixp

WARNS           = 2

VERSION         = 0.1

.SUFFIXES: .md .pod

.pod.md:
	pod2markdown < $< | sed -e 's, - , — ,g' \
	                        -e 's,^- ,* ,g'  \
	                        -e 's,man.he.net/man./,manpages.debian.org/cgi-bin/man.cgi?query=,g' \
	                        -e 's,\[\(<.*@.*>\)\](.*),\1,' > $@

.include <mkc.mk>

POD2MAN_FLAGS += -d 2013-11-17
