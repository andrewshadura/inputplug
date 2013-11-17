CFLAGS = -Wall -Wextra -pedantic -std=c99
LDLIBS = $(shell pkg-config --libs x11 xext xi)

all: inputplug inputplug.1

inputplug: inputplug.o

inputplug.1: inputplug.pod
	pod2man -r "" -c "" -n $(shell echo $(@:%.1=%) | tr a-z A-Z) $< > $@

inputplug.md: inputplug.pod
	pod2markdown < $< | sed -e 's, - , — ,g' \
	                        -e 's,^- ,* ,g'  \
	                        -e 's,man.he.net/man./,manpages.debian.org/cgi-bin/man.cgi?query=,g' \
	                        -e 's,\[\(<.*@.*>\)\](.*),\1,' > $@
