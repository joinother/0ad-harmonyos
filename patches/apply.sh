#!/bin/bash
# 把鸿蒙引擎补丁应用到 0 A.D. 0.28 源码树
# 用法: ./apply.sh <0ad-0.28.0 源码目录>
set -euo pipefail
TARGET="${1:-}"
[ -d "$TARGET/source" ] || { echo "用法: $0 <0ad-0.28.0 目录>"; exit 1; }
cd "$TARGET"
patch -p1 --dry-run < "$(dirname "$0")/0001-ohos-engine-0ad-0.28.0.patch"
echo "--- dry-run 通过，开始应用 ---"
patch -p1 < "$(dirname "$0")/0001-ohos-engine-0ad-0.28.0.patch"
echo "✅ 已应用"
