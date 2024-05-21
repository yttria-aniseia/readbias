CFLAGS = -O3 -flto -mtune=native
INCLUDES = -I htslib/ -I jemalloc/include htslib/libhts.a jemalloc/lib/libjemalloc.a libdeflate/build/libdeflate.a -lcrypto -lm -lpthread -lcurl -llzma -lz -lbz2

LIBDEFLATE_OPTS = -DLIBDEFLATE_BUILD_STATIC_LIB=ON -DLIBDEFLATE_BUILD_SHARED_LIB=OFF -DLIBDEFLATE_BUILD_GZIP=OFF

append_cb: append_cb.c htslib/libhts.a libdeflate/build/libdeflate.a jemalloc/lib/libjemalloc.a
	gcc append_cb.c -o $@ ${CFLAGS} ${INCLUDES}


jemalloc/lib/libjemalloc.a:
	cd jemalloc && ./autogen.sh --enable-prof && make build_lib_static

libdeflate/build/libdeflate.a:
	cd libdeflate && cmake -B build ${LIBDEFLATE_OPTS} && cmake --build build

htslib/htscodecs/:
	git submodule update --init --recursive

htslib/Makefile: libdeflate/build/libdeflate.a
	cd htslib && autoreconf -i
	cd htslib && CFLAGS="-flto -I ../libdeflate/" LDFLAGS="-L ../libdeflate/build/" ./configure --with-libdeflate

htslib/htslib.a: htslib/htscodecs/ htslib/Makefile
	make -C htslib lib-static

clean:
	git submodule deinit -f .
	git submodule update --init --recursive
