#   LDFLAGS: -L/usr/local/Cellar/openssl/0.9.8r/lib
#   CPPFLAGS: -I/usr/local/Cellar/openssl/0.9.8r/include
#   gcc -L../deps/http-parser  -lhttp_parser  -lcrypto -pedantic -Wall -std=c99 -g -ggdb mux.c -o mux

dependencies:
	@echo MAKE hashit
	@cd deps/hashit-0.9.7 && cmake . && $(MAKE)
