# 依赖库交叉编译脚本

这些脚本把 0 A.D. 需要的第三方库交叉编译成 **HarmonyOS arm64-v8a 静态库**，
安装到 `lycium/usr/<lib>/arm64-v8a`。

> 本目录是入库的**权威副本**（已参数化，不含任何个人绝对路径）。
> 工作区里 `manual-deps/`、`bundled-ohos/` 下的同名脚本是早期硬编码版本，以本目录为准。

## 工作区布局

脚本默认从自身位置推断工作区根目录（`ZEROAD_ROOT` 可覆盖）：

```
<workspace>/
├── 0ad-0.28.0/            # 上游 0.28 源码（已应用 patches/）
├── 0ad-0.28.0-unix-build.tar.xz   # 官方基线包（verify/regen 用）
├── lycium/lycium/         # OpenHarmony 第三方构建框架
├── SDL-src/               # SDL3 源码
├── manual-deps/           # 手工依赖源码（libsodium/enet/openal/curl/gloox/miniupnpc…）
├── bundled-ohos/          # fcollada / nvtt
└── 0ad-harmonyos/         # 本仓库
```

## 编译顺序

依赖之间有先后关系，按此顺序执行：

| 顺序 | 脚本 | 产物 |
|---|---|---|
| 1 | `build_manual.sh` | libsodium、enet |
| 2 | `build_openal_curl.sh` | openal-soft（SDL3 后端）、curl（openssl+zlib） |
| 3 | `build_gloox_miniupnpc.sh` | miniupnpc、gloox |
| 4 | `build_bundled.sh` | fcollada、nvtt（含 0 A.D. 自带补丁） |

其余库（mozjs / boost / icu / libxml2 / openssl / zlib / freetype2 / libpng …）
由 lycium 自带的 `lycium/lycium/usr` 流程构建。

## 环境要求

- macOS + `/opt/homebrew/bin`（需要 ninja、autotools、cmake）
- DevEco Studio 的 OHOS SDK，默认路径
  `/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony`
  （可用 `OHOS_SDK` 覆盖）

```bash
export ZEROAD_ROOT=/path/to/workspace
./scripts/deps/build_manual.sh
./scripts/deps/build_openal_curl.sh
./scripts/deps/build_gloox_miniupnpc.sh
./scripts/deps/build_bundled.sh
```
