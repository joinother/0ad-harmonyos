#!/bin/bash
# 交叉编译 openal-soft(SDL3 后端) 与 curl(openssl+zlib)，静态安装到 lycium usr
# 工作区根目录：默认假定本脚本位于 <workspace>/0ad-harmonyos/scripts/deps/
# 可用 ZEROAD_ROOT 覆盖。工作区需包含: 0ad-0.28.0/ lycium/ SDL-src/ manual-deps/ bundled-ohos/
set -e
export PATH=/opt/homebrew/bin:$PATH
WS="${ZEROAD_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
LYCIUM="$WS/lycium/lycium"
export OHOS_SDK="${OHOS_SDK:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony}"
[ -d "$LYCIUM" ] || { echo "找不到 lycium: $LYCIUM（请设置 ZEROAD_ROOT）" >&2; exit 1; }
# shellcheck disable=SC1091
. "$LYCIUM/script/envset.sh"
setarm64ENV
export OHOS_CMAKE_TOOLCHAIN_FILE="-DCMAKE_TOOLCHAIN_FILE=${OHOS_SDK}/native/build/cmake/ohos.toolchain.cmake"
NINJA=$(which ninja)
MAN="$WS/manual-deps"
SDL3DIR="$MAN/sdl3-prefix/lib/cmake/SDL3"

# ---------- openal-soft (SDL3, static) ----------
cd "$MAN/openal-soft-1.24.3"
rm -rf ohos-build
mkdir ohos-build
cd ohos-build
cmake .. -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
  $OHOS_CMAKE_TOOLCHAIN_FILE \
  -DOHOS_ARCH=arm64-v8a -DOHOS_PLATFORM=OHOS -DCMAKE_BUILD_TYPE=Release \
  -DLIBTYPE=STATIC \
  -DSDL3_DIR="$SDL3DIR" \
  -DALSOFT_BACKEND_SDL3=ON -DALSOFT_REQUIRE_SDL3=ON \
  -DALSOFT_BACKEND_PIPEWIRE=OFF -DALSOFT_BACKEND_PULSEAUDIO=OFF \
  -DALSOFT_BACKEND_ALSA=OFF -DALSOFT_BACKEND_JACK=OFF \
  -DALSOFT_BACKEND_OBOE=OFF -DALSOFT_BACKEND_OPENSL=OFF \
  -DALSOFT_BACKEND_SNDIO=OFF -DALSOFT_BACKEND_PORTAUDIO=OFF \
  -DALSOFT_BACKEND_COREAUDIO=OFF -DALSOFT_BACKEND_WASAPI=OFF \
  -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$LYCIUM/usr/openal/arm64-v8a"
ninja
ninja install
echo "OPENAL DONE"

# ---------- curl (openssl+zlib, static) ----------
cd "$MAN/curl-8.7.1"
cp "$LYCIUM/Buildtools/gnuconfig/config.sub" config.sub 2>/dev/null || true
cp "$LYCIUM/Buildtools/gnuconfig/config.guess" config.guess 2>/dev/null || true
make distclean >/dev/null 2>&1 || true
export PKG_CONFIG_PATH="$LYCIUM/usr/openssl/arm64-v8a/lib/pkgconfig:$LYCIUM/usr/zlib/arm64-v8a/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
export CFLAGS="${CFLAGS} -D_GNU_SOURCE"
./configure --host=aarch64-linux-ohos \
  --prefix="$LYCIUM/usr/libcurl/arm64-v8a" \
  --enable-static --disable-shared \
  --with-openssl --with-zlib \
  --without-brotli --without-zstd --without-libidn2 --without-libpsl \
  --without-nghttp2 --without-nghttp3 --without-libgsasl \
  --disable-ldap --disable-ldaps
make -j8
make install
echo "CURL DONE"
