OBJS=transport.o connection.o net_helpers.o websocket.o mux.o flash_policy.o flash_protocol.o
BINS=mux

BASIC_DEPS=Makefile
HTTP_PARSER_DEPS=../deps/http-parser/http_parser.o
HIREDIS_DEPS=../deps/hiredis/libhiredis.a

DEPS_LIB_BINS=../deps/http-parser/http_parser.o ../deps/hiredis/libhiredis.a

CC=gcc
LDFLAGS?=-L../deps/hiredis
# OPTIMIZATION?=-O2
OPTIMIZATION?=
CFLAGS?=$(OPTIMIZATION) -std=c99 -Wall -W -Wstrict-prototypes -Wwrite-strings
CCLINK?=-lcrypto -lev
LIBS?=-I../deps/http-parser -I../deps/hiredis
CCOPT= $(CFLAGS) $(CCLINK)
DEBUG?= -g -ggdb

default: all
all: $(BINS) $(DEPS_LIB_BINS)

mux.o            : $(BASIC_DEPS) mux.c websocket.h  net_helpers.h common.h
net_helpers.o    : $(BASIC_DEPS) net_helpers.c net_helpers.h common.h
flash_protocol.o : $(BASIC_DEPS) flash_protocol.c common.h websocket.h net_helpers.h
flash_policy.o   : $(BASIC_DEPS) flash_policy.c flash_policy.h net_helpers.h common.h
websocket.o      : $(BASIC_DEPS) websocket.c websocket.h
connection.o     : $(BASIC_DEPS) connection.c connection.h common.h
transport.o      : $(BASIC_DEPS) transport.c transport.h common.h

../deps/http-parser/http_parser.o:
	make -C ../deps/http-parser
../deps/hiredis/libhiredis.a:
	make -C ../deps/hiredis

mux: $(OBJS)
	$(CC) -o $@ $(CCOPT) $(LDFLAGS) $(DEBUG) $^ $(DEPS_LIB_BINS)

.c.o:
	$(CC) -c $(LIBS) $(LDFLAGS) $(CFLAGS) $(DEBUG) $<

love:
	@echo Get a Life

clean:
	rm -rf $(BINS) *.o *.dSYM

