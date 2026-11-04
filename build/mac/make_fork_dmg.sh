#!/bin/bash
#
# Jenny Script fork: packages src/_build/starcapp.app as a dmg without an Apple Developer ID.
# The app gets an ad-hoc signature only, so macOS shows a warning the first time it is opened
# (see README_open_on_mac.txt). The upstream create_dmg.sh needs Crashpad and Apple certificates.
#
# The fork maintainer has no Mac, so this script also fails on a bundle that could only work on
# the build machine: a missing architecture or a library outside of the bundle.
#
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT=../..
APP="Jenny Script.app"
STAGE=dmg_stage

VERSION=$(sed -n 's/.*kVersion\[\] = "\([^"]*\)".*/\1/p' \
    "$REPO_ROOT/src/core/management_layer/content/update/update_info.h")
if [ -z "$VERSION" ]; then
    echo "Error: fork version not found in update_info.h"
    exit 1
fi
DMG="JennyScript-v$VERSION-macos.dmg"

rm -rf "$APP" "$STAGE" "$DMG"
cp -R "$REPO_ROOT/src/_build/starcapp.app" "$APP"
find "$APP" -name "*.dSYM" -print0 | xargs -0 rm -rf
macdeployqt "$APP"

# The project files are SQLite. The other Qt SQL drivers need client libraries of database servers
find "$APP/Contents/PlugIns/sqldrivers" -type f ! -name "libqsqlite.dylib" -delete

#
# Licence and source notices (GPL-3.0 requires them next to the binaries)
#
copy_notices() {
    cp "$REPO_ROOT/LICENSE" "$1/LICENSE.txt"
    cp "$REPO_ROOT/NOTICE.txt" "$1/NOTICE.txt"
    mkdir -p "$1/licenses"
    cp "$REPO_ROOT/src/3rd_party/pdfhummus/LICENSE" "$1/licenses/pdfhummus.txt"
    cp "$REPO_ROOT/src/3rd_party/pdftextextraction/LICENSE" "$1/licenses/pdftextextraction.txt"
    cp "$REPO_ROOT/src/3rd_party/qgumboparser/LICENSE" "$1/licenses/qgumboparser.txt"
    cp "$REPO_ROOT/src/3rd_party/qtkeychain/COPYING" "$1/licenses/qtkeychain.txt"
    cp "$REPO_ROOT/src/3rd_party/hunspell/COPYING" "$1/licenses/hunspell.txt"
}
copy_notices "$APP/Contents/Resources"

#
# Info.plist of upstream claims macOS 10.7: state the real minimum of this Qt build, so an older
# Mac gets a clear message instead of a crash
#
MIN_MACOS=$(vtool -arch arm64 -show-build "$APP/Contents/MacOS/starcapp" | awk '/minos/ { print $2 }')
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $MIN_MACOS" "$APP/Contents/Info.plist"
echo "Minimum macOS: $MIN_MACOS"

#
# Every binary must run on Intel and Apple Silicon, and load only from the bundle or the system.
# A run takes half an hour, so report every problem instead of stopping at the first one
#
PROBLEMS=0
report() {
    echo "Error: $1"
    PROBLEMS=$((PROBLEMS + 1))
}

check_binary() {
    local file=$1 arch library own_id resolved
    for arch in x86_64 arm64; do
        if ! lipo "$file" -verify_arch "$arch" 2>/dev/null; then
            report "$file has no $arch code"
            continue
        fi
        own_id=$(otool -arch "$arch" -D "$file" | tail -n +2)
        while read -r library; do
            [ "$library" = "$own_id" ] && continue
            case "$library" in
                /usr/lib/* | /System/Library/*) continue ;;
                @loader_path/*) continue ;;
                @rpath/*) resolved="$APP/Contents/Frameworks/${library#@rpath/}" ;;
                @executable_path/*) resolved="$APP/Contents/MacOS/${library#@executable_path/}" ;;
                *)
                    report "$file ($arch) needs $library from outside of the bundle"
                    continue
                    ;;
            esac
            if [ ! -e "$resolved" ]; then
                report "$file ($arch) needs $library, not found in the bundle"
            fi
        done < <(otool -arch "$arch" -L "$file" | tail -n +2 | sed -E 's/^[[:space:]]+//; s/ \(compatibility.*$//')
    done
}

BINARIES=0
while IFS= read -r -d '' file; do
    case "$(file -b "$file")" in
        *Mach-O*)
            check_binary "$file"
            BINARIES=$((BINARIES + 1))
            ;;
    esac
done < <(find "$APP" -type f -print0)
echo "Checked $BINARIES binaries"
if [ "$BINARIES" -lt 10 ]; then
    report "too few binaries in the bundle, the check did not look at the right files"
fi
if [ "$PROBLEMS" -gt 0 ]; then
    echo "$PROBLEMS problems found"
    exit 1
fi

# macdeployqt and the Info.plist change invalidate the signatures from the linker, and Apple
# Silicon refuses to run code without a valid signature
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

mkdir "$STAGE"
ditto "$APP" "$STAGE/$APP"
ln -s /Applications "$STAGE/Applications"
copy_notices "$STAGE"
cp README_open_on_mac.txt "$STAGE/Doc truoc - Read me first.txt"

# hdiutil sometimes reports "Resource busy" on CI machines, upstream retries the same way
for attempt in 1 2 3 4 5; do
    if hdiutil create -srcfolder "$STAGE" -format UDZO -volname "Jenny Script" -ov "$DMG"; then
        break
    fi
    if [ "$attempt" = 5 ]; then
        exit 1
    fi
    sleep 15
done

rm -rf "$STAGE"
echo "Created $DMG"
