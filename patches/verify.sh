#!/bin/bash
# 校验当前引擎源码树是否仍等于「原始 0.28 源码 + patches/0001」。
# 用于发现"改了引擎但忘记重新生成补丁"的漂移。
#
# 用法: ./verify.sh [--pristine <原始source目录>] [当前source目录]
# 默认原始源码来自 $ZEROAD_ROOT/0ad-0.28.0-unix-build.tar.xz
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH="$HERE/0001-ohos-engine-0ad-0.28.0.patch"
WS="${ZEROAD_ROOT:-$(cd "$HERE/../.." && pwd)}"
CUR="$WS/0ad-0.28.0/source"
PRIS=""
TARBALL="$WS/0ad-0.28.0-unix-build.tar.xz"

while [ $# -gt 0 ]; do
  case "$1" in
    --pristine) PRIS="$2"; shift 2 ;;
    *) CUR="$1"; shift ;;
  esac
done

[ -d "$CUR" ] || { echo "找不到当前源码目录: $CUR" >&2; exit 1; }
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -n "$PRIS" ]; then
  mkdir -p "$TMP/0ad-0.28.0"
  cp -a "$PRIS" "$TMP/0ad-0.28.0/source"
elif [ -f "$TARBALL" ]; then
  echo "· 从官方包解压原始源码: $(basename "$TARBALL")"
  tar -xJf "$TARBALL" -C "$TMP" '0ad-0.28.0/source'
else
  echo "找不到原始源码：请提供 --pristine，或把 0ad-0.28.0-unix-build.tar.xz 放到 $WS" >&2
  exit 1
fi

echo "· 应用补丁到临时副本"
( cd "$TMP/0ad-0.28.0" && patch -p1 --quiet < "$PATCH" )

echo "· 比对当前源码树"
if diff -rq "$TMP/0ad-0.28.0/source" "$CUR" > "$TMP/drift.txt" 2>&1; then
  echo "✅ 无漂移：当前源码 == 原始 + 补丁"
else
  echo "⚠️ 检测到漂移（以下文件与补丁不一致）："
  sed "s|$TMP/0ad-0.28.0/source|<pristine+patch>|; s|$CUR|<current>|" "$TMP/drift.txt" | head -40 | sed 's/^/    /'
  echo
  echo "如果这些改动是有意的，运行 ./regen.sh 重新生成补丁。"
  exit 1
fi
