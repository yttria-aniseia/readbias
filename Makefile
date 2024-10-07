MAKEFLAGS += --no-builtin-rules
CFLAGS = -O3 -flto -mtune=native -g
INCLUDES = -I htslib/ htslib/libhts.a libdeflate/build/libdeflate.a -lcrypto -lm -lpthread -lcurl -llzma -lz -lbz2
export CC=gcc
export ARFLAGS:=--plugin=$(shell gcc --print-file-name=liblto_plugin.so)
export NM=gcc-nm
export RANLIB=gcc-ranlib
LIBDEFLATE_OPTS = -DLIBDEFLATE_BUILD_STATIC_LIB=ON -DLIBDEFLATE_BUILD_SHARED_LIB=OFF -DLIBDEFLATE_BUILD_GZIP=OFF

readbias: readbias.c htslib/libhts.a libdeflate/build/libdeflate.a
	gcc readbias.c -o $@ ${CFLAGS} ${INCLUDES}


libdeflate/build/libdeflate.a:
	cd libdeflate && CFLAGS="-fPIC -O2" LDFLAGS="-flto" cmake -B build ${LIBDEFLATE_OPTS} && cmake --build build

htslib/htscodecs/:
	git submodule update --init --recursive

htslib/Makefile: libdeflate/build/libdeflate.a
	cd htslib && autoreconf -i
	cd htslib && CFLAGS="-fPIC -O2 -I ../libdeflate/" LDFLAGS="-flto -L ../libdeflate/build/" ./configure --with-libdeflate

htslib/libhts.a: htslib/htscodecs/ htslib/Makefile
	make -C htslib lib-static

clean:
	git submodule deinit -f .
	git submodule update --init --recursive
