#!/bin/bash
# 从「原始源码」和「当前源码」重新生成补丁，并打印需要更新的 sha256。
#
# 用法: ./regen.sh [--pristine <原始source目录>] [当前source目录]
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
  mkdir -p "$TMP/a"
  cp -a "$PRIS" "$TMP/a/source"
elif [ -f "$TARBALL" ]; then
  mkdir -p "$TMP/a"
  tar -xJf "$TARBALL" -C "$TMP/a" --strip-components=1 '0ad-0.28.0/source'
else
  echo "找不到原始源码：请提供 --pristine，或把 0ad-0.28.0-unix-build.tar.xz 放到 $WS" >&2
  exit 1
fi
mkdir -p "$TMP/b"
cp -a "$CUR" "$TMP/b/source"

( cd "$TMP" && diff -ruN --text a b > "$PATCH.new" ) || true
if [ ! -s "$PATCH.new" ]; then
  echo "⚠️ 没有差异，未写入补丁"; exit 0
fi
mv "$PATCH.new" "$PATCH"

FILES=$(grep -c '^--- a/' "$PATCH" || true)
STAT=$(awk '/^\+[^+]/{a++} /^-[^-]/{d++} END{printf "+%d / -%d", a, d}' "$PATCH")
echo "✅ 已更新补丁"
echo "   文件数: $FILES   改动: $STAT   大小: $(du -h "$PATCH" | cut -f1)"
if [ -f "$TARBALL" ]; then
  echo "   基线 sha256: $(shasum -a 256 "$TARBALL" | awk '{print $1}')"
fi
echo
echo "别忘了同步 UPSTREAM.md 里的文件数/改动规模/记录时间。"
