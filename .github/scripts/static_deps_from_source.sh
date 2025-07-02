#!/bin/bash

set -e
# set -x

# This script downloads and builds dependencies that were to big or annoying to integrate with meson.
# Dependencies are built as static libraries.


start_path=`pwd`
install_prefix=$start_path/root
export PKG_CONFIG_PATH="$install_prefix/lib/pkgconfig:$install_prefix/lib64/pkgconfig"



function get_and_enter() {
    link=$1
    shift
    directory=$1
    shift

    if [ ! -d $directory ]; then
        echo $link
        curl -LsS --retry 5 --retry-connrefused --max-time 600 $link | tar -xz
    fi
    cd $directory
}


function cmake_build() {
    name=$1
    shift
    extra_config_flags=$@

    echo Building $name...

    global_flags="
        -G Ninja
        -DCMAKE_INSTALL_PREFIX=$install_prefix
        -DCMAKE_PREFIX_PATH=$install_prefix
        -DCMAKE_BUILD_TYPE=Release
        "

    cmake -S . -B ./build $global_flags $extra_config_flags
    cmake -S . -B ./build $global_flags $extra_config_flags -LAH > ../${name}_opts.txt
    cmake --build ./build --config Release
    cmake --install ./build --config Release
    cd ..
}

function autotools_build() {
    name=$1
    shift
    extra_config_flags=$@

    echo Building $name...

    ./configure --prefix=$install_prefix $extra_config_flags
    ./configure --prefix=$install_prefix --help > ../${name}_opts.txt
    make -j -l90 install
    cd ..
}


wolfssl_version=5.8.0
nghttp2_version=1.66.0
ngtcp2_version=1.13.0
nghttp3_version=1.10.1
zlib_version=1.3.1
zstd_version=1.5.7
brotli_version=1.1.0

curl_version=8.14.1


if [ ! -d "temp" ]; then
    mkdir temp
fi
cd temp

get_and_enter https://github.com/wolfSSL/wolfssl/archive/v$wolfssl_version-stable.tar.gz wolfssl-$wolfssl_version-stable
./autogen.sh
autotools_build wolfssl \
    --enable-static \
    --disable-shared \
    --enable-curl \
    --disable-examples \
    --disable-crypttests \
    --enable-quic \

get_and_enter https://github.com/nghttp2/nghttp2/releases/download/v$nghttp2_version/nghttp2-$nghttp2_version.tar.gz nghttp2-$nghttp2_version
autotools_build nghttp2 \
    --disable-shared \
    --enable-static \
    --disable-failmalloc \
    --enable-lib-only \
    --without-libxml2 \
    --without-jansson \
    --without-zlib \
    --without-libevent-openssl \
    --without-libcares \
    --without-wolfssl \
    --without-openssl \
    --without-libev \
    --without-jemalloc \
    --without-systemd \
    --without-libngtcp2 \
    --without-libnghttp3 \

get_and_enter https://github.com/ngtcp2/ngtcp2/releases/download/v$ngtcp2_version/ngtcp2-$ngtcp2_version.tar.gz ngtcp2-$ngtcp2_version
cmake_build ngtcp2 \
    -DENABLE_STATIC_LIB=ON \
    -DENABLE_SHARED_LIB=OFF \
    -DENABLE_LIB_ONLY=ON \
    -DENABLE_OPENSSL=OFF \
    -DENABLE_WOLFSSL=ON \
    -DBUILD_TESTING=OFF \

get_and_enter https://github.com/ngtcp2/nghttp3/releases/download/v$nghttp3_version/nghttp3-$nghttp3_version.tar.gz nghttp3-$nghttp3_version
cmake_build nghttp3 \
    -DENABLE_LIB_ONLY=ON \
    -DENABLE_STATIC_LIB=ON \
    -DENABLE_SHARED_LIB=OFF \
    -DBUILD_TESTING=OFF \

get_and_enter https://zlib.net/zlib-$zlib_version.tar.gz zlib-$zlib_version
autotools_build zlib \
    --static \

get_and_enter https://github.com/facebook/zstd/releases/download/v$zstd_version/zstd-$zstd_version.tar.gz zstd-$zstd_version
cd build/meson/
meson setup bin -Ddefault_library=static -Dbin_programs=false --prefix=$install_prefix
meson install -C bin
cd ../../../

get_and_enter https://github.com/google/brotli/archive/v$brotli_version.tar.gz brotli-$brotli_version
cmake_build brotli \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \

get_and_enter https://curl.se/download/curl-$curl_version.tar.gz curl-$curl_version
cmake_build curl \
    -DBUILD_CURL_EXE=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_LIBCURL_DOCS=OFF \
    -DBUILD_MISC_DOCS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_CURL=ON \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_TESTING=OFF \
    \
    -DCURL_DISABLE_HTTP_AUTH=ON \
    -DCURL_USE_LIBPSL=OFF \
    -DCURL_USE_LIBSSH2=OFF \
    -DCURL_USE_OPENSSL=OFF \
    -DCURL_USE_WOLFSSL=ON \
    -DHTTP_ONLY=ON \
    -DUSE_LIBIDN2=OFF \
    -DUSE_NGTCP2=ON \

cd $start_path

echo To use libraries built from source:
echo "meson setup bin -Dpkg_config_path=$install_prefix/lib/pkgconfig,$install_prefix/lib64/pkgconfig"
