# RPCS3 Mac Build (Apple Silicon + Windows)

A custom RPCS3 build for Apple Silicon Macs (M1/M2/M3/M4), also available for Windows x64, that keeps online play working against the current RPCN server (protocol **v34**).

## What Is This?

This is a patched RPCS3 PS3 emulator build that keeps online multiplayer working on Apple Silicon Macs. The fork stays on a stable base and only picks up upstream changes when the RPCN server bumps its protocol version, since older builds can no longer connect after a bump.

**Base:** RPCS3 0.0.37-18119

**RPCN protocol:** v34

**Upstream changes applied:**
- [PR #17825](https://github.com/RPCS3/rpcs3/pull/17825) — RPCN v1.5.0 protocol update (v30)
- [PR #18112](https://github.com/RPCS3/rpcs3/pull/18112) — Additional RPCN v30 fixes
- [PR #18760](https://github.com/RPCS3/rpcs3/pull/18760) — Trophy sync (v31)
- [PR #19306](https://github.com/RPCS3/rpcs3/pull/19306) — Delete trophies from RPCN (v32; client/protocol part only, no UI)
- Upstream `1a9dca3a3a` — TSS (title small storage) support (v34)
- RPCN fixes: filter UnlockTrophy reply, type cast warning fixes
- macOS fixes: Starhawk RSX fix, aligned allocator for `simple_array`, Vulkan FSR barrier/format fixes, LLVM 21 build support
- Windows build fixes for MSYS2's current toolchain (Clang/LLVM 23, ffmpeg 9, OpenCV 5), mostly backported from upstream

## Download

**[Download Latest Release](../../releases/latest)**

Each release's notes say which RPCN protocol version it supports. If the latest release is older than the protocol above, build from source (see below).

Each release contains:

| File | Platform |
|---|---|
| `RPCS3-Mac-Build-arm64.zip` | macOS, Apple Silicon |
| `RPCS3-Windows-Build-x64.7z` | Windows 10/11, x64 |
| `sha256sum.txt` | Checksums (also read by the macOS in-app updater) |

## Installation (macOS)

1. Download `RPCS3-Mac-Build-arm64.zip` from the latest release
2. Unzip and drag `RPCS3.app` to your **Applications** folder
3. **First launch:** Right-click the app and select **Open** (to bypass Gatekeeper)

Builds from the RPCN v34 release on can update themselves: **Help → Check for Updates** (or the automatic check at startup) installs the latest release.

### Code Signing Fix

If macOS shows a code signing error, open Terminal and run:

```bash
xattr -cr /Applications/RPCS3.app
codesign --force --deep --sign - /Applications/RPCS3.app
```

## Installation (Windows)

1. Download `RPCS3-Windows-Build-x64.7z` from the latest release
2. Extract it to a folder you can write to, such as `C:\RPCS3`. Windows 11 opens `.7z` files directly (right-click → **Extract All**); on Windows 10, use [7-Zip](https://www.7-zip.org)
3. Run `rpcs3.exe`. The build isn't code-signed, so if SmartScreen says "Windows protected your PC", click **More info → Run anyway**

Requires a GPU driver with Vulkan support; everything else is included. The in-app updater is macOS-only, so download new releases manually.

## Notes

- Your existing RPCS3 config, games, and saves will work as-is
- Config location: `~/Library/Application Support/rpcs3/` on macOS, next to `rpcs3.exe` on Windows
- The Windows build is new and hasn't been tested with games yet

## Building from Source

| Platform | Guide | Status |
|---|---|---|
| macOS (Apple Silicon) | [BUILDING-LOCAL-MAC.md](BUILDING-LOCAL-MAC.md) | Primary target, used for releases |
| Windows (x64) | [BUILDING-WINDOWS.md](BUILDING-WINDOWS.md) | MSYS2 + Clang only; builds and starts, not yet tested with games |
| Linux | [BUILDING.md](BUILDING.md) (upstream guide) | Not tested on this fork |

### Quick Start (macOS)

```bash
git clone https://github.com/dylan-buck/RPCS3-Mac-Build.git
cd RPCS3-Mac-Build

export QT_VER="6.7.3"
export QT_VER_MAIN="6"
export LLVM_COMPILER_VER="21"
export RUN_UNIT_TESTS="OFF"

.ci/build-mac-arm64.sh
```

All third-party dependencies are committed directly in this repo (not as git submodules), so `--recursive` isn't needed.

The build script installs dependencies via Homebrew, downloads Qt, configures CMake, compiles with Ninja, and creates the final app bundle at `build/bin/RPCS3.app`.

### Creating a Release

The macOS in-app updater reads the **latest** release and compares its tag with the tag compiled into the app, so the release tag has to be chosen before building.

1. **Pick the tag**, naming the RPCN protocol version, e.g. `v0.0.37-18119-rpcn34`.
2. **Build the Mac app with that tag.** It's only read when CMake configures, so re-run CMake:

   ```bash
   cd build
   RPCS3_RELEASE_TAG=v0.0.37-18119-rpcnXX cmake .
   ninja
   ```

   For a full build, export `RPCS3_RELEASE_TAG` before running `.ci/build-mac-arm64.sh` instead.
3. **Package it** with the deploy script (from the repo root). It signs the app and writes `RPCS3-Mac-Build-arm64.zip` and `sha256sum.txt`:

   ```bash
   mkdir -p /tmp/release
   LLVM_COMPILER_VER=21 BUILD_ARTIFACTSTAGINGDIRECTORY=/tmp/release RELEASE_MESSAGE=/tmp/release/GitHubReleaseMessage.txt .ci/deploy-mac-arm64.sh
   ```

4. **Add the Windows build** ([BUILDING-WINDOWS.md](BUILDING-WINDOWS.md)) as `RPCS3-Windows-Build-x64.7z`, and append its hash to `sha256sum.txt` **after** the Mac line:

   ```bash
   cd /tmp/release
   shasum -a 256 RPCS3-Windows-Build-x64.7z >> sha256sum.txt
   ```

5. **Publish** as the latest release:

   ```bash
   gh release create v0.0.37-18119-rpcnXX \
     RPCS3-Mac-Build-arm64.zip RPCS3-Windows-Build-x64.7z sha256sum.txt \
     --title "RPCS3 Mac & Windows Build v0.0.37-18119 (RPCN vXX)" \
     --notes-file notes.md --latest
   ```

Two rules keep the updater working: the Mac zip must be the **only** `.zip` in the release (the updater downloads the first `.zip` it finds, so ship Windows as `.7z`), and the Mac hash must be the **first** line of `sha256sum.txt` (the updater uses the first hash it finds).

## Updating for a New RPCN Protocol Version

When upstream bumps `RPCN_PROTOCOL_VERSION` in `rpcs3/Emu/NP/rpcn_client.cpp`:

```bash
git remote add upstream https://github.com/RPCS3/rpcs3.git   # first time only
git fetch upstream master
# Find the commit(s) that changed the protocol version
git log upstream/master -L '/constexpr u32 RPCN_PROTOCOL_VERSION/,+1:rpcs3/Emu/NP/rpcn_client.cpp' -s --format='%h %ad %s' --date=short
git cherry-pick -x <commit>
```

Cherry-pick the whole commit that bumps the version, not just the number. The server expects the new commands that ship with the bump.

## Disclaimer

This is an **unofficial** custom build. For issues specific to this build, please [open an issue here](../../issues). Do **not** report issues from this build to the official RPCS3 project.
