# Director's Cut: v2 playtest (realtime)

## Launch
- **Editor:** open `DirectorsCut.uproject` (rebuild if prompted), then press **Play**.
- **Standalone:**
  ```
  "/Users/Shared/Epic Games/UE_5.9/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/DirectorsCut.uproject" -game -windowed -ResX=1920 -ResY=1080
  ```
- **Useful flags:**
  - `-DCSeed=123`: skip the title and start a seeded run.
  - `-DCTimeScale=5`: run the clock 5× faster.
  - `-DCHour=21`: jump to night.
  - `-DCContinue`: resume the saved run.
- **Audio is muted** (`Config/DefaultGame.ini` → `bMuteAll`). Add `-DCUnmute` to hear it.

## Controls
- **Left click:** stations and UI. **Right-drag / WASD / screen edge:** pan. **Scroll:** zoom.
- **Space:** missions. **Esc:** back / pause (pausing stops the clock). **C / G / R / B:** crew / gear / recruits / build.

## Loop
1. Missions (Space) → pick a card → fill the crew slots (or use Auto-fill) → set your cut → **Start**.
2. Crew walk out through the garage and come back when the timer ends.
3. Check results from the bell (Reports).
4. Spend cash on rooms (Build). Rooms take real time to build.
5. Wages are paid at dawn (06:00). If you go broke, the run is over.
6. Every 3 successes you pick a perk.
7. Tier 3 needs the Ops Room. The Big Score wins the run.

## What to look for
- **Pacing:** first mission at 1–2 min, later missions up to 20 min. Is the waiting fun?
- **Clarity:** are the icons readable without text? Do the threats and counters make sense?
- **Day/night:** the light through the windows, night crew in the bunks.
- **Economy:** pressure at payday.
- Anything confusing, broken or ugly.

## Known issues
- The light beams are fake meshes and sometimes clip props.
- The street-lamp pools are mostly hidden behind the Garage wall.
- The cars are built from primitives.
- Bunks only exist once the Bunkroom is built (before then, crew sleep on the floor).
