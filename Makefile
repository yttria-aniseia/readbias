CFLAGS = -O3 -flto -mtune=native
INCLUDES = -I htslib/htslib/ htslib/libhts.a libdeflate/build/libdeflate.a -lcrypto -lm -lpthread -lcurl -llzma -lz -lbz2

LIBDEFLATE_OPTS = -DLIBDEFLATE_BUILD_STATIC_LIB=ON -DLIBDEFLATE_BUILD_SHARED_LIB=OFF -DLIBDEFLATE_BUILD_GZIP=OFF

append_cb: htslib.a libdeflate.a
	gcc append_cb.c -o $@ ${CFLAGS} ${INCLUDES}


libdeflate.a:
	cd libdeflate && cmake -B build ${LIBDEFLATE_OPTS} && cmake --build build

htslib/htscodecs/:
	git submodule update --init --recursive

htslib/Makefile: libdeflate/build/libdeflate.a
	cd htslib && autoreconf -i
	cd htslib && CFLAGS="-flto" LDFLAGS="-I ../libdeflate/lib/ ../libdeflate/build/libdeflate.a" ./configure --with-libdeflate

htslib.a: htslib/htscodecs/ htslib/Makefile
	make -C htslib

clean:
	git submodule deinit -f .
	git submodule update --init --recursive
