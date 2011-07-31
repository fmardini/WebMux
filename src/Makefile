OBJS=http_helpers.o transport.o polling.o connection.o net_helpers.o websocket.o mux.o flash_policy.o flash_protocol.o
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

BASIC_DEPS=Makefile common.h

mux.o            : $(BASIC_DEPS) mux.c flash_policy.h net_helpers.h flash_protocol.h transport.h connection.h
net_helpers.o    : $(BASIC_DEPS) net_helpers.c net_helpers.h
flash_protocol.o : $(BASIC_DEPS) flash_protocol.c flash_protocol.h transport.h connection.h net_helpers.h
flash_policy.o   : $(BASIC_DEPS) flash_policy.c flash_policy.h net_helpers.h
websocket.o      : $(BASIC_DEPS) websocket.c websocket.h transport.h connection.h net_helpers.h
polling.o        : $(BASIC_DEPS) polling.c polling.h transport.h connection.h net_helpers.h
connection.o     : $(BASIC_DEPS) connection.c connection.h transport.h net_helpers.h
transport.o      : $(BASIC_DEPS) transport.c transport.h connection.h net_helpers.h
http_helpers.o   : $(BASIC_DEPS) http_helpers.c http_helpers.h connection.h transport.h net_helpers.h

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

check-syntax:
	gcc -o /dev/null -D$(DEFINE) $(LIBS) $(CFLAGS) -S ${CHK_SOURCES}

