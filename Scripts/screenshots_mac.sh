#!/usr/bin/env bash
# Takes the store / README screenshots of Director's Cut on macOS.
#
#   Scripts/screenshots_mac.sh                        # every scene, 1920x1080
#   Scripts/screenshots_mac.sh --only menu,late       # just those scenes
#   Scripts/screenshots_mac.sh --list                 # show the scenes and shots
#   Scripts/screenshots_mac.sh --width 2560 --height 1440
#
# The game is launched once per scene. Each launch puts the game in the state it needs with the
# dev flags in Source/DirectorsCut/Core/DCDevTools.h (a bot plays the run forward, the camera and
# panels are set per shot), saves the PNGs and quits by itself. Nothing is clicked or typed, so
# leave the mouse alone while a game window is open.
#
# Output: Saved/Screenshots/<timestamp>/*.png (a new folder every time), logs in its logs/ folder.
#
# Your own saves and leaderboard are not touched: the runs save under the "DCShot" prefix. The
# title screen shows a sample leaderboard and sample save slots.
#
# By default the game runs from source through the editor (UnrealEditor -game), which is built
# first. To shoot a packaged build instead pass --app; it must be a Development build
# (Scripts/package_mac.sh --dev), because a Shipping build ignores the settings overrides and the
# sample leaderboard.
#
# Options:
#   --only a,b        scene names (see --list); default = all
#   --list            print the scenes and their shots, then exit
#   --width N         default 1920
#   --height N        default 1080
#   --seed N          run seed for the gameplay scenes (same seed = same crew, jobs and house); default 7
#   --shot-delay S    seconds before a scene starts shooting (raise it if shaders are still compiling); default 8
#   --timeout S       a scene that runs longer than this is stopped; default 600
#   --fullscreen      default is a window; use this if the window does not fit your display
#   --no-build        skip building the editor target
#   --app PATH        packaged Development build to shoot instead (e.g. Packaged/Mac/DirectorsCut.app)
#   --out DIR         default: Saved/Screenshots/<timestamp>
# Engine: set UE_ROOT to override the default install location.

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPROJECT="$PROJECT_DIR/DirectorsCut.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.9}"

ONLY=""; LIST=0; WIDTH=1920; HEIGHT=1080; SEED=7; SHOT_DELAY=8; TIMEOUT=600
FULLSCREEN=0; NO_BUILD=0; APP=""; OUT_DIR=""

fail() { echo "ERROR: $*" >&2; exit 1; }
need() { [[ $# -ge 2 ]] || fail "$1 needs a value"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --only)       need "$@"; ONLY="$2"; shift 2 ;;
    --list)       LIST=1; shift ;;
    --width)      need "$@"; WIDTH="$2"; shift 2 ;;
    --height)     need "$@"; HEIGHT="$2"; shift 2 ;;
    --seed)       need "$@"; SEED="$2"; shift 2 ;;
    --shot-delay) need "$@"; SHOT_DELAY="$2"; shift 2 ;;
    --timeout)    need "$@"; TIMEOUT="$2"; shift 2 ;;
    --fullscreen) FULLSCREEN=1; shift ;;
    --no-build)   NO_BUILD=1; shift ;;
    --app)        need "$@"; APP="$2"; shift 2 ;;
    --out)        need "$@"; OUT_DIR="$2"; shift 2 ;;
    -h|--help)    sed -n '2,37p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)            fail "unknown option: $1 (see --help)" ;;
  esac
done

# --- Scenes -------------------------------------------------------------------
# Shots are "name:focus[:panel]" (see DCDevTools.h). focus@Z sets the zoom: 0 = whole site .. 1 = closest.
# -DCAutoPlay = run-seconds the bot plays before the shots; -DCHour = clock time the bot stops at.
ALL_SCENES="menu early mid late night victory gameover"

scene() {   # sets ABOUT, ARGS (array) and SHOTS (comma list) for scene $1
  case "$1" in
    menu)
      ABOUT="Title screen with a full leaderboard, then each options tab"
      ARGS=(-DCUIFakeData "-DCUIView=-,options.general,options.graphics,options.access,options.gameplay")
      SHOTS="title:wide:Title,options_audio:wide:Title,options_graphics:wide:Title,options_accessibility:wide:Title,options_gameplay:wide:Title" ;;
    early)
      ABOUT="Day 2: the office, two crew, the first jobs"
      ARGS=("-DCSeed=$SEED" -DCAutoPlay=600 -DCHour=10)
      SHOTS="early_overview:wide,early_office:Office@0.6,early_jobs:wide:Missions,early_recruits:wide:Recruits" ;;
    mid)
      ABOUT="Around day 5: a few rooms, a handful of crew"
      ARGS=("-DCSeed=$SEED" -DCAutoPlay=6000 -DCHour=15)
      SHOTS="mid_overview:wide,mid_house:house@0.5,mid_yard:yard@0.35,mid_crew:house@0.3:Crew,mid_build:wide:Build,mid_workbench:wide:Workbench" ;;
    late)
      ABOUT="Around day 14: the whole house, a full crew, the Big Score on the board"
      ARGS=("-DCSeed=$SEED" -DCAutoPlay=18000 -DCHour=17)
      SHOTS="late_overview:wide,late_house:house@0.5,late_house_close:house@0.7,late_ops_room:OpsRoom@0.75,late_lounge:Lounge@0.75,late_crew_closeup:crew@0.8,late_jobs:wide:Missions,late_planner:wide:MissionPlanner,late_crew:house@0.3:Crew,late_reports:wide:Reports" ;;
    night)
      ABOUT="Late game after dark"
      ARGS=("-DCSeed=$SEED" -DCAutoPlay=12000 -DCHour=22)
      SHOTS="night_overview:wide@0.15,night_house:house@0.55,night_yard:yard@0.4" ;;
    victory)
      ABOUT="End screen: the Big Score pulled off"
      ARGS=("-DCSeed=$SEED" -DCAutoPlay=18000 -DCEndRun=win)
      SHOTS="end_victory:wide" ;;
    gameover)
      ABOUT="End screen: went broke"
      ARGS=("-DCSeed=$((SEED + 4))" -DCAutoPlay=2500 -DCEndRun=lose)
      SHOTS="end_gameover:wide" ;;
    *) return 1 ;;
  esac
}

