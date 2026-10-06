# cmaze - a terminal maze generator, in C + ncurses
#
#   make            build
#   make install    install the binary and the man page into $(PREFIX)
#   make clean

CC       ?= cc
PREFIX   ?= /usr/local
BINDIR    = $(PREFIX)/bin
MANDIR    = $(PREFIX)/share/man/man6

CFLAGS   ?= -O2
CFLAGS   += -std=c99 -Wall -Wextra
CPPFLAGS += -D_XOPEN_SOURCE_EXTENDED=1 -I.
LDLIBS   ?= -lncurses

BIN = cmaze
SRC = $(wildcard *.c) $(wildcard algos/*.c)
OBJ = $(SRC:.c=.o)
HDR = cmaze.h algos/algos.h

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: $(BIN) cmaze.6
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -d $(DESTDIR)$(MANDIR)
	install -m 644 cmaze.6 $(DESTDIR)$(MANDIR)/cmaze.6

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -f $(DESTDIR)$(MANDIR)/cmaze.6

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all install uninstall clean
