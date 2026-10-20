# Building This Fork on Windows (x64)

This fork is maintained primarily for macOS, but it can be built for Windows with **MSYS2 + Clang**. This route uses the same scripts as the `Windows_Build_Clang` job in `.github/workflows/rpcs3.yml`, run by hand instead of in GitHub Actions.

> **Status:** these steps follow the CI job and have been checked against the scripts in this repo, but a full Windows build of this fork has not been tested yet. If something fails, open an issue with the first error from the build log.

## Supported and Unsupported Routes

| Route | Works in this fork? | Why |
|---|---|---|
| **MSYS2 + Clang (CLANG64)** | **Yes, use this one** | Uses the CMake files (which include the Protobuf migration) and MSYS2's current LLVM |
| Visual Studio `rpcs3.sln` | **No** | The `.sln`/`.vcxproj` files were never updated for upstream's Flatbuffers → Protobuf migration, so the RPCN/NP code won't build. It also uses LLVM 19 (see below) |
| Visual Studio CMake preset (`msvc`) | **No** | Builds against RPCS3's prebuilt LLVM 19.1.7 or the bundled LLVM 19.1.7 source. This fork's JIT code requires LLVM 21 |

The upstream instructions in [BUILDING.md](BUILDING.md) describe the Visual Studio routes. They apply to upstream RPCS3, not to this fork.

## Requirements

- Windows 10 or 11, x64
- About **30 GB** of free disk space (MSYS2 packages ~8–10 GB, source ~6 GB, build output ~10 GB)
- A few hours for the first build; later builds are faster thanks to ccache

**Windows on ARM (e.g. a UTM VM on Apple Silicon):** use the **CLANG64** environment, not CLANGARM64, so the result is an x64 build that normal Windows PCs can run. The x64 compiler runs under emulation there, so expect the first build to take several hours. You can't meaningfully test games in that VM; treat it as a build machine only.

## 1. Install MSYS2 and Packages

1. Install MSYS2 from <https://www.msys2.org> (default location `C:\msys64`).
2. From the Start menu, open **MSYS2 CLANG64**. All commands below run in this shell; the prompt shows `CLANG64` in purple.
3. Update the package database. If the shell asks to close, reopen **MSYS2 CLANG64** and run the command again until nothing is left to update:

   ```bash
   pacman -Syu
   ```

4. Install the build dependencies (the same list as the CI job):

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
     base-devel curl git p7zip
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
.ci/setup-windows-ci-vars.sh win64 clang
.ci/build-windows-clang.sh
```

- `BUILD_ARTIFACTSTAGINGDIRECTORY` is normally set by CI. The deploy step fails without it, because that's where the packaged build is written.
- `setup-windows-ci-vars.sh` works out the version number and archive name and appends them to `.ci/ci-vars.env`. That file is tracked in git, so it will show as modified afterwards; `git checkout .ci/ci-vars.env` restores it.
- `build-windows-clang.sh` configures CMake in `build/`, compiles with Ninja, then runs `.ci/deploy-windows-clang.sh`. The deploy step copies the needed DLLs next to `rpcs3.exe`, downloads the controller and compatibility databases, and packages everything with 7-Zip.

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

This fork's JIT code targets **LLVM 21** (`Utilities/JITLLVM.cpp`, `rpcs3/Emu/CPU/CPUTranslator.h`, `rpcs3/Emu/Cell/PPUThread.cpp`, `rpcs3/Emu/Cell/SPULLVMRecompiler.cpp`). MSYS2 ships the newest LLVM, which may be newer than 21. If the build fails in those files, check the version with `llvm-config --version` and include it when reporting the error.

### `mkdir: cannot create directory 'build': File exists`

A previous build directory is still there. See [Rebuilding](#rebuilding).

### Deploy step fails at `cygpath`

The build compiled, but `BUILD_ARTIFACTSTAGINGDIRECTORY` isn't set in the current shell. Export it (step 3) and rebuild.

### Path too long errors during clone or build

Make sure `git config --global core.longpaths true` is set, and clone to a short path such as `C:\rpcs3`.
