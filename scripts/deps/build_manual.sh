#!/bin/bash
# 手工交叉编译 lycium 缺失的小库（libsodium、enet），静态安装到 lycium usr 目录
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
GN="$LYCIUM/Buildtools/gnuconfig"

# ---------- libsodium (autotools, static) ----------
cd "$MAN/libsodium-stable"
cp "$GN/config.sub" build-aux/config.sub
cp "$GN/config.guess" build-aux/config.guess
make distclean >/dev/null 2>&1 || true
./configure --host=aarch64-linux-ohos \
  --prefix="$LYCIUM/usr/libsodium/arm64-v8a" \
  --enable-static --disable-shared
make -j8
make install
echo "LIBSODIUM DONE"

# ---------- enet (cmake, static) ----------
cd "$MAN/enet-1.3.18"
rm -rf ohos-build
mkdir ohos-build
cd ohos-build
cmake .. -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
  $OHOS_CMAKE_TOOLCHAIN_FILE \
  -DOHOS_ARCH=arm64-v8a -DOHOS_PLATFORM=OHOS \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX="$LYCIUM/usr/enet/arm64-v8a"
ninja
ninja install
echo "ENET DONE"
