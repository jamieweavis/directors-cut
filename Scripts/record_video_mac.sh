#!/usr/bin/env bash
# Records the jam-submission video footage of Director's Cut on macOS.
#
#   Scripts/record_video_mac.sh                       # the whole reel (Scripts/video_reel.txt): 60 s of clips, ~1.5 min to record
#   Scripts/record_video_mac.sh --list                # show the clips in the reel
#   Scripts/record_video_mac.sh --reel my_reel.txt    # another shot list
#   Scripts/record_video_mac.sh --no-record           # play the reel without filming it (to check it, or to film it yourself)
#
# The game is launched once and plays the shot list in Scripts/video_reel.txt by itself: ten seconds on the title
# screen, then it jumps through one seeded run (day 1, day 2, day 5, a night, day 14, the Big Score), holding each
# clip for a few seconds, some with a slow camera move, some with the UI open and buttons being "pressed". Every clip
# fades in from black and out again; the run jumps forward behind the black. Edit video_reel.txt to change the shots.
#
# ffmpeg films the game window while that happens (brew install ffmpeg). The first time, macOS asks for permission:
# allow your terminal under System Settings > Privacy & Security > Screen & System Audio Recording, then run again.
# Leave the mouse and keyboard alone and keep the game window in front and uncovered until it quits.
#
# Output: Saved/Videos/<timestamp>/ (a new folder every time)
#   reel_full.mp4       the whole recording (fades included)
#   Clips/NN_name.mp4   each clip's clean part, cut from it, ready to edit
#   reel_cues.csv       where each clip starts and ends in reel_full.mp4 (seconds)
#   logs/               game and ffmpeg logs
#
# Your own saves and leaderboard are not touched: the run saves under the "DCShot" prefix, and the title screen
# shows a sample leaderboard and sample save slots.
#
# By default the game runs from source through the editor (UnrealEditor -game), which is built first. To film a
# packaged build pass --app; it must be a Development build (Scripts/package_mac.sh --dev), because a Shipping build
# ignores the dev flags the reel needs.
#
# Options:
#   --reel FILE       shot list; default Scripts/video_reel.txt
#   --list            print the clips in the reel, then exit
#   --width N         video size, and the game window's size in points when it fits on the main display
#   --height N        (otherwise the biggest 16:9 window that fits); default 1920x1080
#   --fullscreen      fill the main display instead (macOS then lets screen capture miss frames: choppier video)
#   --fps N           recording frame rate (the game is capped to it too); default 60
#   --bitrate R       recording bitrate; default 50M
#   --seed N          run seed (overrides the reel's "newrun" lines)
#   --no-record       don't film; just play the reel
#   --no-split        don't cut the clips out afterwards
#   --no-build        skip building the editor target
#   --app PATH        packaged Development build to film instead (e.g. Packaged/Mac/DirectorsCut.app)
#   --out DIR         default: Saved/Videos/<timestamp>
#   --timeout S       stop the game if it runs longer than this; default 1200
# Engine: set UE_ROOT to override the default install location.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/DirectorsCut.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.9}"

REEL="$PROJECT_DIR/Scripts/video_reel.txt"
LIST=0; WIDTH=1920; HEIGHT=1080; FULLSCREEN=0; FPS=60; BITRATE=50M; SEED=""
NO_RECORD=0; NO_SPLIT=0; NO_BUILD=0; APP=""; OUT_DIR=""; TIMEOUT=1200

fail() { echo "ERROR: $*" >&2; exit 1; }
need() { [[ $# -ge 2 ]] || fail "$1 needs a value"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reel)       need "$@"; REEL="$2"; shift 2 ;;
    --list)       LIST=1; shift ;;
    --width)      need "$@"; WIDTH="$2"; shift 2 ;;
    --height)     need "$@"; HEIGHT="$2"; shift 2 ;;
    --fullscreen) FULLSCREEN=1; shift ;;
    --fps)        need "$@"; FPS="$2"; shift 2 ;;
    --bitrate)    need "$@"; BITRATE="$2"; shift 2 ;;
    --seed)       need "$@"; SEED="$2"; shift 2 ;;
    --no-record)  NO_RECORD=1; shift ;;
    --no-split)   NO_SPLIT=1; shift ;;
    --no-build)   NO_BUILD=1; shift ;;
    --app)        need "$@"; APP="$2"; shift 2 ;;
    --out)        need "$@"; OUT_DIR="$2"; shift 2 ;;
    --timeout)    need "$@"; TIMEOUT="$2"; shift 2 ;;
    -h|--help)    sed -n '2,50p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)            fail "unknown option: $1 (see --help)" ;;
  esac
