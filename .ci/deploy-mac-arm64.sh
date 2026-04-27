#!/bin/sh -ex

# shellcheck disable=SC2086
cd build || exit 1

# Gather explicit version number and number of commits
COMM_TAG=$(awk '/version{.*}/ { printf("%d.%d.%d", $5, $6, $7) }' ../rpcs3/rpcs3_version.cpp)
COMM_COUNT=$(git rev-list --count HEAD)
COMM_HASH=$(git rev-parse --short=8 HEAD)

AVVER="${COMM_TAG}-${COMM_COUNT}"

# AVVER is used for GitHub releases, it is the version number.
echo "AVVER=$AVVER" >> ../.ci/ci-vars.env

cd bin
mkdir "rpcs3.app/Contents/lib/" || true

cp "$(realpath /opt/homebrew/opt/llvm@$LLVM_COMPILER_VER/lib/c++/libc++abi.1.0.dylib)" "rpcs3.app/Contents/Frameworks/libc++abi.1.dylib"
cp "$(realpath /opt/homebrew/lib/libsharpyuv.0.dylib)" "rpcs3.app/Contents/lib/libsharpyuv.0.dylib"
cp "$(realpath /opt/homebrew/lib/libintl.8.dylib)" "rpcs3.app/Contents/lib/libintl.8.dylib"

rm -rf "rpcs3.app/Contents/Frameworks/QtPdf.framework" \
"rpcs3.app/Contents/Frameworks/QtQml.framework" \
"rpcs3.app/Contents/Frameworks/QtQmlModels.framework" \
"rpcs3.app/Contents/Frameworks/QtQuick.framework" \
"rpcs3.app/Contents/Frameworks/QtVirtualKeyboard.framework" \
"rpcs3.app/Contents/Plugins/platforminputcontexts" \
"rpcs3.app/Contents/Plugins/virtualkeyboard" \
"rpcs3.app/Contents/Resources/git"

../../.ci/optimize-mac.sh rpcs3.app

# Remove homebrew rpaths if present (may already be removed by macdeployqt)
install_name_tool -delete_rpath /opt/homebrew/lib RPCS3.app/Contents/MacOS/rpcs3 2>/dev/null || true
install_name_tool -delete_rpath /opt/homebrew/opt/llvm@$LLVM_COMPILER_VER/lib RPCS3.app/Contents/MacOS/rpcs3 2>/dev/null || true

# Need to do this rename hack due to case insensitive filesystem
mv rpcs3.app RPCS3_.app
mv RPCS3_.app RPCS3.app

# Make the bundle writable: dylibs copied from Homebrew Cellar inherit read-only mode,
# which blocks xattr -cr from clearing extended attributes
chmod -R u+w RPCS3.app
# Clear extended attributes before signing (prevents "detritus" errors)
xattr -cr RPCS3.app
codesign --force --deep -s - RPCS3.app

echo "[InternetShortcut]" > Quickstart.url
echo "URL=https://rpcs3.net/quickstart" >> Quickstart.url
echo "IconIndex=0" >> Quickstart.url

#DMG_FILEPATH="$BUILD_ARTIFACTSTAGINGDIRECTORY/rpcs3-v${COMM_TAG}-${COMM_COUNT}-${COMM_HASH}_macos_arm64.dmg"
#"$BREW_X64_PATH/bin/create-dmg" --volname RPCS3 \
#--window-size 800 400 \
#--icon-size 100 \
#--icon rpcs3.app 200 190 \
#--add-file Quickstart.url Quickstart.url 400 20 \
#--hide-extension rpcs3.app \
#--hide-extension Quickstart.url \
#--app-drop-link 600 185 \
#--skip-jenkins \
#--format ULMO \
#"$DMG_FILEPATH" \
#RPCS3.app
#FILESIZE=$(stat -f %z "$DMG_FILEPATH")
#SHA256SUM=$(shasum -a 256 "$DMG_FILEPATH" | awk '{ print $1 }')

# Create zip archive using ditto (preserves code signatures, symlinks, extended attributes)
ARCHIVE_FILEPATH="$BUILD_ARTIFACTSTAGINGDIRECTORY/RPCS3-Mac-Build-arm64.zip"
ditto -ck --sequesterRsrc --keepParent RPCS3.app "$ARCHIVE_FILEPATH"
FILESIZE=$(stat -f %z "$ARCHIVE_FILEPATH")
SHA256SUM=$(shasum -a 256 "$ARCHIVE_FILEPATH" | awk '{ print $1 }')

# Write sha256sum.txt for the auto-updater
echo "${SHA256SUM}  RPCS3-Mac-Build-arm64.zip" > "$BUILD_ARTIFACTSTAGINGDIRECTORY/sha256sum.txt"

cd ..
echo "${SHA256SUM};${FILESIZE}B" > "$RELEASE_MESSAGE"
cd bin
