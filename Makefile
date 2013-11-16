CFLAGS = -Wall -Wextra -pedantic -std=c99
LDLIBS = $(shell pkg-config --libs x11 xext xi)

all: inputplug inputplug.1

inputplug: inputplug.o

inputplug.1: inputplug.c
	pod2man -r "" -c "" -n $(shell echo $(@:%.1=%) | tr a-z A-Z) $< > $@

inputplug.md: inputplug.c
	pod2markdown < $< | sed -e 's, - , — ,g' \
							-e 's,^- ,* ,g'  \
							-e '/=cut/,$$d'  \
							-e 's,\[\(<.*@.*>\)\](.*),\1,' > $@
