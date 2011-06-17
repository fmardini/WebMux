OBJS=callbacks.o net_helpers.o websocket.o mux.o
BINS=mux

# gcc -I../deps/http-parser -L../deps/http-parser  -lhttp_parser  -lcrypto -lev -pedantic -Wall -std=c99 -g -ggdb websocket.c callbacks.c net_helpers.c mux.c -o mux

CC=gcc
OPTIMIZATION?=-O3
CFLAGS?=$(OPTIMIZATION) -std=c99 -Wall -W -Wstrict-prototypes -Wwrite-strings -pedantic $(ARCH) $(PROF)
CCLINK?=-lhttp_parser  -lcrypto -lev
LDFLAGS?=-I../deps/http-parser -L../deps/http-parser

CCOPT= $(CFLAGS) $(CCLINK)
DEBUG?= -g -ggdb

all: $(BINS)

dependencies:
	@echo MAKE hiredis
	@cd deps/hiredis && $(MAKE)
	@echo MAKE http_parser
	@cd deps/http-parser && $(MAKE) package

callbacks.o: callbacks.c callbacks.h websocket.h
mux.o: mux.c websocket.h  callbacks.h net_helpers.h
net_helpers.o: net_helpers.c net_helpers.h
websocket.o: websocket.c websocket.h

# Binaries:
mux: $(OBJS)
	$(CC) -o $@ $(CCOPT) $(DEBUG) $(LDFLAGS) $(OBJS)

.c.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $(LDFLAGS) $<

clean:
	rm -rf $(BINS) *.o *.dSYM
