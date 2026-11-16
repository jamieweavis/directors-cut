#!/usr/bin/env bash
# Renders the store thumbnail (630x500) on macOS: the title wordmark over the house and a parked car.
#
#   Scripts/thumbnail_mac.sh                 # -> Saved/Screenshots/thumbnail-<timestamp>/thumbnail.png
#   Scripts/thumbnail_mac.sh --zoom 0.6      # closer on the house
#
# The game is launched once at the title screen with -DCThumbnail (the title shows only the wordmark and tagline,
# large) and the "thumb" shot focus (the camera puts the house in the lower right and parks a car by it); see
# Source/DirectorsCut/Core/DCDevTools.h. It renders at --scale times the final size and is shrunk with sips, so the
# UI keeps its normal proportions and the edges come out clean. Leave the mouse alone while the window is open.
#
# Options:
#   --width N         final width, default 630
#   --height N        final height, default 500
#   --scale N         render at N x the final size, default 2
#   --car N           which car model/colour is parked by the house (any number); default 0
#   --zoom Z          camera zoom on the house, 0 = overview distance .. 1 = closest; default 0.68
#   --shot-delay S    seconds before shooting (raise it if shaders are still compiling); default 8
#   --timeout S       stop the game if it runs longer than this; default 300
#   --no-build        skip building the editor target
#   --app PATH        packaged Development build to shoot instead (Scripts/package_mac.sh --dev)
#   --out DIR         default: Saved/Screenshots/thumbnail-<timestamp>
# Engine: set UE_ROOT to override the default install location.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/DirectorsCut.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.9}"

WIDTH=630; HEIGHT=500; SCALE=2; CAR=0; ZOOM=0.68; SHOT_DELAY=8; TIMEOUT=300; NO_BUILD=0; APP=""; OUT_DIR=""

fail() { echo "ERROR: $*" >&2; exit 1; }
need() { [[ $# -ge 2 ]] || fail "$1 needs a value"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --width)      need "$@"; WIDTH="$2"; shift 2 ;;
    --height)     need "$@"; HEIGHT="$2"; shift 2 ;;
    --scale)      need "$@"; SCALE="$2"; shift 2 ;;
    --zoom)       need "$@"; ZOOM="$2"; shift 2 ;;
    --car)        need "$@"; CAR="$2"; shift 2 ;;
    --shot-delay) need "$@"; SHOT_DELAY="$2"; shift 2 ;;
    --timeout)    need "$@"; TIMEOUT="$2"; shift 2 ;;
    --no-build)   NO_BUILD=1; shift ;;
    --app)        need "$@"; APP="$2"; shift 2 ;;
    --out)        need "$@"; OUT_DIR="$2"; shift 2 ;;
    -h|--help)    sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)            fail "unknown option: $1 (see --help)" ;;
  esac
done

RES_X=$((WIDTH * SCALE)); RES_Y=$((HEIGHT * SCALE))

if [[ -n "$APP" ]]; then
  [[ -d "$APP/Contents/MacOS" ]] || fail "not an app bundle: $APP"
  LAUNCH="$(find "$APP/Contents/MacOS" -maxdepth 1 -type f -perm +111 | head -1)"
  [[ -n "$LAUNCH" ]] || fail "no executable in $APP/Contents/MacOS"
  LAUNCH_ARGS=()
else
  LAUNCH="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
  BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
  [[ -x "$LAUNCH" ]] || fail "UnrealEditor not found at: $LAUNCH (set UE_ROOT)"
  [[ -f "$UPROJECT" ]] || fail "Project not found: $UPROJECT"
  if [[ $NO_BUILD -eq 0 ]]; then
    echo "==> Building DirectorsCutEditor (Mac Development)"
    "$BUILD_SH" DirectorsCutEditor Mac Development "-project=$UPROJECT" || fail "Build failed (pass --no-build to use the binaries that are already there)"
  fi
  # Keeps this game instance from starting the editor's MCP server (it would fight an open editor for the port).
  LAUNCH_ARGS=("$UPROJECT" -game
    "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False")
fi

[[ -n "$OUT_DIR" ]] || OUT_DIR="$PROJECT_DIR/Saved/Screenshots/thumbnail-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
LOG="$OUT_DIR/thumbnail.log"

# Same per-launch overrides as the screenshot script: no FPS counter, debug view, high contrast, focus pause or sound.
SETTINGS="-ini:GameUserSettings:"
for kv in ShowFPS=False DebugView=False HighContrast=False PauseOnFocusLoss=False MuteAll=True; do
  SETTINGS+="[DirectorsCut.Settings]:$kv,"
done
SETTINGS="${SETTINGS%,}"

echo "==> Rendering ${RES_X}x${RES_Y} (title + house, zoom $ZOOM)"
# The tag only makes this launch findable with pgrep: the editor launcher returns at once while the game runs on.
TAG="-DCShotRun=$$-thumbnail"
"$LAUNCH" ${LAUNCH_ARGS[@]+"${LAUNCH_ARGS[@]}"} -windowed "-ResX=$RES_X" "-ResY=$RES_Y" -nosplash \
  -DCSaveSlot=DCShot -DCThumbnail "-DCThumbCar=$CAR" "-DCShotDelay=$SHOT_DELAY" "-DCShotDir=$OUT_DIR" "$SETTINGS" \
  "-DCShots=thumbnail_full:thumb@$ZOOM" "-abslog=$LOG" "$TAG" >/dev/null 2>&1 &
sleep 3
T0=$(date +%s)
while pgrep -f -- "$TAG" >/dev/null; do
  if (( $(date +%s) - T0 > TIMEOUT )); then
    echo "    still running after $TIMEOUT s: stopping it (see $LOG)"
    pkill -f -- "$TAG" || true
    break
  fi
  sleep 2
done

FULL="$OUT_DIR/thumbnail_full.png"
[[ -f "$FULL" ]] || fail "no screenshot was taken; see $LOG"

# Shrink to the exact final size. The window can come out a few pixels off what was asked for, so fit the
# height and centre-crop the width.
OUT="$OUT_DIR/thumbnail.png"
sips --resampleHeight "$HEIGHT" "$FULL" --out "$OUT" >/dev/null
sips --cropToHeightWidth "$HEIGHT" "$WIDTH" "$OUT" >/dev/null
read -r W H < <(sips -g pixelWidth -g pixelHeight "$OUT" | awk '/pixelWidth/{w=$2} /pixelHeight/{h=$2} END{print w, h}')
[[ "$W" == "$WIDTH" && "$H" == "$HEIGHT" ]] || fail "thumbnail is ${W}x${H}, expected ${WIDTH}x${HEIGHT}: $OUT"

echo "Done: $OUT (${W}x${H}); full-size render: $FULL"
