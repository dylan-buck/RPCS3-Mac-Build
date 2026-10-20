# Building RPCS3 Locally on Apple Silicon Mac

This guide documents how to build this fork from source on Apple Silicon Macs (M1/M2/M3/M4).

## Prerequisites

- **macOS**: 14.0 (Sonoma) or later
- **Hardware**: Apple Silicon Mac (M1, M2, M3, or M4)
- **Homebrew**: Install from [brew.sh](https://brew.sh) if not already installed
- **Xcode** or **Xcode Command Line Tools**: Install with `xcode-select --install`

## Full Build (first time, or clean build)

### 1. Clone the Repository

```bash
git clone https://github.com/dylan-buck/RPCS3-Mac-Build.git
cd RPCS3-Mac-Build
```

All third-party dependencies are committed directly in this repo rather than as git submodules, so `--recursive` and `git submodule update` aren't needed.

### 2. Set Environment Variables

```bash
export QT_VER="6.7.3"
export QT_VER_MAIN="6"
export LLVM_COMPILER_VER="21"
export RUN_UNIT_TESTS="OFF"
```

The code requires **LLVM 21**. Older LLVM versions won't compile it (see [LLVM version](#llvm-version)).

Optionally, set the tag the in-app auto-updater compares against (defaults to `unknown`):

```bash
export RPCS3_RELEASE_TAG="v0.0.37-18119-rpcn34"
```

### 3. Run the Build Script

```bash
.ci/build-mac-arm64.sh
```

The build script automatically handles:
- Installing Homebrew dependencies (ninja, `llvm@21`, ffmpeg@5, MoltenVK, sdl3, glew, etc.)
- Unlinking any Homebrew Qt, then downloading Qt 6.7.3 via qt-downloader
- Running CMake configuration in `build/`
- Compiling with Ninja
- Running `.ci/deploy-mac-arm64.sh` to create the final app bundle

The script runs `mkdir build`, so it fails if `build/` already exists. Use it for first-time or clean builds; for updates use an [incremental build](#incremental-build-after-pulling-changes).

## Build Output

After a successful build:
- **Application**: `build/bin/RPCS3.app`

The deploy script (`deploy-mac-arm64.sh`) performs post-build steps:
- Copies required dylibs to the app bundle
- Removes unnecessary Qt frameworks
- Code signs the app with ad-hoc signature
- Creates Quickstart.url pointing to the quickstart guide

## Incremental Build (after pulling changes)

If `build/` already exists from a previous full build, rebuild only what changed:

```bash
cd build
ninja
```

Then replace the installed app's binary and re-sign the bundle. Replacing the binary invalidates the bundle's existing signature, so the re-sign step is required:

```bash
cp -p /Applications/RPCS3.app/Contents/MacOS/rpcs3 ~/rpcs3.backup   # optional backup
cp build/bin/RPCS3.app/Contents/MacOS/rpcs3 /Applications/RPCS3.app/Contents/MacOS/rpcs3
codesign --force --deep -s - /Applications/RPCS3.app
```

This only replaces the executable. If Homebrew library versions have changed since the last full build, check that every library the binary references is in the bundle:

```bash
B=build/bin/RPCS3.app/Contents/MacOS/rpcs3
for l in $(otool -L "$B" | awk '/@executable_path/{print $1}' | sed 's#@executable_path/../Frameworks/##'); do
  [ -e "/Applications/RPCS3.app/Contents/Frameworks/$l" ] || echo "MISSING: $l"
done
```

If anything is reported missing, copy the whole `build/bin/RPCS3.app` to `/Applications` instead.

## Verification

1. Check that the app exists:
   ```bash
   ls -la build/bin/RPCS3.app
   ```

2. Launch the app:
   ```bash
   open build/bin/RPCS3.app
   ```

3. Sign in to RPCN. If the client and server protocol versions don't match, `~/Library/Application Support/rpcs3/RPCS3.log` contains `Server returned protocol version: X, expected: Y`.

## Troubleshooting

### LLVM Version

The JIT code uses LLVM 21 APIs (`Module::setTargetTriple(llvm::Triple)`, `Intrinsic::getOrInsertDeclaration`). Building against LLVM 19 or 20 fails with errors in `Utilities/JITLLVM.cpp`, `rpcs3/Emu/CPU/CPUTranslator.h`, `PPUThread.cpp` or `SPULLVMRecompiler.cpp`. Make sure `LLVM_COMPILER_VER=21` is set and `brew install llvm@21` succeeded.

### Errors after a macOS / Xcode update (`unknown type name 'size_t'`, `wcschr`, `<cstddef> tried including <stddef.h>`)

An existing `build/` directory stores the full path of the macOS SDK it was configured with (for example `MacOSX26.5.sdk`). After a macOS or Xcode update that SDK may be gone, and every compile fails inside the libc++ / SDK headers.

The simplest fix is a [clean build](#clean-build). To keep the existing `build/` directory (and avoid a long rebuild of everything), point it at the new SDK in **all three** places:

```bash
cd build
ls "$(xcrun --show-sdk-platform-path)/Developer/SDKs/"   # find the new SDK name, e.g. MacOSX27.0.sdk
OLD=MacOSX26.5.sdk NEW=MacOSX27.0.sdk
sed -i '' "s#$OLD#$NEW#g" CMakeCache.txt CMakeFiles/*/CMakeCCompiler.cmake CMakeFiles/*/CMakeCXXCompiler.cmake
cmake .
ninja
```

Updating only `CMakeCache.txt` is not enough. The `CMakeFiles/<cmake-version>/CMake*Compiler.cmake` files list the SDK's `usr/include` as a built-in compiler path. If they still name the old SDK, CMake adds `-isystem <new SDK>/usr/include` explicitly, which breaks libc++'s header ordering (seen first in the bundled protobuf).

### Homebrew Dependencies

If the build fails due to missing dependencies, you can manually install them:
```bash
brew install ninja llvm@21 ffmpeg@5 molten-vk sdl3 glew cmake
```

Don't install or link Homebrew's Qt. The build script unlinks it on purpose so that `macdeployqt` doesn't bundle the wrong Qt version.

### Qt Issues

The build script uses `qt-downloader` to fetch Qt. If you encounter Qt-related issues, ensure the Qt version matches the environment variable:
```bash
echo $QT_VER  # Should output 6.7.3
```

### Clean Build

To perform a clean build, remove the build directory:
```bash
rm -rf build
```

Then run the build script again.

## Notes

- A full build takes roughly 15-30 minutes on modern Apple Silicon; incremental builds are much faster thanks to ccache
- The build requires several GB of disk space
- Internet connection is required for downloading dependencies
