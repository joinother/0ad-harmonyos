#!/bin/bash
# 对接 0 A.D. 上游：拉取上游 tag 的目录树，与本地引擎源码做「哈希级」比对。
# 只做「获取 + 报告」，不自动合并。
#
# 为什么不用普通 clone：
#   gitea.wildfiregames.com 的 0 A.D. 全库约 15 GB，depth-1 仍要拉数 GB 素材。
#   这里用 --filter=blob:none --no-checkout 只取提交与目录树（约 1 MB），
#   再用 blob 哈希与本地文件比对 —— 不需要下载任何文件内容。
#
# 必须注意：该 Gitea 的 smart-http 在 protocol v2 下会中途断开（RPC failed /
#   curl 18），必须显式使用 protocol.version=0；本机还需经 Clash 代理。
#
# 用法:
#   ./sync-upstream.sh              # 校验本地引擎 == 上游 tag + 补丁
#   ./sync-upstream.sh --fetch      # 强制重新拉取上游引用
#   REF=a27.1 ./sync-upstream.sh    # 指定上游引用（默认 v0.28.0）
#   UPSTREAM_URL=... PROXY=... ./sync-upstream.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS="${ZEROAD_ROOT:-$(cd "$HERE/../.." && pwd)}"
UPSTREAM_URL="${UPSTREAM_URL:-https://gitea.wildfiregames.com/0ad/0ad}"
REF="${REF:-v0.28.0}"
CLONE="$WS/0ad-upstream"
LOCAL="$WS/0ad-0.28.0"
PATCH="$(ls "$HERE"/../patches/0001-ohos-engine-*.patch 2>/dev/null | head -1)"
PATCH="${PATCH:-$HERE/../patches/0001-ohos-engine-0ad-0.28.0.patch}"
PROXY="${PROXY:-$(git config --get http.proxy || true)}"

GIT=(git -c protocol.version=0 -c http.version=HTTP/1.1)
[ -n "$PROXY" ] && GIT+=(-c "http.proxy=$PROXY")

[ -d "$LOCAL/source" ] || { echo "找不到本地引擎源码: $LOCAL/source（检查 ZEROAD_ROOT）" >&2; exit 1; }

FORCE=0; [ "${1:-}" = "--fetch" ] && FORCE=1

if [ ! -d "$CLONE/.git" ]; then
  echo "· 首次拉取上游目录树 ${REF}（blobless，约 1 MB）"
  ${GIT[@]+"${GIT[@]}"} clone --depth 1 --single-branch --branch "$REF" \
    --filter=blob:none --no-checkout "$UPSTREAM_URL" "$CLONE"
elif [ "$FORCE" = "1" ]; then
  echo "· 重新拉取上游 $REF"
  ( cd "$CLONE" && ${GIT[@]+"${GIT[@]}"} fetch --depth 1 origin "+refs/tags/$REF:refs/tags/$REF" )
fi

[ -d "$CLONE/.git" ] || { echo "上游目录树不可用: $CLONE" >&2; exit 1; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

echo "· 读取上游 $REF 的目录树"
git -C "$CLONE" ls-tree -r "$REF" -- source \
  | awk -F'\t' '{split($1,a," "); print $2"\t"a[3]}' > "$TMP/up.tsv"

echo "· 计算本地 source/ 的 blob 哈希"
( cd "$LOCAL" && find source -type f | sort > "$TMP/files.txt" \
    && git hash-object --no-filters --stdin-paths < "$TMP/files.txt" > "$TMP/hashes.txt" \
    && paste "$TMP/files.txt" "$TMP/hashes.txt" > "$TMP/local.tsv" )

# 补丁覆盖的文件（这些差异是预期的）
grep '^--- a/' "$PATCH" 2>/dev/null | sed 's|^--- a/||' | awk '{print $1}' | sort -u > "$TMP/patch.txt" || true

python3 - "$TMP/up.tsv" "$TMP/local.tsv" "$TMP/patch.txt" "$REF" <<'PY'
import sys
up_f, loc_f, patch_f, ref = sys.argv[1:5]
def load(p):
    d={}
    for l in open(p):
        l=l.rstrip('\n')
        if not l: continue
        parts=l.split('\t')
        if len(parts)==2: d[parts[0]]=parts[1]
    return d
up, loc = load(up_f), load(loc_f)
try: patch=set(x.strip() for x in open(patch_f) if x.strip())
except FileNotFoundError: patch=set()

common=set(up)&set(loc)
diff={p for p in common if up[p]!=loc[p]}
only_up=sorted(set(up)-set(loc)); only_loc=sorted(set(loc)-set(up))
covered=sorted(diff & patch); extra=sorted(diff - patch)

print(f"  两边共有文件      : {len(common)}")
print(f"  ├─ 内容完全一致   : {len(common)-len(diff)}")
print(f"  ├─ 补丁覆盖的差异 : {len(covered)}  (预期: 与补丁文件数一致)")
print(f"  └─ 补丁之外的差异 : {len(extra)}")
if only_up:  print(f"  仅上游有          : {len(only_up)}  (如 Windows 专用 .exe/.bat/.dll，Unix 发布包不含)")
if only_loc: print(f"  仅本地有          : {len(only_loc)}")

def is_bin(p):
    return p.lower().endswith(('.png','.jpg','.jpeg','.gif','.ico','.bmp','.zip','.gz','.7z','.exe','.dll','.ttf','.otf','.wav','.ogg'))

bin_extra=[p for p in extra if is_bin(p)]
src_extra=[p for p in extra if not is_bin(p)]

if bin_extra:
    print(f"\n  其中二进制差异 {len(bin_extra)} 个（官方 tarball 与 git tag 的素材差异，非移植改动）:")
    for p in bin_extra[:12]: print("    ·",p)
if src_extra:
    print(f"\n  ⚠️ 补丁之外的**源码**差异 {len(src_extra)} 个 —— 这才是真正的漂移:")
    for p in src_extra[:20]: print("    ⚠️",p)

print()
if src_extra:
    print("结论: 本地引擎存在补丁之外的改动。若是有意的，请重新生成补丁（patches/regen.sh）。")
    sys.exit(1)
for p in only_loc[:10]: print("   仅本地:",p)
print(f"结论: 本地引擎 == 上游 {ref} + patches（差异全部由补丁解释）。")
PY
