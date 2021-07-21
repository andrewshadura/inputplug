build: inputplug.1 inputplug.md
	cargo build --release

install: build
ifeq ($(DESTDIR),)
	cargo install --path .
else
	cargo install --root "$(DESTDIR)" --path .
endif

inputplug.1: inputplug.pod
	pod2man -r "" -c "" -n $(shell echo $(@:%.1=%) | tr a-z A-Z) $< > $@

inputplug.md: inputplug.pod
	pod2markdown < $< | sed -e 's, - , — ,g' \
	                        -e 's,^- ,* ,g'  \
	                        -e 's,man.he.net/man./,manpages.debian.org/,g' \
	                        -e 's,\[\(<.*@.*>\)\](.*),\1,' > $@
