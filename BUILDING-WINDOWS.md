# Building This Fork on Windows (x64)

This fork is maintained primarily for macOS, but it can be built for Windows with **MSYS2 + Clang**. This route uses the same scripts as the `Windows_Build_Clang` job in `.github/workflows/rpcs3.yml`, run by hand instead of in GitHub Actions.

> **Status:** built successfully on 2026-10-20 in a Windows 11 ARM64 VM (UTM on Apple Silicon) with MSYS2's Clang/LLVM 23.1.3, ffmpeg 9.0.2, OpenCV 5.0.0 and Qt 6.11.2. The resulting `rpcs3.exe` starts and reports its version; games haven't been tested with it yet. MSYS2 always installs the newest packages, so a future toolchain update can introduce new compile errors. If that happens, open an issue with the first error from the build log.

## Supported and Unsupported Routes

| Route | Works in this fork? | Why |
|---|---|---|
| **MSYS2 + Clang (CLANG64)** | **Yes, use this one** | Uses the CMake files (which include the Protobuf migration) and MSYS2's current LLVM |
| Visual Studio `rpcs3.sln` | **No** | The `.sln`/`.vcxproj` files were never updated for upstream's Flatbuffers → Protobuf migration, so the RPCN/NP code won't build. It also uses LLVM 19 (see below) |
| Visual Studio CMake preset (`msvc`) | **No** | Builds against RPCS3's prebuilt LLVM 19.1.7 or the bundled LLVM 19.1.7 source. This fork's JIT code requires LLVM 21 or newer |

The upstream instructions in [BUILDING.md](BUILDING.md) describe the Visual Studio routes. They apply to upstream RPCS3, not to this fork.

## Requirements

- Windows 10 or 11, x64
- About **20 GB** of free disk space. Measured: MSYS2 + packages and the build used about 15 GB on a fresh Windows install (clone ~3 GB, build output and ccache a few GB more)
- **4 GB RAM minimum.** With 4 GB, limit parallel jobs with `NINJA_JOBS=3` (see step 3); 8 GB or more is much more comfortable
- An hour or more for the first build; later builds are faster thanks to ccache

