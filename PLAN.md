# Director's Cut — Build Plan

> A top-down isometric 3D roguelike crime-management game. You are **the Director**. From a moody back office hidden inside an abandoned art-deco cinema, you recruit criminals, kit them out, set your cut, and send them on heists. Grow the empire or go broke trying.

**Engine:** Unreal Engine 5.9 (existing blank project `DirectorsCut.uproject`, Lumen + VSM + Substrate already enabled)
**Platform:** packaged macOS build (Apple Silicon, Xcode 27) first; Windows if we can get a Windows machine to package it (see §7)
**Tooling:** Claude Code with parallel sub-agents, the built-in Unreal MCP server (`http://127.0.0.1:8000/mcp`), UnrealBuildTool, and headless automation tests

---

## Progress: as of Fri Nov 13, ~18:15 PST

**Status: α playable** (tag `alpha` on `main`). Waves 0 and 1 are done and merged. Main builds and all 12 automation tests pass (9 Sim, 2 Audio, 1 Smoke). A bot autoplay of a seeded run works end to end on the real sim.

**Changes from the original plan:**
- **The MCP has no level, actor, Python or screenshot tools** (only Sequencer/UMG/PCG/Fab and similar). So the hideout is **built procedurally in C++ at runtime** (`World/Scene/ADCHideoutBuilder`, spawned by `ADCGameMode::StartPlay`). `L_Hideout` is an empty map.
- **Assets are authored headlessly:** `UnrealEditor-Cmd -run=pythonscript -script=… -AllowCommandletRendering` (Tools/*.py). This covers materials (D) and audio import (H).
- **Visual verification uses dev flags** (`Core/DCDevTools`): `-DCShots=name:focus[:panel]`, `-DCSeed`, `-DCAutoPlay=N`, `-DCBuildAll`, `-DCShotDir`.
- **Parallel-agent hygiene:**
  - Every editor or game run is wrapped in `lockf /private/tmp/claude-501/dc_ue_run.lock` and given its own `-abslog`, because the machine has 16 GB of RAM.
  - `bUseUnity = false` in Build.cs, because unity builds caused cross-agent name clashes.
  - Worktrees sometimes hold LFS pointers; fix with `git lfs checkout`.
- **Audio is 100% procedurally generated** (`Tools/audio_gen.py`), because kenney.nl blocked downloads. It's project-owned, with nothing to attribute. The WAVs are no longer staged in the package; only the imported assets ship.
- The data balance was tuned by Agent B (see `Content/Data/balance_check.py`): difficultyPerHeist 0.05, willSlope 3.0, refusal 0.45, payouts +25–40%. `schema.md` now ends with a Clarifications section covering how modifiers stack and the exact rules.
- Keep the DirectorsCut editor **closed** while building main. With it open, UBT writes hot-reload dylibs and the tests run stale code.

**Wave status:**
| Wave / agent | Status |
|---|---|
| 0 Foundation | ✅ contracts, stubs, greybox, dev tools, materials, map, smoke test |
| 1A Sim | ✅ merged: full §2.7 resolver, modifier stack (every kind), rooms, perks, retakes, Final Cut, Clinic, Lawyer, 9 tests |
| 1B Data | ✅ merged: all JSON, validate.py, balance_check.py |
| 1C UI | ✅ merged: every panel, Dailies reveal, perk draft, game over, credits, settings, tutorial, fonts (OFL/Apache) |
| 1D Scene | ✅ merged: 8-room diorama, 11 stations, doors, neon, practicals, post, procedural materials, L1–L3 dressing, scene-review camera (`-DCSceneShots`) |
| 1E World | ✅ merged: camera rig, input, hover/click, room reveal, pawns, departures/returns |
| 1H Audio | ✅ merged: 32 procedural sounds, music state machine, ambience, volumes, 2 tests (**not yet listened to by a human**) |
| 2 Integration | ✅ α built and screenshot-checked. Your playtest is next |
| 3 / 4 | ⏳ waiting. **Big changes incoming (per you) before Wave 3** |

**Known α issues:**
- Pawn accent rim reads too gold.
- Some pawns wander outside the office.
- **Performance:** 30–44 fps at 1600×900 (with other editors running); cheap fixes are a larger volumetric fog grid or fewer shadowed lights. Volumetric shafts barely show, so a fake beam cone stands in. The Garage/car is dark and boxy. Lounge pawns spawn against the pool table.
- **Instanced-mesh materials:** materials used on instanced meshes need the instanced-static-mesh usage flag (`M_DC_Emissive` and `M_DC_Glass` lack it).
- The Workbench always shows 3 gear slots (needs a per-criminal slot count in the API).
- Gear "unavailable" reasons are guessed (needs an `UnavailableReason` field on `FDCGearItem`).
- Low-loyalty crew treat the default 30% cut as greedy.
- The bot won 0 of 9 runs in AutoPlay200, which is input for balance work.
- Retakes after night end don't regenerate the board difficulty.

**Open contract requests:** `FDCGearItem::UnavailableReason`; a per-criminal gear slot count; `FDCSuccessBreakdown::BustedThreshold` + `bIntel`; a smarter DevTools autoplay bot (bonus roles).

---

## 0. Jam context: 2026 Epic AI MegaJam

Jam page: https://itch.io/jam/2026-epic-ai-megajam. The binding Official Rules are linked from that page and are worth a read before submitting.

| | |
|---|---|
| **Theme** | **"DIRECTOR'S CUT"**: *"Your vision, your edits, your final cut."* |
| **Jam window** | Nov 12 00:00 EST → **deadline Nov 15, 5:59 PM PST / 8:59 PM EST**. Late entries are disqualified |
| **Our internal deadline** | **Submit by Nov 15, 2:00 PM PST** (a 4-hour buffer) |
| **Must submit** | A **packaged** build (Win 10/11, macOS, Android or iOS; unpackaged projects are rejected), under 1 GB, zipped as `TeamName_GameName.zip`; a **30–60 s gameplay video**; team name, plus each member's name and Epic-linked email; the game name; the Fab/third-party assets used |
| **Optional, but worth doing** | A dev log (the **Glass Box** prize: we keep `DEVLOG.md` as we go); "Tools you used" |
| **Rules to respect** | No building before Nov 12 (we started Nov 13 ✔). Declare anything made earlier (only the blank project and MCP config). Licensed assets only, and list them. Custom gameplay beyond Epic's starter templates (✔: the template mannequin is only a placeholder body). No third-party IP |
| **Judging (1–5 each)** | Ambition & innovation · **Theme** · Fun · Gameplay · **Visuals** · **Level design & progression** · **Audio** |
| **Prizes we're aiming at** | Best Overall UE; **Solo Driver** (if you're solo); **Render Royalty** (visuals); **Glass Box** (dev log) |

**How the plan answers the judging criteria:**
- **Theme:** you're literally the Director, taking a cut. Heists are framed as *productions*, the crew as the *cast*, results as *dailies*. Retakes, a "Final Cut" finale and end credits starring your crew all play on the theme (§2.12). Directing AI agents to build it is the meta-layer, which the dev log covers.
- **Visuals:** the lit diorama hideout on a black void (§3).
- **Level design & progression:** the **hideout grows**. You unlock and build new rooms that light up in the void, plus Director perks, job tiers and a final heist (§2.10–2.12).
- **Audio:** music, ambience and UI SFX are **in the MVP**, not post-MVP (§2.13).

## 1. Goal and schedule

The jam submission **is** the MVP. Build a playable core loop first, then layer on progression, polish and audio, then package and submit. Times below are in PST.

| When | Milestone |
|---|---|
| **Fri Nov 13** (today) | Wave 0 foundation (about 2 h), then launch Wave 1 (6 parallel agents) |
| **Sat Nov 14, morning** | Wave 2 integration → **first playable (α)**, which **you playtest** |
| **Sat Nov 14, afternoon/evening** | Wave 3: balance, visual polish, audio import, and expansion rooms finished. Second playtest |
| **Sun Nov 15, 10:00** | **Feature freeze**. Bug fixes only from here |
| **Sun Nov 15, 10:00–14:00** | Wave 4: package, test the packaged build on a clean run, record the video, write the itch page and dev log, **submit** |
| **Sun Nov 15, 17:59** | Hard deadline |

Scope uses **MoSCoW**. If we're running late, cut the *Could* items first, then the *Should* items. *Must* items are never cut.

---

## 2. Game design (MVP scope)

### 2.1 Core loop (turn-based, one "night" per turn)

1. **Plan phase.** You're in the hideout. Click stations to:
   - Look at the **Job Board** (cork board with red string): 4 random jobs.
   - Pick a job, then **assign a crew** from the roster (the job's required roles must be filled).
   - **Kit out** each crew member at the **Workbench** (buy and equip gear).
   - Set your **Director's Cut** with a slider (10–90%, steps of 5%).
   - See a **live success-chance breakdown**.
   - Commit the job. You can run 1 crew per night at first, and more once you build the Garage (§2.10).
   - Spend cash on **building or upgrading rooms** (§2.10).
2. **Execute phase.** Click **"Roll Camera"** (end the night). The committed crews leave through the garage door. Each heist resolves with a short, tense reveal (the % bar fills, the dice land, the outcome stamp hits).
3. **Aftermath.** You take your cut of the payout. Criminals get XP and may level up. Injuries and arrests are applied. Upkeep is paid. The job board refreshes. Difficulty goes up.
4. **Game over** when you can't pay upkeep (cash < 0), or when you have no criminals left and can't afford a recruit. The run summary shows nights survived, heists completed, total take and best heist.

Every run is **seeded** (`FRandomStream`). The seed appears on the summary screen, so a run can be replayed.

### 2.2 Run start (randomised)

- Starting cash: **$25,000**
- Starting roster: **4 random criminals**, with at least 3 different types (so the first board is always playable)
- Starting job board: **4 random jobs** from tier 1. At least 2 of them must be doable with the starting roster.
- Recruit board (at the Director's desk): 3 random recruits with hire costs. It refreshes every 3 nights.

### 2.3 Stats

There are six stats, each from 1 to 100: **Driving, Muscle, Tech, Stealth, Charm, Finesse**.
Each criminal also has:
- **Level** (1–10) and **XP**
- **Luck** (0–10)
- **Loyalty** (0–100)
- **Status**: Available / On Job / Injured (n nights) / Arrested (gone)

### 2.4 Criminal types (6)

| Type | Primary stat | Secondary | Fantasy |
|---|---|---|---|
| **Wheelman** | Driving | Nerve→Muscle | Getaway, car boosts |
| **Muscle** | Muscle | Driving | Intimidation, stickups |
| **Hacker** | Tech | Stealth | Alarms, data, cameras |
| **Ghost** | Stealth | Finesse | Cat burglar, infiltration |
| **Face** | Charm | Tech | Cons, distractions, insiders |
| **Safecracker** | Finesse | Tech | Vaults, locks, demolition |

Each type has a stat template: primary 45–65, secondary 30–45, others 10–30, with random jitter. Names are generated as `First 'Nickname' Last` (for example *Vinnie "The Wrench" Caruso*). Each type has its own colour accent, which is used in the UI and on its pawn.

### 2.5 Jobs (8 templates for the MVP)

| Job | Required roles | Crew size | Key stats | Base payout (tier 1) |
|---|---|---|---|---|
| Corner Store Stickup | Muscle | 1–2 | Muscle, Driving | $4k–7k |
| Car Boost | Wheelman | 1–2 | Driving, Tech | $5k–9k |
| Data Siphon | Hacker | 1–2 | Tech, Stealth | $7k–11k |
| Jewellery Smash & Grab | Muscle + Wheelman | 2–3 | Muscle, Driving | $10k–16k |
| Gallery Lift | Ghost + Face | 2–3 | Stealth, Charm, Finesse | $14k–22k |
| Casino Con | Face + Hacker | 2–4 | Charm, Tech | $16k–26k |
| Armoured Truck | Muscle + Wheelman + Safecracker | 3–4 | Muscle, Driving, Finesse | $22k–34k |
| Bank Vault | Safecracker + Hacker + Wheelman | 3–5 | Finesse, Tech, Driving, Stealth | $30k–48k |

Each job has: `Tier`, `RequiredRoles[]`, `MinCrew/MaxCrew`, `StatWeights{}` (summing to 1), `BaseDifficulty`, `PayoutRange`, `XPReward`, `ArrestRisk`, and flavour text.

### 2.6 Gear (about 15 items, 3 slots per criminal: Tool / Outfit / Gadget)

Examples: *Crowbar* (+8 Muscle), *Lockpick Set* (+10 Finesse), *Laptop Rig* (+12 Tech), *Tailored Suit* (+12 Charm), *Blackout Suit* (+10 Stealth), *Tuned Ignition Kit* (+10 Driving), *Lucky Coin* (+2 Luck), *Police Scanner* (−20% arrest risk), *Thermal Lance* (+18 Finesse, −5 Stealth).

Gear is bought from a fence catalogue at the Workbench. It's permanent, but it's **lost if the wearer is arrested**.

### 2.7 Success formula (transparent and shown in the UI)

```
EffStat(c, s)   = Base(c, s) + LevelBonus(c, s) + GearBonus(c, s)
CrewStat(s)     = max_c EffStat(c, s) + 0.25 * Σ(others) EffStat(c, s)     // best specialist + support
CrewScore       = Σ_s Weight(job, s) * CrewStat(s)
Difficulty      = BaseDifficulty(job) * (1 + 0.10 * HeistsCompleted) * (1 + 0.05 * Tier)
Margin          = (CrewScore − Difficulty) / Difficulty
SkillChance     = clamp(0.55 + 0.75 * Margin, 0.05, 0.95)
RoleFitBonus    = +0.03 per crew member whose type matches a job role beyond the required ones (cap +0.09)

Willingness(c)  = clamp(1 − 2.0 * max(0, Cut − FairCut(c)), 0.2, 1.0)
FairCut(c)      = 0.30 + (Loyalty(c) − 50) / 500                         // 0.20 … 0.40
CrewWill        = mean Willingness(c)
WillFactor      = 0.60 + 0.40 * CrewWill                                   // a greedy Director can cost 40%
GenerosityBonus = +0.04 if Cut < min FairCut(c)

Success%        = clamp((SkillChance + RoleFitBonus + GenerosityBonus) * WillFactor, 0.02, 0.97)
LuckyBreak%     = 2% * Σ Luck(c)  → chance of a free re-roll after a failure ("Lucky break!")
```

- A criminal whose `Willingness < 0.35` **refuses** the job (the UI warns you before you commit).
- **Outcome bands** from the roll: *Clean* (roll < Success% × 0.6), *Messy* (success, but each crew member has a 25% chance of being injured for 1–2 nights), *Failed* (no payout, injury chance 50%, arrest chance = ArrestRisk × (1 − Margin)), *Busted* (a failed roll in the worst 10% band: arrest chance doubles).
- The **cut split**: the Director gets `Payout × Cut`. The crew's share isn't tracked as money; it feeds **Loyalty** instead (+loyalty if Cut ≤ FairCut, −loyalty if Cut is greedy).

The UI shows every term as a line in a "Why this %" breakdown, so players learn the system.

### 2.8 Levelling

- XP per heist: `XPReward × (Clean 1.0 / Messy 0.7 / Failed 0.25)`. The level curve is `100 × level^1.4`.
- On level up: +4 primary stat, +2 secondary, +1 to every other stat, and +1 Luck on even levels. There's a level-up flourish on the pawn and in the UI.
- Loyalty: +5 on success with a fair cut, −8 with a greedy cut, −3 on injury.

### 2.9 Difficulty scaling

Driven by **`HeistsCompleted`** (successful heists):
- Job difficulty multiplier: see the formula above.
- Job tier unlocks: tier 2 at 3 heists, tier 3 at 7, tier 4 at 12. Higher tiers pay ×1.6 per tier.
- Upkeep: `$300 × roster size + $1,000 × floor(HeistsCompleted / 4)` per night (your empire costs more to run as it grows).
- Arrest risk: +2% per 5 heists completed.
- Recruit costs scale with tier, but recruits roll better stats at higher tiers.

All the numbers live in **JSON data files** (§4.3), so balancing doesn't need recompiling.

### 2.10 Hideout expansion: rooms & upgrades (the progression spine)

The run starts with **only the Director's Office lit**. The rest of the cinema sits in the void as dark, dust-sheeted silhouettes marked with "Under Construction" tape. Building a room costs cash (and sometimes requires a number of heists completed). When the build finishes there's a **reveal**: the camera pans over, the lights flicker on one by one, the dust settles and the room's station becomes active. Each room has **3 upgrade levels**, and each level adds a visual change in the room (more props, better lights, a neon sign).

| Room | Priority | Unlock cost | Effect (level 1 → 3) |
|---|---|---|---|
| **Director's Office** | Must (start) | — | Job board (3 jobs → 4 → 5), the desk (finances, recruits), a small "fence crate" with 4 basic gear items |
| **The Lounge** (bunks & bar) | Must | $8k | Roster cap 4 → 6 → 8 → 10. +1 loyalty per night for idle criminals |
| **Workshop** | Must | $12k | Full gear catalogue and the 3rd gear slot. L2: tier-2 gear. L3: −20% gear prices |
| **Garage** | Must | $15k, 2 heists | **Run 2 crews per night** (base 1) → 3 → 4. Unlocks "heavy" jobs (Armoured Truck, Bank Vault). +4% success when a Wheelman is on the crew |
| **Screening Room** (the old auditorium) | Should | $25k, 5 heists | **Retakes**: 1 free re-roll of a failed heist per night (L3: 2). Unlocks the **Final Cut** job (§2.12) |
| **Back-Alley Clinic** | Should | $10k | Injuries heal twice as fast. L2: injured criminals can be patched up for cash. L3: −25% injury chance |
| **Server Closet** | Could | $14k | **Intel**: shows the exact arrest risk and the outcome bands on job cards. Hacker jobs +20% payout |
| **Lawyer's Office** | Could | $20k | Bail out arrested criminals within 1 night for cash. −15% arrest chance |

The base unlocks **more jobs, more crew capacity, more parallel heists and new features**. That gives the run a clear arc: from a one-room grifter to a whole cinema-sized empire. Upkeep also grows with each room built (+$200 per night per room), so expanding is a real economic decision.

### 2.11 Director perks (a roguelike draft)

Every **3 successful heists** you **pick 1 of 3 random perks**. Each run gets a different build. Around 15 perks for the jam, for example:
- **Method Acting:** Faces get +10 Charm.
- **Practical Effects:** Safecracker gear costs 30% less.
- **Union Rates:** everyone's FairCut is +5%.
- **Stunt Double:** your first arrest each run is prevented.
- **Dailies:** once per night, see a job's exact outcome before you commit.
- **Box Office:** +15% payout on tier 3+ jobs.
- **Typecasting:** +6% success when every crew member is a different type.
- **Ensemble Cast:** +3% success per crew member beyond the minimum.
- **Shoestring Budget:** −25% upkeep, −10% payouts.
- **Auteur:** your cut never lowers willingness below 0.6.
- **Casting Call:** recruits arrive at +1 level.
- **Hot Set:** +30% payout, +10% arrest risk.
- **Location Scout:** +1 job on the board.
- **Overtime:** criminals can do 2 jobs per night but lose 3 loyalty.
- **Lucky Charm:** +1 Luck for the whole roster.

### 2.12 Theme hooks, the win condition and the fail screen

- **Framing:** jobs are *productions* with a "script" (flavour text). Committing a crew is *casting*. Ending the night is **"Roll Camera"**. The resolution sequence is the **Dailies**, with a film-reel transition and a clapperboard per heist. Outcome stamps read **IT'S A WRAP** (Clean) / **NEEDS FIXING IN POST** (Messy) / **CUT!** (Failed) / **SHUT DOWN** (Busted).
- **Retakes** (from the Screening Room): re-shoot a failed heist with a new roll.
- **The Final Cut:** a legendary tier-4 job, *"The Premiere"* (robbing the Grand Palace Theatre on opening night). It unlocks once you have the Screening Room and 12 heists. It needs 5 crew, including Face, Ghost, Safecracker, Hacker and Wheelman. **Completing it wins the run**, and you get **end credits**: your surviving crew listed as "Starring…", the arrested ones as "In Memoriam", plus stats.
- **Game over:** a "CANCELLED" title card, burnt film, the run summary and the seed.
- **Endless option** after a win, as a stretch goal.

### 2.13 Audio (in the MVP, since it's a judging category)

- **Music:** a noir jazz loop for the planning phase, a tense percussive cue for the Dailies, and stingers for wrap, cut, level-up, room-built and perk-pick.
- **Ambience:** rain on the window, a projector whirr, a faint street siren now and then, an electrical neon buzz near the sign.
- **UI SFX:** typewriter clacks, paper slides, a rubber-stamp thunk, cash register/counter ticks, a clapperboard snap, and a dice/film-sprocket roll.
- **Source:** licensed free or jam-partner packs (**WeLoveIndies is non-commercial only**, which is fine for the jam, and we'd note it), Fab free audio, or AI-generated audio from a commercial-friendly tool. Every source goes in the asset list.
- **Implementation:** `UDCAudioSubsystem` in C++ (a music state machine with crossfades, plus SFX by name through soft paths). Simple MetaSounds/Sound Classes, and a master/music/SFX volume setting.

---

## 3. Visual & art direction

### 3.1 The space: the Director's office in an abandoned art-deco cinema

A **modular cutaway diorama**: the two walls facing the camera are removed (or dropped to knee height), and the building floats in **pure black void**. There's no sky, no horizon and no fog outside. The hideout is a **cluster of room modules** (one per room in §2.10) around the starting office, each a separate actor group with three states:
- **Locked:** a dark silhouette with a sheet-draped shape, faintly rim-lit, with tape on the door.
- **Building:** scaffolding and work lights.
- **Built L1–L3:** lights fade on with the reveal, and each upgrade level adds more props and lights.

The camera bounds widen as the base grows.

Stations (each one can be clicked and gets a hover highlight). Each station lives in its room, and the Lounge, Workbench and Garage stations become active once those rooms are built:
| Station | Prop | Opens |
|---|---|---|
| **Director's Desk** | Mahogany desk, green banker's lamp, ledger, rotary phone, whisky glass | Finances, recruit board, end-of-night summary |
| **Job Board** | Cork board, polaroids, red string, pins | Job list + crew planner |
| **The Lounge** | Worn leather sofa, pool table, ashtray, radio | Roster (criminal pawns idle here) |
| **Workbench** | Pegboard of tools, gun-case, duffel bags, work lamp | Gear shop + loadouts |
| **Garage Door** | Roll-up door, getaway car silhouette | Departures/returns animation |
| **Projector** | Old 35mm projector in the corner (a nod to "the Director") | Settings / run seed / quit |
| **Blueprint Table** (Office) | Architect's drafting table with rolled plans | **Build & upgrade rooms**, Director perks |
| **Screening Room** | Rows of velvet seats, a flickering screen | Retakes, the Final Cut job, the end-credits roll |
| **Clinic / Server Closet / Lawyer** | Gurney under a surgical lamp / blinking rack of servers / filing cabinets and a typewriter | That room's actions |

### 3.2 Camera

- A **true-isometric look made with a telephoto perspective camera** (FOV about 15–20°, yaw 45°, pitch −35°, placed far away). This gives the clean iso read while keeping Lumen, DOF and VSM behaving well. (Orthographic is the fallback if the look wins out.)
- Gentle pan (WASD/edge), scroll zoom inside limits, and a smooth dolly to a station when you click it. There's subtle **tilt-shift DOF** (miniature diorama feel) and a slow idle breathing drift.

### 3.3 Lighting: moody, warm/cold contrast, noir

- **Black void**: no SkyAtmosphere, a SkyLight captured from a black cubemap (or disabled), no ExponentialHeightFog. Post-process with locked exposure (manual EV) so the void stays true black. A thin floor-edge bevel catches the light so the diorama silhouette reads.
- **Key:** the green-shaded banker's lamp (warm 2700K pool on the desk).
- **Practicals:** the hanging pendant over the pool table, the workbench task lamp, a dim Edison string across the lounge.
- **Accent:** a pink/cyan **neon sign** outside a high window (rect light + emissive), spilling through **venetian blinds** (light-function or IES gobo) into **volumetric shafts**. Shafts come from **Local Fog Volumes** placed only inside the room, so the void stays black.
- **Atmosphere:** Niagara dust motes in the light shafts, a lazy cigarette-smoke curl from the ashtray, rain streaks on the window, a flickering projector beam.
- **GI:** Lumen (software Lumen is the safe default on Mac; hardware RT if it's stable on your GPU). Consider **MegaLights** for the many small shadowed practicals.
- **Post:** filmic tonemapper, mild bloom on practicals, film grain, vignette, a touch of chromatic aberration, and a slight teal-shadow/amber-highlight grade (LUT).

### 3.4 Characters

- MVP: the UE5 template **Manny/Quinn mannequins** (imported from the engine's `Templates/TP_ThirdPerson` content), with a **dark matte noir material**, a **type-colour accent** (rim light/emissive trim) and a simple silhouette prop per type (flat cap, beanie, fedora, headset, gloves, toolbag). Idle and walk animations come from the template.
- Pawns idle and wander in the lounge. A selected criminal gets a spotlight pool and a floating name/level plate. They walk to the garage when leaving and come back injured (limping/bandage tint) or don't come back at all if they're arrested.
- Post-MVP: Fab character/animation packs or MetaHumans (§9).

### 3.5 UI style

- Art-deco noir: cream paper cards, typewriter + condensed deco display fonts, gold hairline borders, red "stamp" outcomes (CLEAN / MESSY / FAILED / BUSTED). Cards slide out from the station you clicked.
- Built in **C++ (UMG widgets constructed in code / Slate styling)**, so agents can write and merge it as text without the editor (see §4.2).

### 3.6 Art sourcing for the MVP

- Greybox → final with **Modeling Tools** (already enabled) and engine primitives, plus **free Quixel Megascans / Fab content** for surfaces and props (wood, leather, brick, concrete, cork). Installing through the Fab toolset needs you to sign in to Fab, and **I'll only install packs you approve** (see Open Questions).

---

## 4. Technical architecture

### 4.1 Why it's built this way

- **Gameplay in C++, data in JSON, art in the editor.** `.uasset`/`.umap` files are binary and can't be merged, and only **one editor instance can serve the MCP on port 8000**. Keeping logic and data as text is what lets several agents work at the same time without conflicts.
- **One editor-driving agent at a time.** Every other agent compiles with UBT and tests headless.

### 4.2 Module & folder layout

```
Source/
  DirectorsCut.Target.cs / DirectorsCutEditor.Target.cs
  DirectorsCut/
    DirectorsCut.Build.cs            (Core, Engine, InputCore, EnhancedInput, UMG, Slate, SlateCore, Json, JsonUtilities, Niagara)
    Core/      ← CONTRACTS (lead-owned): types, enums, subsystem interface, asset path constants
    Sim/       ← Agent A: run state, RNG, generation, resolver, economy, levelling
    UI/        ← Agent C: HUD, panels, breakdown, outcome reveal, styles
    World/     ← Agent E: camera rig, stations, criminal pawns, interaction, sequences
    Audio/     ← Agent H: UDCAudioSubsystem (music states, SFX by name)
    Tests/     ← Automation tests (A owns sim tests; others add their own)
Content/
  Data/        ← Agent B: *.json (criminal types, names, jobs, gear, rooms, perks, balance constants)
  Fonts/       ← raw .ttf (loaded at runtime)
  RawAudio/    ← Agent H: licensed .wav/.ogg + LICENSES.md (imported to /Game/DC/Audio by the editor agent)
  DC/          ← Agent D: maps, meshes, materials, lighting, Niagara, characters
    Maps/L_Hideout.umap
```

### 4.3 Contracts (written in Wave 0, frozen during Wave 1)

`Source/DirectorsCut/Core/DCTypes.h`:
- Enums: `EDCCriminalType`, `EDCStat`, `EDCGearSlot`, `EDCCriminalStatus`, `EDCHeistOutcome`, `EDCGamePhase`, `EDCRoomType`, `EDCRoomState`
- Structs: `FDCStatBlock`, `FDCCriminal`, `FDCGearItem`, `FDCJob`, `FDCCrewPlan` (job id, criminal ids, cut), `FDCSuccessBreakdown` (every term from §2.7 as a named line), `FDCHeistResult`, `FDCRoom` (type, state, level), `FDCPerk`, `FDCRunSummary`

`Source/DirectorsCut/Core/DCRunSubsystem.h` (`UGameInstanceSubsystem`), the single API that UI and World use:
```
StartNewRun(int32 Seed = 0)
GetRoster() / GetJobs() / GetRecruits() / GetGearCatalogue() / GetCash() / GetNight() / GetHeistsCompleted()
PreviewPlan(const FDCCrewPlan&) -> FDCSuccessBreakdown
CommitPlan(const FDCCrewPlan&) -> bool   /  CancelPlan(JobId)
BuyGear(ItemId, CriminalId) / Unequip(CriminalId, Slot) / Hire(RecruitId)
GetRooms() / BuildRoom(RoomType) / UpgradeRoom(RoomType) / GetPerkOffer() / PickPerk(PerkId) / UseRetake(HeistResultId)
EndNight() -> TArray<FDCHeistResult>
Delegates: OnRunStateChanged, OnPlanCommitted, OnNightResolved(results), OnCriminalLevelUp, OnRoomChanged(room),
           OnPerkOffered(options), OnRunWon(summary), OnGameOver(summary)
```
Wave 0 ships a **stub implementation** that returns believable fake data, so C and E can build and test against it on day one while A writes the real thing.

`Source/DirectorsCut/Core/DCAssetPaths.h`: soft-object path constants for every asset that code references (`/Game/DC/Maps/L_Hideout`, `/Game/DC/Characters/SK_Criminal`, `/Game/DC/Materials/MI_Criminal_<Type>`, `/Game/DC/FX/NS_LevelUp`, …). **Agent D must create assets at exactly these paths.** Code agents load them softly and fall back to engine primitives if an asset is missing.

`Content/Data/schema.md`: the JSON schema for every data file (Agent B writes data, Agent A writes the loader, and both build against the schema).

World actors that Agent D places in the level are stubbed in Wave 0 as empty C++ classes, so D can place them before E fills them in: `ADCCameraRig`, `ADCStation` (with `EDCStationType`), `ADCCriminalSpawnZone`, `ADCGarageDoor`, and **`ADCRoomModule`** (with `EDCRoomType`).
- `ADCRoomModule` holds tagged child actor groups: `State.Locked`, `State.Building`, `Level.1`, `Level.2`, `Level.3`.
- Its lights are tagged `RoomLight` so E can fade them in for the reveal.

### 4.4 Build & test commands (used by every agent)

```bash
UE="/Users/Shared/Epic Games/UE_5.9"
PROJ="$PWD/DirectorsCut.uproject"

# Compile (no editor needed)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" DirectorsCutEditor Mac Development -Project="$PROJ" -WaitMutex

# Headless automation tests: MCP autostart DISABLED so it never fights the live editor for port 8000
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -unattended -nullrhi -nosplash -nosound -log \
  -ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False \
  -ExecCmds="Automation RunTests DirectorsCut.;Quit"
```

---

## 5. Parallel agent orchestration

Agents are launched from Claude Code with the `Agent` tool. **Code agents run with `isolation: "worktree"`**, so each one has its own `Intermediate/` and can compile without fighting over UBT locks. **The editor/art agent works in the main tree**, because it owns the running editor and the MCP. The lead (the main Claude session) owns the contracts and does every merge.

### Rules for all agents
1. Only edit files in your own folder (§4.2). If you need a contract change, **don't edit `Core/`**: describe the change in your final report, and the lead will apply it.
2. Don't open the Unreal Editor and don't call `mcp__unreal-mcp__*`, unless you are Agent D (or the Wave 2 polish agent).
3. Your work must compile with the Build command, and your tests must pass headless, before you report back.
4. Final report: files changed, what's done, what's stubbed, any contract requests, and known issues.

```
Wave 0  (lead, serial)          ─ foundation + contracts + stubs
   │
Wave 1  (6 agents in parallel)                                         Fri 13 → Sat 14 AM
   ├─ A  Simulation Core        [worktree]   Sim/, Tests/Sim   (incl. rooms, perks, retakes, win)
   ├─ B  Content & Data         [worktree]   Content/Data/*.json
   ├─ C  UI / UX                [worktree]   UI/, Content/Fonts
   ├─ D  Hideout Scene & Light  [main tree, owns editor + MCP]   Content/DC/**
   ├─ E  World & Interaction    [worktree]   World/   (incl. room reveal choreography)
   └─ H  Audio                  [worktree]   Audio/, Content/RawAudio
   │
Wave 2  (lead, serial)                                                 Sat 14 AM
   └─ merge A→B→C→E→H → build → wire into L_Hideout → PIE smoke test → α build → YOU PLAYTEST
   │
Wave 3  (3 agents in parallel, informed by your playtest notes)         Sat 14 PM
   ├─ F  Balance & QA           [worktree, headless]  1,000 auto-played runs, tune JSON, bug list
   ├─ G  Polish & Audio Import  [main tree, editor + MCP]  import audio, lighting/post/FX, room L2/L3 dressing
   └─ I  Juice & Feel           [worktree]   camera shake, tweens, number tickers, transitions (C++ only)
   │
Wave 4  (lead + J, serial-ish)                                         Sun 15, 10:00–14:00 PST
   ├─ Lead: feature freeze, fix blockers, package Shipping build, smoke-test the .app/.exe
   └─ J  Submission             [main tree, no editor]  DEVLOG.md, itch page copy, asset/licence list, video shot list
```

Wave 1 has six agents running at once. Only D touches the editor, and it starts straight away on greyboxing all the room modules, because it's on the longest path.

### Wave 0 — Foundation (lead, serial)
1. `git init` + an Unreal `.gitignore` (Binaries, Intermediate, Saved, DerivedDataCache) + **Git LFS** for `*.uasset *.umap *.ttf *.png`. Initial commit. (Worktrees need git.)
2. Convert to a C++ project: add `Source/` with the targets, the `DirectorsCut` module and `Build.cs`; add the module to `.uproject`; generate project files; first compile.
3. Write the contracts (§4.3): `DCTypes.h`, `DCRunSubsystem.h` + **stub .cpp**, `DCAssetPaths.h`, the stub world actor classes, and `Content/Data/schema.md`.
4. Add a minimal `ADCGameMode` / `ADCPlayerController` (mouse cursor, Enhanced Input mapping in C++), and set `GameDefaultMap` to `/Game/DC/Maps/L_Hideout`.
5. One trivial automation test, to prove the headless test pipeline works.
6. Commit as `wave-0`. **Exit criteria:** it compiles, the headless test passes, and the editor opens with the MCP up.

### Wave 1 — Agent briefs

**Agent A: Simulation Core**
- Implement `UDCRunSubsystem` for real: the seeded `FRandomStream`, the JSON data loader (from schema.md), roster/job/recruit generation with the playability guarantees from §2.2, the success resolver from §2.7 returning a full `FDCSuccessBreakdown`, outcomes and the injury/arrest rules, cut → loyalty, XP and levelling from §2.8, difficulty scaling from §2.9, upkeep, game-over detection and the run summary.
- Rooms and upgrades (§2.10): costs, prerequisites, effects as modifiers, and upkeep per room. The perk draft every 3 heists (§2.11). Retakes. The Final Cut job and the win condition (§2.12). Rooms and perks are implemented as a generic **modifier stack** (stat bonuses, chance bonuses, payout/upkeep multipliers, capacity changes), so B can add perks as data only.
- Tests: determinism (same seed = same run), formula unit tests (edge clamps, refusal threshold, lucky break), generation guarantees, room/perk modifier application, a winnable run with a scripted seed, and a 200-night auto-play smoke test that doesn't crash.
- Until B's data lands, use its own small `Tests/Fixtures/*.json`.

**Agent B: Content & Data**
- Write `Content/Data/criminal_types.json`, `names.json` (≥40 first names, ≥40 surnames, ≥60 nicknames), `jobs.json` (the 8 templates, with 3 flavour-text variants each), `gear.json` (about 15 items), `rooms.json` (§2.10, with flavour text per level), `perks.json` (≥15, §2.11), the Final Cut job, `balance.json` (every constant in §2.7–2.10), `strings.json` (film-themed copy from §2.12, credits text) (outcome lines such as *"Vinnie got out clean, smelling of diamonds."*).
- Validate everything against schema.md with a tiny Python validator in `Content/Data/validate.py`.
- Tone: witty 1970s crime-noir, never grimdark.

**Agent C: UI / UX**
- C++ UMG/Slate: the art-deco style set (colours, brushes, runtime-loaded TTF fonts; pick free OFL fonts, for example *Special Elite* (typewriter) and *Limelight* or *Poiret One* (deco)), plus:
  - A top bar (cash, night, heists, upkeep due)
  - The Job Board panel (job cards)
  - The Crew Planner (role slots, roster drag/click to assign, cut slider, live **"Why this %"** breakdown, refusal warnings)
  - The Workbench panel (catalogue + 3-slot loadout per criminal)
  - Roster cards (stats bars, level/XP, luck, loyalty, status)
  - The Recruit panel
  - The **Night Resolution sequence** (one card per heist: chance bar fills → roll → stamp, with a level-up flourish)
  - Game-over summary (+ seed, "New Run")
  - The title screen ("Director's Cut" with a projector-flicker title)
  - The **Blueprint Table** build panel: room cards with locked/available/built states, costs and effects
  - The **perk draft** (3 cards, pick 1)
  - Retake prompts in the Dailies
  - The **win end-credits roll**
  - A settings panel (volume sliders, fullscreen/resolution)
  - A 3-step first-time **tutorial hint** strip. Judges play cold, so it has to be learnable in 60 seconds
- Builds only against the stub subsystem. Panel open/close is driven by `UDCUISubsystem::OpenPanel(EDCPanel)`, which E calls when a station is clicked.

**Agent D: Hideout Scene & Lighting (the only editor/MCP user in Wave 1)**
- **Order of work:** greybox *all* room modules and lighting first (playable by Saturday morning), then the Office at final quality, then Lounge/Workshop/Garage, then the Should/Could rooms. Each room is an `ADCRoomModule` with Locked/Building/L1–L3 actor groups (§4.3), laid out around the office like a cinema floor plan (the auditorium sits behind the office).
- Build `/Game/DC/Maps/L_Hideout`: the cutaway office (about 10 × 8 m) plus the room modules, the black void setup, every station prop group from §3.1, and the lighting rig, local fog volumes, post-process volume and LUT from §3.3. Add Niagara dust/smoke/rain. Import the template mannequin plus anims to `/Game/DC/Characters/`, and make the noir criminal material with a per-type accent MI at the paths in `DCAssetPaths.h`.
- Place the stub actors (`ADCCameraRig`, six `ADCStation`s, `ADCCriminalSpawnZone`, `ADCGarageDoor`).
- Deliver **PIE screenshots from the game camera** (wide plus each station) for review, and keep a steady frame rate on your Mac (target 60 fps at 1440p).
- Commit assets on the main tree in small commits; never touch `Source/`.

**Agent E: World & Interaction**
- `ADCCameraRig` (telephoto iso, pan/zoom/clamp, eased focus-to-station, tilt-shift DOF control, idle drift).
- `ADCStation` (hover outline through custom depth/stencil, click → focus camera + `OpenPanel`).
- `ADCCriminalPawn` (spawned from the roster, type material/prop, idle-wander in the spawn zone with simple navmesh or spline wandering, selection spotlight, a world-space name plate, a level-up FX hook, injured tint).
- `ADCGarageDoor` (open/close timeline). The night departure/return choreography listens to subsystem delegates.
- `ADCRoomModule` behaviour: switching state and level groups, plus the **room-built reveal** (camera pans to the room, `RoomLight`s fade on in a staggered sequence, a dust-puff hook, then the camera pans back). Camera bounds grow with the built rooms.
- Must run with primitive fallbacks if D's assets aren't merged yet.

**Agent H: Audio**
- Source licensed music, ambience and SFX (§2.13) into `Content/RawAudio/`, with a `LICENSES.md` row for every file (source, licence, author, URL).
- Don't install from Fab without your approval. Prefer CC0 or commercial-friendly sources.
- Write `UDCAudioSubsystem`: music states (Title / Planning / Dailies / Win / GameOver) with crossfades, `PlaySFX(Name)` through soft paths under `/Game/DC/Audio/`, and volume settings. It binds to subsystem delegates so the game is scored automatically.
- G imports the files in Wave 3. H delivers an import manifest, `Content/RawAudio/manifest.json` (file → asset path → sound class → loop?), so the import can be scripted over MCP.


### Wave 2: Integration → α build (lead, Saturday morning)
- Merge the worktrees in this order: A → B → C → E → H. Apply any contract requests, compile, open the editor, wire the panels to stations and room modules in `L_Hideout`, and run a PIE smoke test of a full run via MCP with screenshots. Fix any blockers.
- Write `PLAYTEST.md`: how to launch, the controls, what to look for, and known issues. **You play it** and send notes, which become the inputs to Wave 3.

### Wave 3: Balance, polish, juice (3 agents in parallel, Saturday afternoon/evening)
- **Agent F: Balance & QA** (headless). Add a `DirectorsCut.Balance` automation test that auto-plays 1,000 seeded runs with a simple greedy-but-sane bot. The bot builds rooms and picks perks too. Report the distribution of nights survived, win rate, success rate per tier, room build timings and cut choices. Tune the JSON to hit these targets:
  - Median run of 15–25 nights
  - Win rate for the bot of about 15–25% (a good human should win about half the time)
  - Tier 1 success with a sensible crew ≈ 70–80%
  - A cut of 70%+ almost always worse than 40%
  - Every room worth building in some runs
  - No dominant job or perk

  Report the results as tables, plus a prioritised bug list.
- **Agent G: Polish & audio import** (the only editor/MCP user in Wave 3):
  - Import the audio from H's manifest
  - Dress room levels L2/L3
  - Lighting and post pass
  - Hover glow, a pawn level-up VFX, the room-reveal dust and light polish
  - Check for shimmer/noise from Lumen and VSM
  - Performance pass (60 fps at 1440p)
  - Before/after screenshots
- **Agent I: Juice & feel** (C++ only): camera shake and flash on stamps, eased tweens on every panel, cash and number tickers, film-reel and burn transitions, the projector-flicker title, and hover sounds wired to H's SFX names.

### Wave 4: Ship it (Sunday 10:00–14:00 PST)
- **10:00, feature freeze.** The lead fixes blockers only.
- The lead packages a **Shipping** build: `RunUAT BuildCookRun -platform=Mac -clientconfig=Shipping -cook -stage -pak -archive`. The Windows build comes from a Windows machine if one is available (§7). Then a clean-machine-style smoke test: launch, play to night 5, build a room, quit. Check the size is under 1 GB and zip it as `TeamName_DirectorsCut.zip`.
- **Agent J: Submission pack** (text only, starting Saturday evening):
  - `DEVLOG.md` for the Glass Box prize: how we *directed* parallel agents, the wave diagram, agent reports, screenshots, and what went wrong
  - The itch page copy (pitch, controls, screenshots list) and the Gatekeeper "right-click → Open" note for macOS
  - The **asset and licence list** compiled from every `LICENSES.md`, plus the pre-jam declaration
  - The **30–60 s video shot list and script** (title → planning a heist → cut slider → Dailies stamp → room reveal → perk pick → win credits). You record it with screen capture (QuickTime), or we use the in-game camera rig with a scripted demo seed.
- **14:00, submit.** That leaves 4 hours of buffer before the 17:59 PST deadline.

---

## 6. Jam build definition of done

**Must** (core loop, never cut):
- [x] A title screen, then a new seeded run in the lit hideout on a black void
- [x] Random roster (4) and random job board every run; the seed shows on the end screen
- [x] All 6 criminal types and 8 job templates are in the game, and required roles are enforced
- [x] Gear can be bought and equipped, and it visibly changes stats and success %
- [x] The cut slider changes willingness and success %, with refusal warnings
- [x] "Why this %" breakdown is accurate to the resolver
- [x] The Dailies reveal shows four outcome bands; injuries and arrests are applied
- [x] XP/levelling raises stats and luck; lucky-break re-rolls fire
- [x] Difficulty, tiers and upkeep scale with heists completed
- [x] **Office, Lounge, Workshop and Garage** can be built and upgraded, each with a lights-on reveal
- [x] Game over ("CANCELLED") plus a run summary, and "New Run" works
- [x] Clickable stations with hover highlight and a camera focus dolly; pawns idle and depart/return through the garage
- [x] Moody Lumen lighting, volumetric shafts, neon spill, dust/smoke FX, filmic post
- [x] **Music, ambience and UI SFX**, with volume settings
- [ ] A packaged build runs outside the editor, and the zip is under 1 GB

**Should:**
- [x] The perk draft (≥15 perks)
- [x] The Screening Room with Retakes, the **Final Cut** job, and the **win end credits**
- [x] Back-Alley Clinic
- [x] Tutorial hints
- [x] Room L2/L3 visual dressing

**Could:**
- [x] Server Closet, Lawyer's Office
- [ ] Endless mode after a win
- [ ] A Windows build

**Submission:**
- [ ] The video (30–60 s)
- [ ] The itch page
- [ ] The asset list
- [ ] `DEVLOG.md`
- [ ] Submitted by 14:00 PST Sunday

---

## 7. Risks & mitigations

| Risk | Mitigation |
|---|---|
| **Deadline (about 2.5 days)** | MoSCoW scope, a playable α by Saturday morning, freeze at 10:00 Sunday, submit 4 hours early. Cut the Could items, then the Should items, never the Must items |
| **Judges mostly on Windows, and we can only package for Mac here** | Ask: do you have access to a Windows PC? If you do, we push the repo and package there on Sunday morning. If you don't, ship Mac only with clear Gatekeeper instructions, and keep the project Windows-ready (DX12/SM6 is already configured) |
| macOS Gatekeeper blocks the unsigned .app | Include the "right-click → Open" / `xattr -dr com.apple.quarantine` instructions on the itch page and in a README inside the zip |
| Two editors fight over MCP port 8000 (Action project shares it) | Only one editor agent at a time. Headless runs pass `bAutoStartServer=False`. Close the Action editor while working here, or move DirectorsCut to port 8001 |
| Binary asset merge conflicts | Only D/G touch `Content/DC`. Code, data and UI are text. Git LFS for binaries |
| Contract drift between parallel agents | `Core/` is lead-owned and frozen during Wave 1. Changes go through requests in final reports |
| Lumen HW-RT instability on Metal | Default to software Lumen, with HW RT as an optional toggle |
| "Polished" art with no art assets | Megascans/Fab packs (with your approval) plus a strong lighting/post pass. Characters are stylised silhouettes, so mannequins read as intentional |
| Licence problems | Every external file is logged in `LICENSES.md`. Nothing is installed from Fab without your approval. No third-party IP or real brands |
| Balance feels random or unfair | A transparent breakdown UI plus the 1,000-run auto-play balance harness |
| UBT lock contention | Code agents work in separate worktrees, and builds use `-WaitMutex` |

---

## 8. Controls

- **Mouse:** hover/click stations, criminals and UI. **RMB drag / WASD:** pan. **Scroll:** zoom.
- **Esc:** close panel / back out of a station. **Space:** "Roll Camera" (end the night) once crews are committed.

---

## 9. After the jam (if it goes well, for example a Continuation Grant)

1. **Heat & police:** global heat from loud jobs, raids on the hideout, bribes.
2. **Traits & quirks:** *Hothead*, *Greedy*, *Loyal to a fault*, crew chemistry and rivalries.
3. **Heist events:** mid-heist choices ("The guard recognises Vinnie, so bribe or bolt?") in the Dailies.
4. **Meta-progression:** unlock new criminal types (Demolitions, Insider, Sniper), jobs, rooms and starting perks across runs.
5. **A heist diorama cinematic:** a miniature isometric replay of each heist in the void beside the hideout.
6. **An art pass:** Fab characters/animations or MetaHumans, bespoke props, and more room tiers.
7. **Save/load mid-run**, controller support, accessibility, and a Steam build.

---

## 10. Open questions for you

1. **Team:** solo, or a team (up to 5)? This decides Solo Driver vs Best Team, and we need the team name for the zip.
2. **Windows:** do you have access to a Windows PC for packaging on Sunday? Most judges will probably be on Windows.
3. **Art and audio sourcing:** OK to sign in to Fab and install **free** Megascans/Fab props, textures and audio? I'll list the exact packs before installing anything.
4. **Setting:** keep the "abandoned art-deco cinema" hideout? It's a strong fit for the "Director's Cut" theme.
