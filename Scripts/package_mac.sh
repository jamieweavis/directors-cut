#!/usr/bin/env bash
# Builds, cooks and packages Director's Cut for macOS, then zips it for upload.
#
#   Scripts/package_mac.sh            # Shipping build
#   Scripts/package_mac.sh --dev      # Development build (logs + console available)
#
# Output: Packaged/Mac/ (the .app) and Packaged/DirectorsCut-Mac.zip
# Engine: set UE_ROOT to override the default install location.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/DirectorsCut.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.9}"
RUNUAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
OUT_DIR="$PROJECT_DIR/Packaged"
ZIP="$OUT_DIR/DirectorsCut-Mac.zip"
LIMIT_BYTES=$((1024 * 1024 * 1024))   # jam limit: build must be under 1 GB

CONFIG="Shipping"
[[ "${1:-}" == "--dev" ]] && CONFIG="Development"

[[ -x "$RUNUAT" ]] || { echo "RunUAT not found at: $RUNUAT (set UE_ROOT)"; exit 1; }
[[ -f "$UPROJECT" ]] || { echo "Project not found: $UPROJECT"; exit 1; }

# Warn if the game would ship muted.
if grep -qiE '^\s*bMuteAll\s*=\s*True' "$PROJECT_DIR/Config/DefaultGame.ini"; then
  echo "WARNING: Config/DefaultGame.ini has bMuteAll=True, so the packaged game will be SILENT."
  read -r -p "Continue anyway? [y/N] " ans
  [[ "$ans" =~ ^[Yy]$ ]] || exit 1
fi

if pgrep -fq "UnrealEditor.*DirectorsCut"; then
  echo "WARNING: the editor has DirectorsCut open. Close it to avoid cook/asset lock issues."
  read -r -p "Continue anyway? [y/N] " ans
  [[ "$ans" =~ ^[Yy]$ ]] || exit 1
fi

echo "==> Packaging DirectorsCut ($CONFIG) for Mac"
START=$(date +%s)

"$RUNUAT" BuildCookRun \
  -project="$UPROJECT" \
  -noP4 -utf8output -unattended \
  -platform=Mac \
  -clientconfig="$CONFIG" \
  -build -cook -stage -pak -iostore -compressed -prereqs

# Every "/Game/..." path hard-coded in Source must have been cooked. Assets loaded
# by path from C++ are invisible to the cooker unless their folder is listed in
# DefaultGame.ini DirectoriesToAlwaysCook; missing ones render as grey/black.
COOKED_SET="$PROJECT_DIR/Saved/Cooked/Mac/DirectorsCut/Metadata/ReferencedSet.txt"
MISSING=0
for p in $(grep -rhoE '"/Game/[^"]+"' "$PROJECT_DIR/Source" | tr -d '"' | sed -E 's/\.[A-Za-z0-9_]+$//' | sort -u); do
  # Folder paths (e.g. /Game/DC/Audio) count as cooked if anything under them is.
  grep -qiE "^${p}(/|$)" "$COOKED_SET" || { echo "ERROR: not cooked: $p"; MISSING=1; }
done
(( MISSING == 0 )) || { echo "Add the folder(s) to DirectoriesToAlwaysCook in Config/DefaultGame.ini."; exit 1; }

# Don't use UAT's -archive on Mac: it copies the bare Binaries/Mac .app (no
# Contents/UE, no dylibs), which dies at launch with "libtbb.12.dylib not
# loaded". The complete, self-contained bundle is the staged one.
STAGED_APP="$(find "$PROJECT_DIR/Saved/StagedBuilds/Mac" -maxdepth 1 -name '*.app' | head -1)"
[[ -n "$STAGED_APP" && -d "$STAGED_APP/Contents/UE" ]] || { echo "No complete staged .app in Saved/StagedBuilds/Mac"; exit 1; }

rm -rf "$OUT_DIR/Mac"
mkdir -p "$OUT_DIR/Mac"
APP="$OUT_DIR/Mac/$(basename "$STAGED_APP")"
echo "==> Copying staged app to $APP"
ditto "$STAGED_APP" "$APP"

# Sanity check: every @rpath dylib the executable needs must be inside the bundle.
EXE="$APP/Contents/MacOS/$(basename "$APP" .app)"
for lib in $(otool -L "$EXE" | awk '/@rpath\// && !/weak/ {sub("@rpath/","",$1); print $1}'); do
  if [[ -z "$(find "$APP" -name "$lib" -print -quit)" ]]; then
    echo "ERROR: $lib is not bundled; the app will crash at launch."; exit 1
  fi
done

echo "==> Zipping $(basename "$APP")"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"

SIZE=$(stat -f%z "$ZIP")
SIZE_MB=$((SIZE / 1024 / 1024))
ELAPSED=$(( $(date +%s) - START ))

echo
echo "Done in $((ELAPSED / 60))m $((ELAPSED % 60))s"
echo "  App: $APP"
echo "  Zip: $ZIP (${SIZE_MB} MB)"
if (( SIZE >= LIMIT_BYTES )); then
  echo "  WARNING: zip is over the 1 GB jam limit!"
fi
echo
echo "Note: the app is unsigned. Players will need to right-click > Open (or run"
echo "  xattr -dr com.apple.quarantine \"/path/to/$(basename "$APP")\") the first time."
