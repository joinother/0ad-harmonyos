# 上游基线 / Upstream anchor

本仓库只包含 0 A.D. 的 **HarmonyOS 适配层**（鸿蒙壳工程 + 引擎补丁集）。
上游游戏引擎源码**不在此仓库内**，通过 `patches/` 应用。

| 项目 | 值 |
|---|---|
| 上游仓库 | https://github.com/0ad/0ad |
| 版本 | 0 A.D. **0.28「Boiorix」** |
| 基线产物 | `0ad-0.28.0-unix-build.tar.xz`（官方发布包） |
| 基线 sha256 | `27e217755ef76a922fe58dbf593d96e54b6ed2375d23f548c35619aa6bd5a42a` |
| 补丁集 | `patches/0001-ohos-engine-0ad-0.28.0.patch` |
| 涉及文件 | **61 个**，全部位于 `source/` 下 |
| 改动规模 | +1460 / −532 行 |
| 记录时间 | 2026-10-28 |

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