done

[[ -f "$REEL" ]] || fail "reel not found: $REEL"
REEL="$(cd "$(dirname "$REEL")" && pwd)/$(basename "$REEL")"

# Clips in the reel: "name<TAB>camera<TAB>rest" per line.
reel_clips() { sed -e 's/#.*//' "$REEL" | awk '$1 == "clip" { rest = ""; for (i = 4; i <= NF; i++) rest = rest " " $i; printf "%s\t%s\t%s\n", $2, $3, rest }'; }

if [[ $LIST -eq 1 ]]; then
  N=0
  while IFS=$'\t' read -r name cam rest; do
    N=$((N + 1)); printf '%02d  %-20s %-28s%s\n' "$N" "$name" "$cam" "$rest"
  done < <(reel_clips)
  exit 0
fi

if [[ $NO_RECORD -eq 0 ]]; then
  command -v ffmpeg >/dev/null || fail "ffmpeg not found: brew install ffmpeg (or pass --no-record and film it yourself)"
fi

# --- What to launch -----------------------------------------------------------
if [[ -n "$APP" ]]; then
  [[ -d "$APP/Contents/MacOS" ]] || fail "not an app bundle: $APP"
  LAUNCH="$(find "$APP/Contents/MacOS" -maxdepth 1 -type f -perm +111 | head -1)"
  [[ -n "$LAUNCH" ]] || fail "no executable in $APP/Contents/MacOS"
  LAUNCH_ARGS=()
  echo "==> Filming the packaged build: $APP"
  echo "    (it must be a Development build: Scripts/package_mac.sh --dev)"
else
  LAUNCH="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
  BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
  [[ -x "$LAUNCH" ]] || fail "UnrealEditor not found at: $LAUNCH (set UE_ROOT)"
  [[ -f "$UPROJECT" ]] || fail "Project not found: $UPROJECT"
  if [[ $NO_BUILD -eq 0 ]]; then
    echo "==> Building DirectorsCutEditor (Mac Development)"
    "$BUILD_SH" DirectorsCutEditor Mac Development "-project=$UPROJECT" || fail "Build failed (pass --no-build to use the binaries that are already there)"
  fi
  # The last one keeps this game instance from starting the editor's MCP server (it would fight an open editor for the port).
  LAUNCH_ARGS=("$UPROJECT" -game
    "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False")
fi

[[ -n "$OUT_DIR" ]] || OUT_DIR="$PROJECT_DIR/Saved/Videos/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR/logs"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
LOG_DIR="$OUT_DIR/logs"
FULL="$OUT_DIR/reel_full.mp4"

# Player settings that would spoil the video are overridden for this launch only (nothing is written to disk):
# no FPS counter, no debug overlay, no high-contrast UI, no auto-pause when the window loses focus, no edge scrolling, no sound.
SETTINGS="-ini:GameUserSettings:"
for kv in ShowFPS=False DebugView=False HighContrast=False PauseOnFocusLoss=False EdgeScroll=False MuteAll=True; do
  SETTINGS+="[DirectorsCut.Settings]:$kv,"
done
SETTINGS="${SETTINGS%,}"

# Displays, as "x y width height scale" in points with the origin at the top left of the main display (the game's
# coordinates), main display first.
screens() {
  osascript -l JavaScript -e 'ObjC.import("AppKit"); var s = $.NSScreen.screens, o = [], mainH = s.objectAtIndex(0).frame.size.height;
    for (var i = 0; i < s.count; i++) { var f = s.objectAtIndex(i).frame;
      o.push([f.origin.x, mainH - f.origin.y - f.size.height, f.size.width, f.size.height, s.objectAtIndex(i).backingScaleFactor].join(" ")); }
    o.join("\n")'
}
read -r _ _ MAIN_W MAIN_H _ < <(screens | head -1)

