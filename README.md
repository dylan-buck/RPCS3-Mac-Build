# RPCS3 Mac Build (Apple Silicon)

A custom RPCS3 build for Apple Silicon Macs (M1/M2/M3/M4) that keeps online play working against the current RPCN server (protocol **v34**).

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

## Download

**[Download Latest Release](../../releases/latest)**

Each release's notes say which RPCN protocol version it supports. If the latest release is older than the protocol above, build from source (see below).

## Installation

1. Download `RPCS3-Mac-Build-arm64.zip` from the latest release
2. Unzip and drag `RPCS3.app` to your **Applications** folder
3. **First launch:** Right-click the app and select **Open** (to bypass Gatekeeper)

### Code Signing Fix

If macOS shows a code signing error, open Terminal and run:

```bash
xattr -cr /Applications/RPCS3.app
codesign --force --deep --sign - /Applications/RPCS3.app
```

## Notes

- Release builds are for **Apple Silicon Macs** (arm64)
- Your existing RPCS3 config, games, and saves will work as-is
- Config location: `~/Library/Application Support/rpcs3/`

## Building from Source

| Platform | Guide | Status |
|---|---|---|
| macOS (Apple Silicon) | [BUILDING-LOCAL-MAC.md](BUILDING-LOCAL-MAC.md) | Primary target, used for releases |
| Windows (x64) | [BUILDING-WINDOWS.md](BUILDING-WINDOWS.md) | MSYS2 + Clang only; not yet tested on this fork |
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

### Creating a Release Package

After building, package the app for distribution:

```bash
cd build/bin
codesign --deep -fs - RPCS3.app
zip -r -9 RPCS3-Mac-Build-arm64.zip RPCS3.app
```

Upload the zip as a GitHub Release, naming the RPCN protocol version in the tag:

```bash
gh release create vX.X.X-XXXXX-rpcnXX \
  RPCS3-Mac-Build-arm64.zip \
  --title "RPCS3 Mac Build vX.X.X (RPCN vXX)" \
  --notes "Release notes here"
```

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
