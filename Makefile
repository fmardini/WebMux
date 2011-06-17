#   LDFLAGS: -L/usr/local/Cellar/openssl/0.9.8r/lib
#   CPPFLAGS: -I/usr/local/Cellar/openssl/0.9.8r/include
#   gcc -L../deps/http-parser  -lhttp_parser  -lcrypto -lev -pedantic -Wall -std=c99 -g -ggdb mux.c -o mux
#   gcc -I../deps/http-parser -L../deps/http-parser  -lhttp_parser  -lcrypto -lev -pedantic -Wall -std=c99 -g -ggdb websocket.c callbacks.c net_helpers.c mux.c -o mux

dependencies:
	@echo MAKE hashit
	@cd deps/hashit-0.9.7 && cmake . && $(MAKE)
	@echo MAKE hiredis
	@cd deps/hiredis && $(MAKE)
	@echo MAKE http_parser
	@cd deps/http-parser && $(MAKE) package