**Windows Defender:** real-time scanning slows down cloning and compiling enormously (a clone of this repo's ~170,000 files can stall partway). Excluding the build folders helps a lot. In an **administrator** PowerShell:

```powershell
Add-MpPreference -ExclusionPath C:\rpcs3,C:\msys64,C:\ccache
```

**Clock and time zone:** turn off **Settings → Time & language → Date & time → Set time zone automatically** and set your real time zone before starting. A clock or time zone change in the middle of a build leaves files with future timestamps, which makes Ninja loop forever (see [Troubleshooting](#ninja-error-manifest-buildninja-still-dirty-after-100-tries)).

**Windows on ARM (e.g. a UTM VM on Apple Silicon):** use the **CLANG64** environment, not CLANGARM64, so the result is an x64 build that normal Windows PCs can run. The x64 compiler runs under emulation there, so builds are slower than on an x64 PC (the tested VM took about an hour with 3 parallel jobs). You can't meaningfully test games in that VM; treat it as a build machine only.

## 1. Install MSYS2 and Packages

1. Install MSYS2 from <https://www.msys2.org> (default location `C:\msys64`).
2. From the Start menu, open **MSYS2 CLANG64**. All commands below run in this shell; the prompt shows `CLANG64` in purple.
3. Update the package database. If the shell asks to close, reopen **MSYS2 CLANG64** and run the command again until nothing is left to update:

   ```bash
   pacman -Syu
   ```

4. Install the build dependencies (the CI job's list, plus `openal` and `iconv`, which the build and deploy scripts need outside CI):

   ```bash
   pacman -S --needed \
     mingw-w64-clang-x86_64-clang \
     mingw-w64-clang-x86_64-ccache \
     mingw-w64-clang-x86_64-cmake \
     mingw-w64-clang-x86_64-lld \
     mingw-w64-clang-x86_64-ninja \
     mingw-w64-clang-x86_64-llvm \
     mingw-w64-clang-x86_64-ffmpeg \
     mingw-w64-clang-x86_64-opencv \
     mingw-w64-clang-x86_64-glew \
     mingw-w64-clang-x86_64-vulkan \
     mingw-w64-clang-x86_64-vulkan-headers \
     mingw-w64-clang-x86_64-vulkan-loader \
     mingw-w64-clang-x86_64-gtest \
     mingw-w64-clang-x86_64-qt6-base \
     mingw-w64-clang-x86_64-qt6-declarative \
     mingw-w64-clang-x86_64-qt6-multimedia \
     mingw-w64-clang-x86_64-qt6-svg \
     mingw-w64-clang-x86_64-openal \
     mingw-w64-clang-x86_64-iconv \
     base-devel curl git p7zip
   ```

5. Optionally, free 1–2 GB by clearing the downloaded package files:

   ```bash
   pacman -Scc
   ```

## 2. Clone the Repository

Clone to a short path without spaces. Some bundled third-party files have very long paths, so enable long path support first:

```bash
git config --global core.longpaths true
cd /c
git clone https://github.com/dylan-buck/RPCS3-Mac-Build.git rpcs3
cd rpcs3
```

All third-party dependencies are committed directly in this repo rather than as git submodules, so `--recursive` isn't needed.

## 3. Build

```bash
export BUILD_ARTIFACTSTAGINGDIRECTORY="$PWD/artifacts"
export CCACHE_DIR=/c/ccache
export NINJA_JOBS=3          # only needed with little RAM; leave unset to use all cores
.ci/setup-windows-ci-vars.sh win64 clang
.ci/build-windows-clang.sh
```

- `BUILD_ARTIFACTSTAGINGDIRECTORY` is normally set by CI. The deploy step fails without it, because that's where the packaged build is written.
- `setup-windows-ci-vars.sh` works out the version number and archive name and appends them to `.ci/ci-vars.env`. That file is tracked in git, so it will show as modified afterwards; `git checkout .ci/ci-vars.env` restores it.
- `NINJA_JOBS` limits how many files compile at once. Each compile job can use 1 GB of RAM or more, so on a 4 GB machine the default (one job per CPU core) runs out of memory.
- `build-windows-clang.sh` configures CMake in `build/` (using MSYS2's system OpenAL, because the bundled OpenAL 1.25.2 doesn't compile with Clang 23), compiles with Ninja, then runs `.ci/deploy-windows-clang.sh`. The deploy step copies the needed DLLs next to `rpcs3.exe`, downloads the controller and compatibility databases, and packages everything with 7-Zip.

## 4. Output

- **Runnable build:** `build/bin/rpcs3.exe` (with its DLLs in the same folder)
- **Packaged build:** `artifacts/rpcs3-v0.0.37-<commit count>-<hash>_win64_clang.7z`, plus a `.sha256` file

Share the `.7z`, not the `build/bin` folder. RPCS3 on Windows keeps its config and data next to the executable, so if you run the emulator from `build/bin`, your settings end up in there too.

## Rebuilding

`build-windows-clang.sh` runs `mkdir build`, and the deploy step creates `build/bin/config`, so neither can be re-run on top of an existing build. To make a new package after pulling changes, start from a clean build directory (ccache keeps this much faster than the first build):

```bash
rm -rf build
.ci/setup-windows-ci-vars.sh win64 clang
.ci/build-windows-clang.sh
```

For a quick compile check without packaging, run `ninja` inside the existing `build/` directory.

## Troubleshooting

### Errors in the LLVM JIT files

This fork's JIT code builds with **LLVM 21 through 23** (`Utilities/JITLLVM.cpp`, `rpcs3/Emu/CPU/CPUTranslator.h`, `rpcs3/Emu/Cell/PPUThread.cpp`, `rpcs3/Emu/Cell/SPULLVMRecompiler.cpp`). MSYS2 ships the newest LLVM, which may be newer than that. If the build fails in those files, check the version with `llvm-config --version` and include it when reporting the error.

### Ninja error: `manifest 'build.ninja' still dirty after 100 tries`

Some files have timestamps in the future, usually because the system clock or time zone changed mid-build. Ninja keeps regenerating the build forever. Run `cd build && ninja -d explain build.ninja` to see which file it complains about. To fix it, close any running build and reset all timestamps, keeping the build output newest (this doesn't force a full rebuild):

```bash
now=$(date +%s)
find /c/rpcs3 -path /c/rpcs3/build -prune -o -print0 | xargs -0 touch -h -d "@$now"
find /clang64 -print0 | xargs -0 touch -h -d "@$now"
find /c/rpcs3/build -print0 | xargs -0 touch -h -d "@$((now+5))"
```

Note that `.git` is included on purpose: the build depends on `.git/refs/heads/main` for its version number.

### Deploy fails with `iconv: command not found` or `conversion from char unsupported`

Install `mingw-w64-clang-x86_64-iconv` (step 1). Use the repo's current `.ci/deploy-windows-clang.sh`, which passes the input encoding explicitly.

### `mkdir: cannot create directory 'build': File exists`

A previous build directory is still there. See [Rebuilding](#rebuilding).

### Deploy step fails at `cygpath`

The build compiled, but `BUILD_ARTIFACTSTAGINGDIRECTORY` isn't set in the current shell. Export it (step 3) and rebuild.

### Path too long errors during clone or build

Make sure `git config --global core.longpaths true` is set, and clone to a short path such as `C:\rpcs3`.
