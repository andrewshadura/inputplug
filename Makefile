CFLAGS = -Wall -Wextra -pedantic -std=c99
LDLIBS = $(shell pkg-config --libs x11 xext xi)

inputplug: inputplug.o
