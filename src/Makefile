OBJS=callbacks.o net_helpers.o websocket.o mux.o http_parser.o
BINS=mux
VPATH=../deps/http-parser

# gcc -I../deps/http-parser -L../deps/http-parser -lhttp_parser -lcrypto -lev -pedantic -Wall -std=c99 -g -ggdb websocket.c callbacks.c net_helpers.c mux.c -o mux

CC=gcc
OPTIMIZATION?=-O2
CFLAGS?=$(OPTIMIZATION) -std=c99 -Wall -W -Wstrict-prototypes -Wwrite-strings -pedantic $(ARCH) $(PROF)
CCLINK?=-lcrypto -lev
LIBS?=-I../deps/http-parser
LDFLAGS?=

CCOPT= $(CFLAGS) $(CCLINK)
DEBUG?= -g -ggdb

all: $(BINS)

dependencies:
	@echo MAKE hiredis
	@cd ../deps/hiredis && $(MAKE)

callbacks.o: callbacks.c callbacks.h websocket.h
mux.o: mux.c websocket.h  callbacks.h net_helpers.h
net_helpers.o: net_helpers.c net_helpers.h

# Binaries:
mux: $(OBJS)
	$(CC) -o $@ $(CCOPT) $(DEBUG) $^

http_parser.o:http_parser.h http_parser.c
	$(CC) -c $(LIBS) $(LDFLAGS) $(CFLAGS) $(DEBUG) $<

.c.o:
	$(CC) -c $(LIBS) $(LDFLAGS) $(CFLAGS) $(DEBUG) $<

clean:
	rm -rf $(BINS) *.o *.dSYM