shot_names() { tr ',' '\n' <<< "$1" | cut -d: -f1; }   # "a:wide,b:house@0.5:Crew" -> a, b (one per line)

if [[ $LIST -eq 1 ]]; then
  for s in $ALL_SCENES; do
    scene "$s"
    printf '%-9s %s\n' "$s" "$ABOUT"
    shot_names "$SHOTS" | sed 's/^/            /; s/$/.png/'
  done
  exit 0
fi

SCENES="$ALL_SCENES"
if [[ -n "$ONLY" ]]; then
  SCENES="$(tr ',' ' ' <<< "$ONLY")"
  for s in $SCENES; do scene "$s" || fail "unknown scene '$s' (scenes: $ALL_SCENES)"; done
fi
COUNT=$(wc -w <<< "$SCENES" | tr -d ' ')

# --- What to launch -----------------------------------------------------------
if [[ -n "$APP" ]]; then
  [[ -d "$APP/Contents/MacOS" ]] || fail "not an app bundle: $APP"
  LAUNCH="$(find "$APP/Contents/MacOS" -maxdepth 1 -type f -perm +111 | head -1)"
  [[ -n "$LAUNCH" ]] || fail "no executable in $APP/Contents/MacOS"
  LAUNCH_ARGS=()
  echo "==> Shooting the packaged build: $APP"
  echo "    (it must be a Development build: Scripts/package_mac.sh --dev)"
else
  LAUNCH="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
  BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
  [[ -x "$LAUNCH" ]] || fail "UnrealEditor not found at: $LAUNCH (set UE_ROOT)"
  [[ -f "$UPROJECT" ]] || fail "Project not found: $UPROJECT"
  if [[ $NO_BUILD -eq 0 ]]; then
    echo "==> Building DirectorsCutEditor (Mac Development)"
    "$BUILD_SH" DirectorsCutEditor Mac Development "-project=$UPROJECT" || fail "Build failed (pass --no-build to shoot with the binaries that are already there)"
  fi
  # The last one keeps this game instance from starting the editor's MCP server (it would fight an open editor for the port).
  LAUNCH_ARGS=("$UPROJECT" -game
    "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False")
fi

[[ -n "$OUT_DIR" ]] || OUT_DIR="$PROJECT_DIR/Saved/Screenshots/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR/logs"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
LOG_DIR="$OUT_DIR/logs"

# Player settings that would spoil a shot are overridden for these launches only (nothing is written to disk):
# no FPS counter, no debug overlay, no high-contrast UI, no auto-pause when the window loses focus, no sound.
SETTINGS="-ini:GameUserSettings:"
for kv in ShowFPS=False DebugView=False HighContrast=False PauseOnFocusLoss=False MuteAll=True; do
  SETTINGS+="[DirectorsCut.Settings]:$kv,"
done
SETTINGS="${SETTINGS%,}"

WINDOW=-windowed
[[ $FULLSCREEN -eq 1 ]] && WINDOW=-fullscreen
COMMON=("$WINDOW" "-ResX=$WIDTH" "-ResY=$HEIGHT" -nosplash -DCSaveSlot=DCShot "-DCShotDelay=$SHOT_DELAY" "-DCShotDir=$OUT_DIR" "$SETTINGS")

# --- Shoot --------------------------------------------------------------------
START=$(date +%s)
MISSING=0; TAKEN=0; N=0
for s in $SCENES; do
  N=$((N + 1))
  scene "$s"
  echo
  echo "==> [$N/$COUNT] $s: $ABOUT"
  LOG="$LOG_DIR/$s.log"
  # The game ignores this flag; it only makes this launch findable with pgrep. The editor launcher
  # returns at once while the game keeps running, so the wait is on the process, not the command.
  TAG="-DCShotRun=$$-$s"
  "$LAUNCH" ${LAUNCH_ARGS[@]+"${LAUNCH_ARGS[@]}"} "${COMMON[@]}" "${ARGS[@]}" "-DCShots=$SHOTS" "-abslog=$LOG" "$TAG" >/dev/null 2>&1 &
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

  for name in $(shot_names "$SHOTS"); do
    if [[ -f "$OUT_DIR/$name.png" ]]; then TAKEN=$((TAKEN + 1)); echo "    $name.png"
    else MISSING=$((MISSING + 1)); echo "    MISSING $name.png"; fi
  done
  if [[ -f "$LOG" ]]; then
    grep -E '\[AutoPlay\] t=|\[DCShots\] unknown focus' "$LOG" | sed -E 's/^.*(\[(AutoPlay|DCShots)\])/    \1/' || true
  fi
done

ELAPSED=$(( $(date +%s) - START ))
echo
echo "Done in $((ELAPSED / 60))m $((ELAPSED % 60))s: $TAKEN screenshots in $OUT_DIR"
if [[ $MISSING -gt 0 ]]; then
  echo "$MISSING shot(s) missing; logs are in $LOG_DIR"
  exit 1
fi
