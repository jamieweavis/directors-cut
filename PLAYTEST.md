# Director's Cut: v3 playtest (rural house)

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
- **Left click:** stations and UI. **Right-drag / WASD:** pan (screen-edge pan is off by default; Options → Gameplay). **Scroll:** zoom.
- **Space:** missions. **Esc:** back / pause (pausing stops the clock). **C / G / R / B:** crew / gear / recruits / build.

## Loop
1. Missions (Space) → pick a card → fill the crew slots (or use Auto-fill) → set your cut → **Start**.
2. Crew walk out to a car in the yard and drive off along the lane; the car comes back as the job ends. One car per mission slot.
3. Check results from the bell (Reports).
4. Spend cash on new wings (Expand). The house physically grows: overgrown plot → timber frame → finished wing.
5. Wages are paid at dawn (06:00). If you go broke, the run is over.
6. Every 3 successes you pick a perk.
7. Tier 3 needs the Ops Room. The Big Score wins the run.

## Menus
- Title: Continue, New run (pick one of 3 slots), Load, Options, leaderboard of past runs.
- Pause (Esc): Save to a slot, Load, Options. Finished runs stay in their slot and can be saved/copied.
- Options: Audio, Graphics (Apply/Revert, 10 s keep-settings confirm), Accessibility, Gameplay.

## What to look for
- **Pacing:** first mission at 1–2 min, later missions up to 20 min. Is the waiting fun?
- **Clarity:** are the icons readable without text? Do the threats and counters make sense?
- **Day/night:** sun across the meadow, golden/blue hour, coloured lights at night (festoons, campfire, lanterns, fireflies, house interior).
- **Expiry:** leads/recruits turn amber < 60 s and red + pulsing < 20 s, with a dock badge.
- **Economy:** pressure at payday.
- Anything confusing, broken or ugly.

## Known issues
- Crew can clip through furniture standing on a room's open front when walking out.
- Rooms are tight after the 0.65x shrink; a few props overlap (pool table, sofa, kitchenette); the workshop pegboard can hide the house front.
- Ambience is still the old city day/night loops (audio is muted by default).
- UI scale doesn't scale tooltips.