# The window has to fit on the main display with its title bar and the menu bar; if it doesn't, the game gets the
# biggest 16:9 window that does. The video is scaled to --width x --height either way. (A window rather than
# fullscreen: macOS hands a fullscreen game the display directly and screen capture then misses most of its frames.)
GAME_W=$WIDTH; GAME_H=$HEIGHT
if [[ $FULLSCREEN -eq 0 ]] && (( GAME_W > MAIN_W - 20 || GAME_H > MAIN_H - 90 )); then
  GAME_H=$(( (MAIN_H - 90) / 18 * 18 )); GAME_W=$(( GAME_H * 16 / 9 ))
  (( GAME_W > MAIN_W - 20 )) && { GAME_W=$(( (MAIN_W - 20) / 32 * 32 )); GAME_H=$(( GAME_W * 9 / 16 )); }
  echo "==> The main display (${MAIN_W}x${MAIN_H}) is too small for a ${WIDTH}x${HEIGHT} window: using ${GAME_W}x${GAME_H}"
fi
WINDOW=-windowed
[[ $FULLSCREEN -eq 1 ]] && { WINDOW=-fullscreen; GAME_W=$MAIN_W; GAME_H=$MAIN_H; }

# Without a recorder there is no go signal: the reel starts on its own.
WAIT=60
[[ $NO_RECORD -eq 1 ]] && WAIT=0
TAG="-DCReelRun=$$"   # ignored by the game; makes this launch findable with pgrep
GAME_ARGS=("$WINDOW" "-ResX=$GAME_W" "-ResY=$GAME_H" -nosplash -DCSaveSlot=DCShot -DCUIFakeData
  "-DCShotDir=$OUT_DIR" "-DCReel=$REEL" "-DCReelWait=$WAIT" "-ExecCmds=t.MaxFPS $FPS" "$SETTINGS" "-abslog=$LOG_DIR/game.log" "$TAG")
[[ -n "$SEED" ]] && GAME_ARGS+=("-DCReelSeed=$SEED")

FFMPEG_PID=""
cleanup() {
  [[ -n "$FFMPEG_PID" ]] && kill -TERM "$FFMPEG_PID" 2>/dev/null || true
  pkill -f -- "$TAG" 2>/dev/null || true
}
trap cleanup INT TERM

# --- Play ---------------------------------------------------------------------
CLIPS=$(reel_clips | wc -l | tr -d ' ')
echo "==> Playing $CLIPS clips from $(basename "$REEL") (leave the mouse alone until the game quits)"
START=$(date +%s)
"$LAUNCH" ${LAUNCH_ARGS[@]+"${LAUNCH_ARGS[@]}"} "${GAME_ARGS[@]}" >/dev/null 2>&1 &

