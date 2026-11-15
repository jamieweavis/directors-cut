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
  -build -cook -stage -pak -iostore -compressed -prereqs \
  -archive -archivedirectory="$OUT_DIR"

APP="$(find "$OUT_DIR/Mac" -maxdepth 1 -name '*.app' | head -1)"
[[ -n "$APP" ]] || { echo "No .app found in $OUT_DIR/Mac"; exit 1; }

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
