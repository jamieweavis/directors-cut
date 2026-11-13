# Director's Cut — α playtest

## Launch
- **Editor:** open `DirectorsCut.uproject` (rebuild if prompted). `L_Hideout` is the startup map. Press **Play**; "Selected Viewport" or "New Editor Window" both work.
- **Standalone** (no editor UI):
  ```
  "/Users/Shared/Epic Games/UE_5.9/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/DirectorsCut.uproject" -game -windowed -ResX=1920 -ResY=1080
  ```
  Add `-DCSeed=123` to skip the title and play a specific seed.

## Controls
- **Left click:** stations (desk, job board, blueprint table, projector…) and UI
- **Right-drag / WASD:** pan · **Scroll:** zoom
- **Esc:** back / menu · **Space:** Roll Camera (end the night)
- The top-bar tabs (JOBS / CREW / GEAR / DESK / BUILD / MENU) open the same panels as the stations.

## Loop
JOBS → pick a job → cast the required roles, set your cut → COMMIT → ROLL CAMERA → watch the Dailies → Continue. Build rooms from BUILD (the blueprint table); each one gets a lights-on reveal. A perk draft comes every 3 successful heists.

## What to look for
Fun and clarity in the first 5 minutes; whether the "Why this %" panel makes sense; balance (cash pressure, greedy-cut refusals, loyalty); room reveals; camera feel; anything confusing, broken or ugly.

## Known issues (α)
- Wave 1 scene snapshot: Agent D is still polishing lighting and rooms.
- Criminal pawns' accent rim reads too gold/bright.
- A couple of pawns can wander outside the office edge.
- The red Lumen exposure warning appears on screen in dev builds.
- The Workbench always shows 3 gear slots (the sim allows 2 until the Workshop is built).
- The audio is fully procedural and hasn't been listened to by a human yet. Please tell us how it sounds!
- Balance is first-pass (from Agent B's Monte-Carlo). Low-loyalty crew treat a 30% cut as greedy.