if [[ $NO_RECORD -eq 0 ]]; then
  # The game writes its window rect once the loading screen has lifted (shaders can take a while the first time).
  echo "    waiting for the game window..."
  T0=$(date +%s)
  until [[ -s "$OUT_DIR/reel_window.txt" ]]; do
    pgrep -f -- "$TAG" >/dev/null || fail "the game quit before the reel started (see $LOG_DIR/game.log)"
    (( $(date +%s) - T0 > 600 )) && { cleanup; fail "the game never got past loading (see $LOG_DIR/game.log)"; }
    sleep 1
  done
  read -r WX WY WW WH WDPI WFULL < "$OUT_DIR/reel_window.txt"

  # Where macOS has the game's window (points, top-left origin), its client area (the window minus its title bar),
  # and the display it is on.
  GAME_PID=$(pgrep -f -- "$TAG" | head -1)
  read -r BX BY BW BH < <(osascript -l JavaScript -e 'ObjC.import("CoreGraphics");
    function run(argv) { var l = ObjC.castRefToObject($.CGWindowListCopyWindowInfo($.kCGWindowListOptionOnScreenOnly | $.kCGWindowListExcludeDesktopElements, 0)), best = "", area = 0;
      for (var i = 0; i < l.count; i++) { var w = l.objectAtIndex(i), b = w.objectForKey("kCGWindowBounds");
        if (ObjC.unwrap(w.objectForKey("kCGWindowOwnerPID")) != argv[0] || ObjC.unwrap(w.objectForKey("kCGWindowLayer")) != 0) continue;
        var r = ["X", "Y", "Width", "Height"].map(function (k) { return ObjC.unwrap(b.objectForKey(k)); });
        if (r[2] * r[3] > area) { area = r[2] * r[3]; best = r.join(" "); } }
      return best; }' "$GAME_PID")
  [[ -n "${BW:-}" ]] || { cleanup; fail "can't find the game's window"; }
  CLIENT_H=$(awk -v bh="$BH" -v h="$WH" -v d="$WDPI" 'BEGIN { c = h / d; if (c > bh || bh - c > 60) c = bh; printf "%d", c }')
  BY=$(( BY + BH - CLIENT_H )); BH=$CLIENT_H
  read -r SX SY SPW SPH SSCALE < <(screens | awk -v x="$BX" -v y="$BY" -v w="$BW" -v h="$BH" '
    { cx = x + w / 2; cy = y + h / 2; if (cx >= $1 && cx < $1 + $3 && cy >= $2 && cy < $2 + $4) { print; found = 1; exit } }
    END { if (!found) exit 1 }') || { cleanup; fail "can't tell which display the game window is on"; }
  PIX_W=$(awk -v a="$SPW" -v s="$SSCALE" 'BEGIN { printf "%d", a * s }')
  PIX_H=$(awk -v a="$SPH" -v s="$SSCALE" 'BEGIN { printf "%d", a * s }')

  # ffmpeg numbers the displays itself: film one frame from each until one is that size.
  DEV=""
  PROBE="$LOG_DIR/probe.png"
  while read -r D; do
    ffmpeg -hide_banner -loglevel error -y -f avfoundation -capture_cursor 0 -framerate "$FPS" -i "$D:none" -frames:v 1 "$PROBE" 2>>"$LOG_DIR/ffmpeg.log" || continue
    if [[ "$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0:s=x "$PROBE")" == "${PIX_W}x${PIX_H}" ]]; then DEV=$D; break; fi
  done < <( (ffmpeg -hide_banner -f avfoundation -list_devices true -i "" 2>&1 || true) | sed -nE 's/.*\[([0-9]+)\] Capture screen [0-9]+$/\1/p')
  [[ -n "$DEV" ]] || { cleanup; fail "can't film the screen. Allow your terminal under System Settings > Privacy & Security > Screen & System Audio Recording, then run again (see $LOG_DIR/ffmpeg.log)."; }

  # Window rect -> pixels of that display, even numbers for the encoder; then scaled to the video size.
  to_px() { awk -v v="$1" -v o="$2" -v s="$SSCALE" 'BEGIN { printf "%d", int((v - o) * s / 2 + 0.5) * 2 }'; }
  CX=$(to_px "$BX" "$SX"); CY=$(to_px "$BY" "$SY"); CW=$(to_px "$BW" 0); CH=$(to_px "$BH" 0)
  (( CX + CW > PIX_W )) && CW=$(( (PIX_W - CX) / 2 * 2 ))
  (( CY + CH > PIX_H )) && CH=$(( (PIX_H - CY) / 2 * 2 ))
  echo "    filming ${CW}x${CH} at ${CX},${CY} of a ${PIX_W}x${PIX_H} display, scaled to ${WIDTH}x${HEIGHT}"
  VF="crop=$CW:$CH:$CX:$CY,scale=$WIDTH:$HEIGHT:flags=lanczos:force_original_aspect_ratio=decrease,pad=$WIDTH:$HEIGHT:-1:-1:black,format=yuv420p"

  ffmpeg -hide_banner -nostdin -y -f avfoundation -capture_cursor 0 -capture_mouse_clicks 0 -framerate "$FPS" -i "$DEV:none" \
    -vf "$VF" -r "$FPS" -c:v h264_videotoolbox -b:v "$BITRATE" -maxrate "$BITRATE" -bufsize "$BITRATE" -g "$FPS" \
    -movflags +faststart "$FULL" > "$LOG_DIR/ffmpeg.log" 2>&1 &
  FFMPEG_PID=$!
  # Wait until frames are being written, then let the game start.
  for _ in $(seq 1 40); do
    grep -q "frame=" "$LOG_DIR/ffmpeg.log" 2>/dev/null && break
    kill -0 "$FFMPEG_PID" 2>/dev/null || { cleanup; fail "ffmpeg stopped (see $LOG_DIR/ffmpeg.log)"; }
    sleep 0.25
  done
  sleep 1
  touch "$OUT_DIR/reel_go"
  echo "==> Recording"
fi

T0=$(date +%s)
LAST=""
while pgrep -f -- "$TAG" >/dev/null; do
  if (( $(date +%s) - T0 > TIMEOUT )); then
    echo "    still running after $TIMEOUT s: stopping it"
    pkill -f -- "$TAG" || true
    break
  fi
  # Progress: the clip the game is on.
  NOW=$(grep -oE '\[Reel\] clip [^ ]+' "$LOG_DIR/game.log" 2>/dev/null | tail -1 | awk '{print $3}' || true)
  if [[ -n "$NOW" && "$NOW" != "$LAST" ]]; then
    N=$(grep -cE '\[Reel\] clip ' "$LOG_DIR/game.log" || true)
    echo "    [$N/$CLIPS] $NOW"
    LAST="$NOW"
  fi
  sleep 1
done

if [[ -n "$FFMPEG_PID" ]]; then
  sleep 0.5
  kill -TERM "$FFMPEG_PID" 2>/dev/null || true
  wait "$FFMPEG_PID" 2>/dev/null || true
  FFMPEG_PID=""
fi
grep -E '\[Reel\].*(did nothing|unknown|can.t|no reel_go)' "$LOG_DIR/game.log" 2>/dev/null | sed -E 's/^.*(\[Reel\])/    \1/' || true

if [[ $NO_RECORD -eq 1 ]]; then
  echo "Done (not recorded). Clip times: $OUT_DIR/reel_cues.csv"
  exit 0
fi
[[ -s "$FULL" ]] || fail "no recording was written (see $LOG_DIR/ffmpeg.log)"
[[ -s "$OUT_DIR/reel_cues.csv" ]] || fail "the game wrote no clip times (see $LOG_DIR/game.log)"

# --- Line the cues up with the video ------------------------------------------
# The game's clock starts at the go signal; the recording started a little before. The reel opens on black and the
# first clip fades in from it, so the end of that first black stretch in the video is the "sync" cue.
SYNC=$(awk -F, '$1 == "sync" { print $3; exit }' "$OUT_DIR/reel_cues.csv")
BLACK_END=$(ffmpeg -hide_banner -nostdin -t 90 -i "$FULL" -vf "blackdetect=d=0.3:pix_th=0.06" -an -f null - 2>&1 \
  | sed -nE 's/.*black_start:([0-9.]+).*black_end:([0-9.]+).*/\1 \2/p' | awk '$1 < 3 { print $2; exit }')
if [[ -n "$BLACK_END" && -n "$SYNC" ]]; then
  OFFSET=$(awk -v b="$BLACK_END" -v s="$SYNC" 'BEGIN { printf "%.3f", b - s }')
else
  OFFSET=1.0
  echo "    couldn't find the opening fade; assuming the game started 1 s into the recording"
fi

# reel_cues.csv: times in the video.
awk -F, -v o="$OFFSET" 'NR == 1 { print "kind,name,start,end"; next } { printf "%s,%s,%.3f,%.3f\n", $1, $2, $3 + o, $4 + o }' \
  "$OUT_DIR/reel_cues.csv" > "$OUT_DIR/reel_cues.tmp" && mv "$OUT_DIR/reel_cues.tmp" "$OUT_DIR/reel_cues.csv"

# --- Cut the clips --------------------------------------------------------------
CUT=0
if [[ $NO_SPLIT -eq 0 ]]; then
  echo "==> Cutting the clips"
  mkdir -p "$OUT_DIR/Clips"
  N=0
  while IFS=, read -r kind name start end; do
    [[ "$kind" == "clip" ]] || continue
    N=$((N + 1))
    # The whole clean part: the reel cuts hard (fade 0), so there is no fade to trim off.
    LEN=$(awk -v s="$start" -v e="$end" 'BEGIN { printf "%.3f", e - s }')
    OUT="$OUT_DIR/Clips/$(printf '%02d' "$N")_$name.mp4"
    if ffmpeg -hide_banner -nostdin -loglevel error -y -ss "$start" -i "$FULL" -t "$LEN" \
        -c:v h264_videotoolbox -b:v "$BITRATE" -pix_fmt yuv420p -an -movflags +faststart "$OUT"; then
      CUT=$((CUT + 1)); echo "    $(basename "$OUT")"
    else
      echo "    FAILED $(basename "$OUT")"
    fi
  done < <(tail -n +2 "$OUT_DIR/reel_cues.csv")
fi

ELAPSED=$(( $(date +%s) - START ))
echo
echo "Done in $((ELAPSED / 60))m $((ELAPSED % 60))s: $FULL"
[[ $NO_SPLIT -eq 0 ]] && echo "$CUT clips in $OUT_DIR/Clips"
echo "Clip times: $OUT_DIR/reel_cues.csv"
