# This project is best built with mk-configure(7).
# To build the project, type `mkcmake`.

MKC_REQD        = 0.23.0

PROG            = inputplug

MAN             = inputplug.1

CLEANFILES      = ${MAN}

MKC_REQUIRE_PKGCONFIG = xcb xcb-xinput

MKC_REQUIRE_HEADERS = X11/extensions/XInput2.h
MKC_CHECK_HEADERS  = ixp.h bsd/libutil.h libutil.h
MKC_CHECK_FUNCLIBS = ixp_mount:ixp pidfile_open:bsd pidfile_open:util

CFLAGS_inputplug   = \
	${${HAVE_FUNCLIB.pidfile_open.bsd}:?-DHAVE_PIDFILE=1:} \
	${${HAVE_FUNCLIB.pidfile_open.util}:?-DHAVE_PIDFILE=1:} \

WARNS           = 2

VERSION         = 0.1

.SUFFIXES: .md .pod

.pod.md:
	pod2markdown < $< | sed -e 's, - , — ,g' \
	                        -e 's,^- ,* ,g'  \
	                        -e 's,man.he.net/man./,manpages.debian.org/cgi-bin/man.cgi?query=,g' \
	                        -e 's,\[\(<.*@.*>\)\](.*),\1,' > $@

.include <mkc.mk>

POD2MAN_FLAGS += -d 2013-12-07
