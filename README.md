# RPCS3 Mac Build (Apple Silicon)

A custom RPCS3 build for Apple Silicon Macs (M1/M2/M3/M4) with RPCN v1.5/v30 online support.

## What Is This?

This is a patched RPCS3 PS3 emulator build that restores online multiplayer functionality on Apple Silicon Macs. The official RPCN server was updated to protocol v30 (RPCN v1.5), which broke online play for older builds. This build includes the necessary patches.

**Base:** RPCS3 0.0.37-18119
**Patches applied:**
- [PR #17825](https://github.com/RPCS3/rpcs3/pull/17825) — RPCN v1.5.0 protocol update
- [PR #18112](https://github.com/RPCS3/rpcs3/pull/18112) — Additional RPCN v30 fixes

## Download

**[Download Latest Release](../../releases/latest)**

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

- This build is specifically for **Apple Silicon Macs** (arm64)
- Your existing RPCS3 config, games, and saves will work as-is
- Config location: `~/Library/Application Support/rpcs3/`

## Building from Source

### Prerequisites

- macOS 14.0 (Sonoma) or later on Apple Silicon
- Xcode Command Line Tools (`xcode-select --install`)
- [Homebrew](https://brew.sh)

### Build

```bash
git clone --recursive https://github.com/dylan-buck/RPCS3-Mac-Build.git
cd RPCS3-Mac-Build

export QT_VER="6.7.3"
export QT_VER_MAIN="6"
export LLVM_COMPILER_VER="19"
export RUN_UNIT_TESTS="OFF"

.ci/build-mac-arm64.sh
```

The build script installs dependencies via Homebrew, downloads Qt, configures CMake, compiles with Ninja, and creates the final app bundle at `build/bin/rpcs3.app`.

### Creating a Release Package

After building, package the app for distribution:

```bash
cd build/bin
cp -R rpcs3.app RPCS3.app
codesign --deep -fs - RPCS3.app
zip -r -9 RPCS3-Mac-Build-arm64.zip RPCS3.app
rm -rf RPCS3.app
```

Upload the zip as a GitHub Release:

```bash
gh release create vX.X.X-XXXXX-rpcnXX \
  RPCS3-Mac-Build-arm64.zip \
  --title "RPCS3 Mac Build vX.X.X" \
  --notes "Release notes here"
```

## Disclaimer

This is an **unofficial** custom build. For issues specific to this build, please [open an issue here](../../issues). Do **not** report issues from this build to the official RPCS3 project.
