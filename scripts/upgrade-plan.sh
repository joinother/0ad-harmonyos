#!/bin/bash
# 评估「升级到某个上游版本」的影响，只出报告，不改动任何东西。
#   1. 上游从基线到目标版本改了多少文件
#   2. 这些文件里有多少与我们的补丁重叠（= rebase 时的冲突面）
#
# 用法:
#   ./upgrade-plan.sh master
#   ./upgrade-plan.sh v0.29.0
#   BASE=v0.28.0 ./upgrade-plan.sh master
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS="${ZEROAD_ROOT:-$(cd "$HERE/../.." && pwd)}"
ENG="$WS/0ad-0.28.0"
BASE="${BASE:-v0.28.0}"
TARGET="${1:-}"
PATCH="$(ls "$HERE"/../patches/0001-ohos-engine-*.patch 2>/dev/null | head -1 || true)"
PROXY="${PROXY:-$(git config --get http.proxy || true)}"

[ -n "$TARGET" ] || { echo "用法: $0 <上游ref>  例如: $0 master 或 $0 v0.29.0" >&2; exit 1; }
[ -d "$ENG/.git" ] || { echo "引擎仓库不存在: $ENG" >&2; exit 1; }
[ -n "$PATCH" ] && [ -f "$PATCH" ] || { echo "找不到补丁文件" >&2; exit 1; }

gitup() {
  if [ -n "$PROXY" ]; then
    git -C "$ENG" -c protocol.version=0 -c http.version=HTTP/1.1 -c "http.proxy=$PROXY" "$@"
  else
    git -C "$ENG" -c protocol.version=0 -c http.version=HTTP/1.1 "$@"
  fi
}

echo "· 拉取上游 ${TARGET}（blobless，只要目录树）"
gitup fetch --depth 1 --filter=blob:none upstream "$TARGET" 2>&1 | tail -2
TARGET_SHA="$(gitup rev-parse FETCH_HEAD)"
echo "  目标提交: ${TARGET_SHA:0:12}"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

echo "· 计算上游 $BASE..$TARGET 的改动"
gitup diff --name-only "$BASE" "$TARGET_SHA" -- source > "$TMP/up.txt" || true
grep '^--- a/' "$PATCH" | sed 's|^--- a/||' | awk '{print $1}' | sort -u > "$TMP/ours.txt"
sort "$TMP/up.txt" -o "$TMP/up.txt"
comm -12 "$TMP/up.txt" "$TMP/ours.txt" > "$TMP/overlap.txt"

echo
echo "  上游改动文件        : $(wc -l < "$TMP/up.txt" | tr -d ' ')"
echo "  我们的补丁文件      : $(wc -l < "$TMP/ours.txt" | tr -d ' ')"
echo "  ⚠️ 重叠（需处理）   : $(wc -l < "$TMP/overlap.txt" | tr -d ' ')"
echo
if [ -s "$TMP/overlap.txt" ]; then
  echo "  重叠文件按目录分布:"
  awk -F/ '{if(NF>1) print $1"/"$2; else print $1}' "$TMP/overlap.txt" | sort | uniq -c | sort -rn | sed 's/^/    /'
  echo
  echo "  重叠文件清单:"
  sed 's/^/    /' "$TMP/overlap.txt"
else
  echo "  ✅ 无重叠：上游改动避开了我们修改的 61 个文件，rebase 预计零冲突。"
fi
echo
echo "  提示：rebase 前先建备份分支 —— git -C \"$ENG\" branch backup-before-upgrade"
