# 上游基线 / Upstream anchor

本仓库只包含 0 A.D. 的 **HarmonyOS 适配层**（鸿蒙壳工程 + 引擎补丁集）。
上游游戏引擎源码**不在此仓库内**，通过 `patches/` 应用。

| 项目 | 值 |
|---|---|
| 上游官方仓库 | https://gitea.wildfiregames.com/0ad/0ad （0 A.D. 官方开发已迁至 Gitea） |
| 公开镜像 | https://repo.or.cz/0ad.mirror.git |
| ⚠️ 已停更镜像 | https://github.com/0ad/0ad —— 最新 tag 仅到 `A26`（2022），最后提交 2024-08-17，**不能用于同步** |
| 版本 | 0 A.D. **0.28「Boiorix」** |
| 上游 tag | `v0.28.0` |
| 上游基线提交 | `a2cae4d69f81`（2026-02-15，"Update appdata for next RC"） |
| 上游 master 现状 | 最新提交 **2026-10-21**（`46a5570281` "Fix spidermonkey build rust target detection"）——**基线之后上游仍在活跃开发约 8 个月** |
| 基线产物 | `0ad-0.28.0-unix-build.tar.xz`（官方发布包，**当前最可靠锚点**） |
| 基线 sha256 | `27e217755ef76a922fe58dbf593d96e54b6ed2375d23f548c35619aa6bd5a42a` |
| 补丁集 | `patches/0001-ohos-engine-0ad-0.28.0.patch` |
| 涉及文件 | **61 个**，全部位于 `source/` 下 |
| 改动规模 | +1460 / −532 行 |
| 记录时间 | 2026-10-28 |

## 同步状态

**当前不与上游同步。** 引擎是一份 0.28 的冻结快照，加上本文档记录的补丁；
仓库没有 `upstream` remote，也没有 merge/rebase 机制。

因此：

- **基线落后上游约 8 个月**：快照取自 `v0.28.0`（2026-02-15），而上游 master
  已经推进到 2026-10-21
- 上游 0.28 之后的 bug 修复、安全修复、平台改动**不会自动进入本移植**
- 升级到新的上游版本时，需要把 61 个补丁**重新 rebase** 到新源码上；
  冲突预计集中在 `source/ps`（20 个文件，启动流程/GameSetup/HWDetect）
- 判断"快照有没有偏离上游"最快的依据是 `v0.28.0` tag（`a2cae4d69f81`）；
  连不上 Gitea 时退回用基线包的 sha256 校验

> 核对方式：GitHub 的 `0ad/0ad` 已停更（最新 tag `A26`/2022，最后提交 2024-08），
> 不能用。Gitea 在本机需经 Clash 代理访问（`gitea.wildfiregames.com` 会解析到
> Clash 的 fake-IP 段 `198.18.x.x`，直连会被判定为非公网地址）。
> 本次核对经 `http://127.0.0.1:7897` 代理访问 Gitea REST API 完成；
> 其 `compare` 接口返回 404，故未能取得精确提交增量。

若将来要接入上游，建议：在 `<workspace>/0ad-0.28.0` 初始化 git 并
`git remote add upstream https://gitea.wildfiregames.com/0ad/0ad`，
检出 `v0.28.0` 作为基线提交，再定期 `git merge upstream/master`。

## 基线核对（2026-10-28）

用 `scripts/sync-upstream.sh` 拉取上游 `v0.28.0` 的目录树，与本地引擎做 **blob 哈希比对**
（`--filter=blob:none` 只取目录树，不下载任何文件内容）：

| 分类 | 文件数 | 说明 |
|---|---|---|
| 内容完全一致 | 1605 | — |
| **补丁覆盖的差异** | **61** | 与 `patches/0001-ohos-engine-0ad-0.28.0.patch` 的文件数完全吻合 |
| 补丁之外的差异 | 9 | 全是 `source/tools/templatesanalyzer/…/themes/*.png`，官方 tarball 与 git tag 的素材差异，与移植无关 |
| 仅上游有 | 8 | Windows 专用 `.exe/.bat/.dll`，Unix 发布包不含 |
| 仅本地有 | 1 | `source/tools/spirv/rules.json` |

**结论：本地引擎 == 上游 `v0.28.0` + 本仓库补丁。** 锚点是可靠的。

## 网络与协议要点

- `gitea.wildfiregames.com` 在本机解析到 Clash 的 fake-IP 段（`198.18.x.x`），
  直连会被判定为非公网地址，需要走代理
- 该 Gitea 的 smart-http 在 **protocol v2 下会中途断开**（`RPC failed / curl 18`），
  必须显式 `-c protocol.version=0`
- 全库约 **15 GB**；即使 `--depth 1` 也要拉数 GB 素材。
  做法是用 `--filter=blob:none --no-checkout` 只取提交与目录树（约 1 MB），
  再与本地文件做哈希比对 —— 全程不需要下载文件内容

```bash
./scripts/sync-upstream.sh           # 校验本地引擎 == 上游 tag + 补丁
./scripts/sync-upstream.sh --fetch   # 重新拉取上游目录树
REF=a27.1 ./scripts/sync-upstream.sh # 核对其他上游版本
```

## 应用补丁

```bash
# 1. 解压官方 0.28 源码
tar -xJf 0ad-0.28.0-unix-build.tar.xz
cd 0ad-0.28.0

# 2. 应用鸿蒙补丁（或直接跑 patches/apply.sh）
patch -p1 < ../0ad-harmonyos/patches/0001-ohos-engine-0ad-0.28.0.patch
```

## 改动分类

| 目录 | 文件数 | 内容 |
|---|---|---|
| `source/ps` | 20 | 启动流程、GameSetup、HWDetect |
| `source/renderer` | 13 | GLES / Vulkan 后端适配 |
| `source/gui` | 8 | 触控输入、下拉框、输入框 |
| `source/lib` | 7 | SDL、OGL、sysdep |
| `source/graphics` | 5 | 相机、字体、纹理 |
| 其他 | 8 | network / lobby / soundmanager / main.cpp |

## 重新生成补丁

```bash
# 在原始源码的父目录下
diff -ruN --text <原始>/source <修改后>/source > 新补丁.patch
# 或使用 a/ b/ 前缀以便 patch -p1 应用
```

## 签名材料

本仓库**不保存签名材料**。`build-profile.json5` 中的 `signingConfigs` 在版本库内为空，
本地签名请由 DevEco Studio 自动生成，**不要提交**。

## 维护脚本

| 脚本 | 用途 |
|---|---|
| `patches/apply.sh` | 把补丁应用到指定 `0ad-0.28.0` 目录（先 dry-run 再应用） |
| `patches/verify.sh` | 校验当前引擎树是否仍等于「原始 + 补丁」，发现漂移即列出文件并以非 0 退出 |
| `patches/regen.sh` | 有意改动引擎后重新生成补丁，并打印新的文件数、改动规模与基线 sha256 |

工作区根目录通过 `ZEROAD_ROOT` 指定；未设置时自动按脚本位置推断，
原始源码默认从 `0ad-0.28.0-unix-build.tar.xz` 解压。
