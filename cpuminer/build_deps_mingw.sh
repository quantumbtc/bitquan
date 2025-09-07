#!/usr/bin/env bash
set -e

# ==============================
# 配置
# ==============================
TARGET=x86_64-w64-mingw32
PREFIX=/usr/$TARGET
CURL_VERSION=8.7.1
JSON_VERSION=v3.11.3

mkdir -p deps && cd deps

# ==============================
# 下载并编译 libcurl (静态库)
# ==============================
if [ ! -d curl-$CURL_VERSION ]; then
    wget https://curl.se/download/curl-$CURL_VERSION.tar.gz
    tar xf curl-$CURL_VERSION.tar.gz
fi

cd curl-$CURL_VERSION
echo "=== Building curl for $TARGET ==="
./configure \
    --host=$TARGET \
    --prefix=$PREFIX \
    --disable-shared \
    --enable-static \
    --with-winssl \
    --without-gnutls \
    --without-libssh2 \
    --without-nghttp2 \
    --without-zstd \
    --without-brotli

make -j$(nproc)
sudo make install
cd ..

# ==============================
# 安装 nlohmann_json (header-only)
# ==============================
if [ ! -d nlohmann_json ]; then
    git clone --depth 1 --branch $JSON_VERSION https://github.com/nlohmann/json.git nlohmann_json
fi

echo "=== Installing nlohmann_json headers ==="
sudo mkdir -p $PREFIX/include/nlohmann
sudo cp -r nlohmann_json/single_include/nlohmann/* $PREFIX/include/nlohmann/

# ==============================
# 完成
# ==============================
echo "✅ 依赖安装完成，已安装到 $PREFIX"
echo "   - libcurl: $PREFIX/lib/libcurl.a"
echo "   - nlohmann_json: $PREFIX/include/nlohmann/json.hpp"
