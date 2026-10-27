#!/bin/bash
# 交叉编译 miniupnpc(静态) 与 gloox(OpenSSL, 静态)，安装到 lycium usr
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
OPENSSL="$LYCIUM/usr/openssl/arm64-v8a"

# ---------- miniupnpc (static) ----------
cd "$MAN/miniupnpc-2.2.7"
rm -rf ohos-build
mkdir ohos-build
cd ohos-build
cmake .. -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
  $OHOS_CMAKE_TOOLCHAIN_FILE \
  -DOHOS_ARCH=arm64-v8a -DOHOS_PLATFORM=OHOS -DCMAKE_BUILD_TYPE=Release \
  -DUPNPC_BUILD_STATIC=ON -DUPNPC_BUILD_SHARED=OFF \
  -DUPNPC_BUILD_TESTS=OFF -DUPNPC_BUILD_SAMPLE=OFF \
  -DCMAKE_INSTALL_PREFIX="$LYCIUM/usr/miniupnpc/arm64-v8a"
ninja
ninja install
echo "MINIUPNPC DONE"

# ---------- gloox (OpenSSL, static) ----------
cd "$MAN/gloox-1.0.28"
cp "$LYCIUM/Buildtools/gnuconfig/config.sub" config.sub
cp "$LYCIUM/Buildtools/gnuconfig/config.guess" config.guess
make distclean >/dev/null 2>&1 || true
export PKG_CONFIG_PATH="$OPENSSL/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
export CPPFLAGS="${CPPFLAGS} -I$OPENSSL/include"
export LDFLAGS="${LDFLAGS} -L$OPENSSL/lib"
export CFLAGS="${CFLAGS} -D_GNU_SOURCE"
export CXXFLAGS="${CXXFLAGS} -D_GNU_SOURCE"
./configure --host=aarch64-linux-ohos \
  --prefix="$LYCIUM/usr/gloox/arm64-v8a" \
  --enable-static --disable-shared \
  --with-openssl --without-gnutls --without-libidn \
  --without-zlib \
  --without-tests --without-examples
make -j8
make install
echo "GLOOX DONE"
