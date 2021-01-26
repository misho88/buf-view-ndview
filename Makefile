CC=gcc
CFLAGS=-fPIC -I./inc
LDFLAGS=-L./lib -lbuf -lndview

all: static_libs dynamic_libs

STATIC_LIBS=lib/libbuf.a  lib/libndview.a
DYNAMIC_LIBS=lib/libbuf.so lib/libndview.so

static_libs: $(STATIC_LIBS)
dynamic_libs: $(DYNAMIC_LIBS)
swig: python/_easyfft.so python/easyfft.py

lib/libbuf.so: src/buf.c inc/buf.h
	$(CC) -shared $(CFLAGS) $< -o $@

lib/libbuf.a: src/buf.c inc/buf.h
	$(CC) -static -c $(CFLAGS) $< -o $@

lib/libndview.so: src/ndview.c inc/ndview.h
	$(CC) -shared $(CFLAGS) $< -o $@

lib/libndview.a: src/ndview.c inc/ndview.h
	$(CC) -static -c $(CFLAGS) $< -o $@

test: test.c static_libs dynamic_libs
	$(CC) -static $(CFLAGS) $< $(LDFLAGS) -o $@

run_test: test
	./$<

clean:
	rm -f $(STATIC_LIBS) $(DYNAMIC_LIBS)
	rm -f test
