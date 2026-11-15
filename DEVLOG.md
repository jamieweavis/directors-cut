# Director's Cut - Devlog

Epic AI MegaJam entry, built Nov 13–15, 2026. Three days, about 160 commits, a lot of coffee.

## Table of Contents

1. [Idea](#day-1-idea)
2. [Building the core gameplay loop](#day-1-building-the-core-loop)
3. [Refining the core loop](#day-2-refining-the-core-loop)
4. [Level Design](#day-2-level-design)
5. [Crew Bodies](#day-3-crew-bodies)
6. [Cars](#day-3-cars)
7. [Graphics](#day-3-graphics)
8. [Perks and Balance](#day-3-perks-and-balance)
9. [Music](#day-3-music)
10. [Shipping It](#day-3-shipping-it)

---

## Day 1: Idea

I started by brainstorming with Claude and landed on two themes I liked:

1. Some sort of director mechanic where you can yell **CUT!** and **ACTION!**
2. Something where you're "the director" who runs a crew and takes a cut of their jobs

### Prototyping

Rather than pick one on gut feel, I wrote a detailed prompt for each idea, with a plan to get a playable prototype up before fleshing out a full game.

**Idea 1: CUT! / ACTION! physics puzzler.** Something falls and you have to get it to the exit. Yell CUT! to freeze time and rearrange the props, then ACTION! to start it again and see if your setup works.

I never really knew what it would look like or what the falling thing even was, but I could picture the puzzle side working: time and physics play well together, and CUT!/ACTION! gives you a nice layer of control and strategy. The prototype turned out alright, but it didn't wow me.

**Idea 2: the director who takes a cut.** This one was a roguelike management game. You recruit and upgrade crew members, send them on missions, and try to make as much money as you can without losing them.

I added roguelike stuff to the prototype: random missions, permadeath for the crew, rotating objectives and randomised characters. This was the one that clicked, so it became the game.

## Day 1: Building the Core Loop

With the idea locked in, I set the project up so I could run several Claude agents at once without them stepping on each other. First came a "Wave 0" pass: the C++ module, shared **contracts** (types and subsystem APIs every part of the game codes against), stubs for everything, a greybox hideout, and a set of dev tools for screenshots and auto-play so the agents could check their own work without me clicking around.

Then "Wave 1" split the game into parallel tracks, each on its own worktree:

- **Data:** crew, traits, missions, gear, rooms and perks as JSON, plus a validator and a Monte-Carlo balance script
- **Simulation:** the run itself (generation, mission resolution, rooms, perks)
- **World:** camera rig, input, clickable stations, crew pawns
- **UI:** the whole interface in Slate
- **Audio:** procedurally generated SFX and ambience, with no samples or recordings
- **Scene:** the hideout diorama, materials and lighting

Everything merged back into main by early evening and I had an alpha. One fun casualty: Unreal's unity build kept mashing together files from different agents that happened to use the same names, so I switched it off.

### Turn-based → real time

The first version was a turn-based daily sim: plan your day, hit go, read the results. It worked, but it felt like a spreadsheet. So on the evening of Day 1 I redid the contracts for a **real-time** version:

- A day/night clock that keeps running
- Timed missions, with crew physically walking around the hideout
- Build timers for new rooms
- A darker, icon-first UI
- Save and continue

All of the tracks got rebuilt against the new contracts in one more wave. By the end of the night crew were walking around a warehouse hideout with god rays coming through the windows and a day/night lighting rig.

## Day 2: Refining the Core Loop

Day 2 started with a playtest and a pile of fixes:

- Drag crew from the roster straight onto mission slots
- Gear slots in the mission planner (the Workshop unlocks a third)
- Crew who quit over low loyalty or get arrested now actually tell you about it
- Mission costs go red when you can't afford them
- Lots more names and nicknames, so you stop getting the same three people

Then the bigger systems landed: **3 save slots**, a **leaderboard**, an **options screen**, and warnings when a job is about to expire.

The HUD got a reshuffle too. The clock, day and payday moved into one bar in the top right, the crew list now stays visible bottom-left, and mission results pop up as cards bottom-right. Game speed controls came in as well (pause / 1x / 2x / 4x, on Space / 1 / 2 / 3).

I also changed the opening so you start with **no crew**: you get some starting cash and hire your first people yourself, which makes the first few minutes far more interesting.

## Day 2: Level Design

This is where the game changed the most. The warehouse looked fine, but it was all interior, so you never really saw the day/night cycle. I scrapped it for a **rural scene**: a small abandoned farmhouse in a meadow, front walls collapsed so the isometric camera can see in, and enough outside space to show off the sun arc and the coloured lights at night.

Rooms stopped being fixed spots inside a building and became **wings** of the house, so it physically grows as you build. That first version used set plots for each wing, which felt too rigid, so it got replaced by a **player-placed build grid**:

- The grid is deliberately **wonky**: every cell corner is jittered, so the house looks hand-built rather than snapped to a ruler. (The first jitter was a bit too wonky, so I halved it on Day 3.)
- The Office sits in the front row facing the lane, and new rooms can go on any free cell touching the house (left, right or behind, never in front).
- The house geometry gets regenerated from your placements: floors, tall back walls, collapsed fronts, partitions with doorways where rooms connect, and timber frame and scaffolding while a room is going up.

Late that night I added a **procedural furniture solver**. It reads each room's shape, which edges are walls, partitions or open, and where the doorways and exits are, then tries layouts with scoring and backtracking until it finds one that fits. It started with just the Office. By Day 3 every room had its own recipe: Lounge, Workshop, Ops Room, Clinic, Server Room, Lawyer, and the Garage, which I re-themed as a **Kit Room** with lockers and go-bags for crews heading out.

To make all of that walkable, crew pathfinding moved to a **generated tile map**. The house stamps a 20 cm grid from its own walls, doorways, furniture and parked cars, and crew run A* over it. They also steer around each other, queue, and only use the front door to get in and out. There's a debug view in the options menu that draws the whole lot, and it saved me a lot of guessing.

## Day 3: Crew Bodies

Up to this point the crew were the stock Unreal mannequins. For Day 3 I used **Tripo AI** to generate 3D models for the crew and brought them into Unreal with a custom Python script (`Tools/crew_import.py`) that runs headlessly.

The trick that made it work: each Tripo body is driven by a **hidden mannequin** underneath, so all the existing animations just carry over. Manny drives the male bodies and Quinn drives the female ones.

The roster grew over the day: Hooded, then a second male model, Hoodie, Leather Jacket, Tracksuit, Casual, Navy Sweats, Businessman, Brown Jacket and Shearling, for **ten male bodies**. After that came **ten female bodies**: Red Top, Blouse, Puffer, Plum Jacket, Businesswoman, Denim, Streetwear, Blazer, Tank Top and Cream Knit.

There were a few bumps:

- Some exports came out of Unreal's Interchange importer about **100× too big** with stretched skin, thanks to a bind pose warning. Routing those through the legacy FBX importer fixed it, and every female body ended up needing that path.
- One body used a different bone naming scheme, so it needed a bone map.
- Blazer was first exported without a skeleton and came in as a static mesh. A re-export sorted it out.

Crew now get a gender based on the name they're dealt, and every new hire gets a random body that **nobody else in play is wearing**, so your crew always look like different people.

## Day 3: Cars

Cars came in on Day 2 with the rural rework: every car in the car park is a **mission slot**. When you send a crew out, they walk to a car, drive off down the lane, and come back the same way when the job's done.

At first I used basic Unreal engine shapes, and that worked fine early on.

When it came time to polish, I tried car models from Tripo AI, the same way I'd made the crew bodies. I got seven of them in (estate, sedan, pickup, classic, SUV and so on), but they were **way** more detailed than everything else in the scene. They also came as one mesh, so the doors and wheels couldn't move anymore.

So I went back to the basic models and improved them instead. The cubes became smooth generated body shells with a sloped bonnet, a raked windscreen, side windows that lean in at the roof, and rounded bumpers. That let me keep:

* rotating and turning wheels
* lights with proper headlamps
* opening and closing doors

The work from the Tripo detour wasn't wasted, though. The new turning code (the back axle gets dragged behind the front one, so cars pivot like real cars instead of spinning on the spot) stayed.

The last piece was the **car park**. Bays are now angled pull-throughs with a one-way loop in and out, and departures are staggered and planned ahead so cars never drive into each other.

## Day 3: Graphics

Late on Day 2 I wanted a prettier, more stylised look for the isometric view, and settled on a **toybox diorama** style with light cel shading. It's built from three pieces:

- **Toybox palette:** every surface colour in the game gets remapped as its material is created. Brighter, calmer greens and far less procedural grime. It's one function, so I could tune the whole look in one place.
- **Cel shading with ink outlines:** a post-process material that draws ink outlines and splits the lighting into soft bands. My first setting was too harsh and looked blotchy in dim rooms, so I softened the bands. Grass and leaves are marked so they don't get outlines (otherwise the meadow turns into a scribble). It costs about 1 FPS, and there's a toggle in Graphics options if you want it off.
- **Tilt-shift:** the camera uses a shallow depth of field focused on the point it's looking at, which makes the whole farmhouse look like a little model on a table. A subtle vignette helps sell it.

The landscape got a final pass too: leafy trees, tiered pines, shrubs and chiselled rocks, grass growing over the build grid (cleared wherever a room stands), and the yard and paths blended into the grass. A couple of things got cut to keep it clean, like a campfire, a pond and some fireflies.

## Day 3: Perks and Balance

Day 3 was mostly playtesting and tweaking, and then more playtesting.

- **Rebalance (late Day 2):** a 4-person recruit board (6 with the Lounge) where no role repeats, flatter payouts, and rare jackpot jobs that pay 2.5× but are harder and riskier. Recruits refresh twice as often.
- **Harder odds:** strong crews were sitting at 100%+ success, which killed the tension. Now a perfectly matched plan shows about 88%, and you need traits, perks, rooms or night jobs to push past that.
- **8 new perks:** Foreman (two builds at once), Hot Streak, Solo Act, Insurance Policy, Nest Egg, Fresh Blood, Line of Credit and All In. The last perk draft of a run also throws away the two you didn't pick, so the final choice actually costs you something.
- **Perk draft UX:** you pick a card first, then hit Confirm, so it's harder to grab the wrong one by accident.
- **Room labels:** room levels now spell out what they do in plain language.

The balance bot from Day 1 kept paying for itself here. It plays thousands of simulated runs, so I could check things like when you can first afford the Big Score without playing the whole game every time.

## Day 3: Music

For most of the jam the game had no music at all, just procedural SFX and ambience. Right at the end of Day 3 I wanted something to sit under the hideout, and went for a **laid-back lounge score**: easy bossa by day, slow brushed swing at night.

Like the rest of the audio, none of it is samples or recordings. A Python script (`Tools/music_gen.py`, using numpy and scipy) both **composes and synthesises** every note:

- **Four sets:** DayA (a sunny bossa in F, 96 bpm), DayB (brighter and busier in Eb, 104 bpm), NightA (D minor swing, 82 bpm) and NightB (a warmer late-hours groove in A minor with ride cymbal, 88 bpm). Each one is a 32-bar tune with its own key, tempo and hand-written chord progression.
- **Seeded composition:** the melodies are generated from rules. The script picks rhythm cells for a call, a response and a bridge, gives each a random contour, then snaps every note to a scale that fits whatever chord is playing underneath. The chords are voiced so each one moves smoothly to the next, and the bass plays roots and fifths with a little approach note leading into the next chord. Same seed, same tune, every time.
- **Synthesised instruments:** electric piano, bass, vibraphone, flute, nylon guitar, pads, bells, and a drum kit (kick, rim, brushes, shaker, hats, ride, congas), each built from oscillators, noise and filters. The reverb is generated too, and the tail of each loop wraps back round onto its start so the loops are seamless.

Each set is rendered as **five stems** of exactly the same length: **Core** (piano and bass), **Groove** (drums), **MelA** (vibes), **MelB** (flute, a different tune over the same chords) and **Color** (guitar and congas by day, pad and bells at night).

In game, all five stems of a set start on the same frame and the audio subsystem **re-mixes them every 8-bar phrase**, so the music follows what's happening:

- It switches between day and night sets with the clock, with a bit of slack so it doesn't flip back and forth around dusk and dawn.
- As you unlock higher mission tiers, it moves through four "chapters": the later sets come in, the layers fill out, and eventually both melodies play together.
- Set changes crossfade on a phrase boundary, so you never hear a cut mid-bar.
- The title screen gets a quiet mix, pausing ducks it and muffles it with a low-pass filter, and it fades out on game over.

There's a new **Music** volume slider in Options and the pause menu (60% by default), and tests check the music assets load and loop at the right tempo, plus the mixing rules.

## Day 3: Shipping It

I built the whole thing on **macOS** with **Unreal Engine 5.9**, which meant Windows was the scary unknown. I added packaging scripts for both platforms (`Scripts/package_mac.sh` and `Scripts/package_win.ps1`). They cook the game, check that every asset the C++ loads by path actually made it into the build, and warn if the zip goes over the jam's 1 GB limit.

Getting it compiling on Windows late on Day 3 meant a few rounds of fixing MSVC errors that the Mac compiler never complained about: variable shadowing, narrowing conversions in brace-init, and the like. Shipping builds also caught an editor-only argument that had slipped in.

For the jam submission I built custom scripts to make the screenshots and video for me instead of capturing them by hand:

- **Screenshots** (`Scripts/screenshots_mac.sh` / `screenshots_win.ps1`) launch the game once per scene: menu, early game, mid game, late game, night, victory and game over. An auto-play bot hires a crew, builds rooms and plays up to the right hour, then the script takes the shot. Each batch goes into its own timestamped folder.
- **Video** (`Scripts/record_video_mac.sh` / `record_video_win.ps1`) plays a scripted shot list (`Scripts/video_reel.txt`) through one seeded run: placing a room, starting a job, picking a perk, time-lapses. Between shots it jumps the run forward behind a cut to black. The script records the screen, matches up the timing cues, and cuts out 10-second clips I can drop straight into an edit.

Final polish before submitting: a loading screen that waits until shaders and streaming have settled, rendered crew portraits in the UI, rural ambience replacing the old city sounds, and one last change that felt right for a crew of night-shift criminals: **they don't sleep anymore**. They just walk around and hang out through the night.
