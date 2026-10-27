# 0 A.D. HarmonyOS Port

A native HarmonyOS port of [0 A.D.](https://play0ad.com/) — an open-source real-time strategy (RTS) game developed by [Wildfire Games](https://wildfiregames.com/). This project brings 0 A.D. (version 0.28.0 "Boiorix") to HarmonyOS PC devices (e.g., HAD-W32) using SDL3 with a custom HarmonyOS backend.

## Attribution

This is a **derivative work** based on 0 A.D. and the Pyrogenesis engine by Wildfire Games. All credit for the original game, engine, art, and design goes to the Wildfire Games team and the 0 A.D. community.

- Original project: https://play0ad.com/
- Source repository: https://github.com/0ad/0ad
- License: GPL-2.0-or-later (see [LICENSE](./LICENSE))

## License

This port is licensed under the **GNU General Public License v2.0 or later** (GPL-2.0+), consistent with the original 0 A.D. / Pyrogenesis engine license. See [LICENSE](./LICENSE) for the full text.

## Architecture

The port consists of a HarmonyOS (OpenHarmony) native application that bridges the original C++ Pyrogenesis engine with the ArkTS UI layer:

```
┌──────────────────────────────────────────┐
│  ArkTS UI (Index.ets)                    │
│  XComponent (SDL3 surface)               │
│  Axis/key event forwarding               │
└───────────┬──────────────────────────────┘
            │ NAPI (libclibridge.so)
┌───────────▼──────────────────────────────┐
│  C++ Glue Layer                          │
│  ohos_napi.cpp   — NAPI command bridge   │
│  ohos_bootstrap.cpp — asset download     │
│  ohos_cli.cpp    — CLI dispatch           │
│  ohos_input_monitor.cpp — input capture   │
│  ohos_wrapper.c  — SDL3 main entry        │
│  ohos_rtti_shim.cpp — typeinfo shim       │
│  cacert_data.c   — embedded CA bundle     │
└───────────┬──────────────────────────────┘
            │ static link
┌───────────▼──────────────────────────────┐
│  Pyrogenesis Engine (libmain.so)         │
│  0 A.D. 0.28.0 source (GPL-2.0+)        │
│  + SDL3 (HarmonyOS backend)              │
│  + SpiderMonkey (servo-mozjs)            │
│  + Boost, ICU, libxml2, OpenAL, etc.     │
└──────────────────────────────────────────┘
```

### Key Components

| File | Purpose |
|---|---|
| `entry/src/main/cpp/CMakeLists.txt` | Build system — compiles the 0AD engine + glue code into native libraries |
| `entry/src/main/cpp/ohos_napi.cpp` | NAPI bridge: `import { command } from 'libclibridge.so'` → dlopen/dlsym into libmain.so |
| `entry/src/main/cpp/ohos_bootstrap.cpp` | First-run asset bootstrap: downloads game data over LAN via libcurl |
| `entry/src/main/cpp/ohos_cli.cpp` | CLI command dispatcher (`OhosCli_Dispatch`) for runtime engine control |
| `entry/src/main/cpp/ohos_input_monitor.cpp` | Input event monitoring and forwarding |
| `entry/src/main/cpp/ohos_wrapper.c` | SDL3 main entry wrapper |
| `entry/src/main/cpp/ohos_rtti_shim.cpp` | RTTI shim for missing `js::BaseProxyHandler` typeinfo |
| `entry/src/main/cpp/cacert_data.c` | Embedded Mozilla CA certificate bundle (for TLS) |
| `entry/src/main/ets/pages/Index.ets` | ArkTS UI: XComponent surface + axis/key event forwarding |
| `entry/src/main/ets/entryability/EntryAbility.ets` | HarmonyOS entry ability |

## Build Prerequisites

This project does **not** include the 0 A.D. engine source or pre-built dependencies. You need to set up the following sibling directories:

1. **0 A.D. source** (`0ad-0.28.0/`) — Download from https://play0ad.com/ and extract
2. **SDL3 source** (`SDL-src/`) — HarmonyOS backend build of SDL3
3. **Lycium cross-compiled dependencies** (`lycium/`) — Pre-built libraries for arm64-v8a:
   - mozjs (SpiderMonkey), boost, fmt, icu, libxml2, freetype2, libpng, zlib, libiconv, libsodium, libcurl, openssl, openal, enet, fcollada, nvtt, libvorbis, libogg, gloox, miniupnpc, xz

Set the `ZEROAD_ROOT` environment variable to the directory containing these siblings:

```bash
export ZEROAD_ROOT=/path/to/workspace
# This directory should contain:
#   0ad-0.28.0/
#   lycium/
#   SDL-src/  (symlinked as entry/src/main/cpp/SDL)
```

You also need:
- [DevEco Studio](https://developer.huawei.com/consumer/cn/deveco-studio/) (for HarmonyOS SDK and build tools)
- HarmonyOS SDK 6.1.0+ (API 23+)
- A HarmonyOS device or emulator for testing

## Building

1. Open the project in DevEco Studio
2. Configure signing in `build-profile.json5` (signing configs are not included in the repo for security)
3. Set `ZEROAD_ROOT` environment variable
4. Symlink or copy SDL3 source: `ln -s /path/to/SDL-src entry/src/main/cpp/SDL`
5. Build via DevEco Studio or `hvigor` CLI

## Engine Patchset

The Pyrogenesis engine is **not vendored** in this repository. Every HarmonyOS
change to the engine lives in
[`patches/0001-ohos-engine-0ad-0.28.0.patch`](./patches/0001-ohos-engine-0ad-0.28.0.patch)
(61 files, +1460 / −532 lines). [`UPSTREAM.md`](./UPSTREAM.md) records the
upstream anchor: repository, version (0.28 "Boiorix") and the sha256 of the
baseline tarball.

```bash
# Apply to a freshly extracted official 0.28 source tree
cd 0ad-0.28.0
patch -p1 < ../0ad-harmonyos/patches/0001-ohos-engine-0ad-0.28.0.patch

# or use the helper (does a dry-run first)
../0ad-harmonyos/patches/apply.sh .
```

Two maintenance helpers keep the patch and the working tree from silently
drifting apart. Both locate the workspace via `$ZEROAD_ROOT` and fall back to
the baseline tarball:

| Script | Purpose |
|---|---|
| `patches/verify.sh` | Checks that the current engine tree still equals *pristine + patch*. Run it after any engine edit; exits non-zero and lists the drifting files. |
| `patches/regen.sh` | Regenerates the patch from pristine + current sources, prints the new file count / line delta and the baseline sha256 (update `UPSTREAM.md` afterwards). |
| `scripts/sync-upstream.sh` | Fetches the upstream tag's tree from Gitea (blobless, ~1 MB) and hash-compares it against the local engine, so a drift outside the patch is detected. |
| `scripts/upgrade-plan.sh <ref>` | Reports how many files upstream changed since the baseline and how many of them overlap our 61 patched files — i.e. the conflict surface of a future upgrade. Report only, changes nothing. |

```bash
./patches/verify.sh         # ✅ no drift  /  ⚠️ lists drifting files
./patches/regen.sh          # rewrite the patch after intentional engine changes
./scripts/sync-upstream.sh  # compare against the actual upstream tag (needs proxy)
./scripts/upgrade-plan.sh main   # estimate the conflict surface of a future upgrade
```

> Upstream access notes: `gitea.wildfiregames.com` resolves to a Clash fake-IP
> (`198.18.x.x`) on this machine and its smart-http breaks under protocol v2 —
> the helper forces `protocol.version=0` and reuses `git config http.proxy`.

## Dependency Build Scripts

Cross-compile recipes for the OHOS dependencies are tracked in
[`scripts/deps/`](./scripts/deps/) so a fresh machine can rebuild them. They are
parameterized — the workspace root comes from `$ZEROAD_ROOT` (auto-detected from
the script location otherwise) and no absolute personal paths are hard-coded.

| Order | Script | Produces |
|---|---|---|
| 1 | `scripts/deps/build_manual.sh` | libsodium, enet |
| 2 | `scripts/deps/build_openal_curl.sh` | openal-soft (SDL3 backend), curl |
| 3 | `scripts/deps/build_gloox_miniupnpc.sh` | miniupnpc, gloox |
| 4 | `scripts/deps/build_bundled.sh` | fcollada, nvtt |

See [`scripts/deps/README.md`](./scripts/deps/README.md) for the expected
workspace layout, build order and required environment.

## Input Support

The port supports three input methods simultaneously:

- **Mouse + Touchpad**: Left-click select, right-click command, scroll/pinch zoom, two-finger pan
- **Touch**: Single-finger tap/drag for select and box-select, long-press for right-click, two-finger pan/zoom
- **Keyboard**: WASD/arrows for camera, Esc for cancel, standard 0 A.D. hotkeys

## Known Limitations

- **Vulkan backend**: Dark output and occasional GPU sync crashes on Maleoon GPU; use OpenGL (default)
- **Gamepad**: No SDL HarmonyOS gamepad backend yet (planned via GameControllerKit)
- **Atlas scenario editor**: Not compiled (depends on wxWidgets); menu entry hidden
- **GPU skinning**: Falls back to CPU skinning on GLES; animation may appear choppy at high resolutions
- **Occasional black screen**: When switching from browser etc.; switch page once to recover

## Acknowledgments

- **Wildfire Games** — creators of 0 A.D. and the Pyrogenesis engine
- **SDL3** — Simple DirectMedia Layer (HarmonyOS backend)
- **Mozilla** — SpiderMonkey JS engine and CA certificate bundle
- All third-party library maintainers whose work makes this port possible
