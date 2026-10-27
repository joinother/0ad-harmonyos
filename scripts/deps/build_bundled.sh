#!/bin/bash
# 交叉编译 fcollada + nvtt（HarmonyOS aarch64, 静态），处理 0AD 自带补丁
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
export PATH="$OHOS_SDK/native/llvm/bin:$PATH"
B="$WS/bundled-ohos"
SRCSRC="$WS/0ad-0.28.0/libraries/source"

# ================= fcollada =================
cd "$B/fcollada-28209"
rm -rf src/output lib/*.a
mkdir -p lib
export PKG_CONFIG_LIBDIR="$LYCIUM/usr/libxml2/arm64-v8a/lib/pkgconfig"
( cd src && make CXX="$CXX" CC="$CC" AR="$AR" OS_DEFINE="-DLINUX" PIC_FLAGS="-fPIC" output/libFColladaSR.a )
cp src/output/libFColladaSR.a lib/libFColladaSR.a
echo "FCOLLADA DONE"

# ================= nvtt =================
cd "$B/nvtt-28209"
# apply 0ad patches
for p in "$SRCSRC"/nvtt/patches/*.patch; do
  patch -p1 -N < "$p" >/dev/null 2>&1 || true
done
rm -rf src/build lib/*.a
mkdir -p lib src/build
cd src/build
cmake .. -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
  $OHOS_CMAKE_TOOLCHAIN_FILE \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DPNG=0 -DNVTT_SHARED=0 \
  -DBINDIR=bin -DLIBDIR=lib
ninja
# collect all static libs
cd ../..
find src/build -name "*.a" -exec cp {} lib/ \;
echo "NVTT DONE"
echo "=== nvtt libs ==="
ls lib/
