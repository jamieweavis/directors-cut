# Balance notes (v2 realtime)

`python3 Content/Data/balance_check.py` runs 200 seeded bot runs of 120 min each. `python3 Content/Data/validate.py` checks the data against `schema.md`.

## The bot
The bot follows a simple greedy policy and plays at cut 0.30:
- It starts every mission it can staff at ≥ 50%, best expected take first. The Big Score always goes first.
- It pays a mission's cost whenever it has the cash, with no buffer, which is the main source of game overs.
- It buys a Tool when that counters an open threat on the plan.
- It builds the cheapest affordable room or upgrade, keeping 2 days of upkeep in reserve.
- It hires the best affordable recruit while the roster is under the cap, keeping the same reserve.
- It picks perks at random.

## Result (seed 1, 200 runs, v4 rebalance)
| Milestone | reached | p25 | median | p75 | target |
|---|---|---|---|---|---|
| First success | 100% | 1.3 | 1.5 | 1.7 | ≤ 2.5 min |
| First room (Garage) | 100% | 4.9 | 6.3 | 10.4 | ≤ 6 min |
| Tier 2 unlocked | 99% | 13.0 | 16.2 | 20.3 | ~10–12 min |
| Tier 3 unlocked | 98% | 23.7 | 29.8 | 35.9 | ~25–35 min |
| Tier 4 unlocked | 96% | 36.6 | 43.9 | 52.6 | |
| Big Score unlocked | 89% | 63.9 | 72.4 | 83.8 | ~55–75 min |
| **Big Score won** | **34%** | 84 | 99 | 108 | 30–60% |
| **Game over** | **2%** | 41 | 59 | 83 | 10–25% |

- Game overs are well under the old 10–25% target. Payouts are flatter and mission costs are lower than the old tier 3/4 stakes, so one failure no longer empties the bank. The pressure now comes from slow growth (cash sits at about 5–8k all run), not from sudden collapses.
- **Mission success by tier (bot, ≥ 50% plans only):** tier 1 84%, tier 2 91%, tier 3 92%, tier 4 87%, Big Score 80%.
- **Typical room order:** Garage 6m → Ops L1 24m → Clinic 34m → Lounge 44m → Workshop 57m → Ops L2 68m (Big Score) → Clinic L2 → Office L2 …

## Key numbers
- **Economy:**
  - You start with **4,500 cash and no crew**. Two Street hires (1,200–1,700 each) plus a 400 reroll still leave a buffer.
  - While the crew has fewer than two members, every recruit roll (including rerolls) lists enough Street hires to make up two.
  - Upkeep is **550 per crew per day**, plus room upkeep. A day lasts 24 min, and payday is at 06:00 (first payday at t = 23 min).
- **Recruits:**
  - **4 listed**. The Lounge raises this to 5 at L1 and 6 at L2/L3 (`recruitBoardAdd`).
  - Roles are dealt from a shuffled bag, so a board never repeats a role until all six have appeared.
- **Progression:**
  - The tier unlocks fall at **0 / 9 / 16 / 24 successes**. Tier 3 and above also need Ops Room L1 (2.5k).
  - The Big Score needs **34 successes and Ops Room L2** (4.5k).
  - The level ranges by tier are **1–2 / 2–3 / 4–5 / 6–8**, with XP curve `100 × L^1.3`.
  - XP rewards by tier are about 60 / 150 / 320 / 600, and 800 for the Big Score.
- **Base limits:**
  - **1 active mission**. The Garage adds +1, the Office L2 adds +1 and the Ops Room L3 adds +1.
  - A roster cap of 4. The Lounge adds +2, +3 or +5 by level.
  - A board of 4.
- **Payouts.** The curve is flatter: each tier pays roughly 2× the one below, not 4–5×. Durations are shorter at the top, so the take per mission slot still rises gently with tier. The Director takes 30%.

  | Tier | Payout | Cost | Duration |
  |---|---|---|---|
  | 1 | 1.6–3.2k | 0–200 | 60–120 s |
  | 2 | 4.5–7.5k | 450–1.2k | 150–240 s |
  | 3 | 9–14k | 1.2–2.6k | 270–400 s |
  | 4 | 16–25k | 2.2–4.5k | 420–600 s |
  | Big Score | 100k | 6k | 600 s |

- **Rare jobs** (`board.rareChance` 6%) are the high-value postings. They pay **2.5×**, are **one level harder** and carry **one extra threat** (from the template's pool first, then any threat). They still roll loot. Tier weights also favour lower tiers once everything is unlocked (25/35/27/13%).
- **Arrest risk:** 0.04–0.06 at tier 1, 0.08–0.10 at tier 2, 0.20–0.30 at tier 3, 0.35–0.45 at tier 4 and the Big Score. `outcome.bustedBand` is 0.4, so 40% of the failure range is Busted.
- **Room L1 costs:** Garage 1.5k, Ops 2.5k, Clinic 3.0k, Lounge 3.5k, Workshop 4.0k, Server 7.0k, Lawyer 8.0k. L2/L3 run from 4.5k to 20k.

## Changes from the example values in schema.md
These are data choices; the formula is unchanged:
- `tiers.unlockSuccesses` [0,3,8,15] → [0,9,16,24]
- `tiers.levelRange` → [[1,2],[2,3],[4,5],[6,8]]
- `final.requiredSuccesses` 15 → 34
- `levels.xpExp` 1.4 → 1.3
- `upkeep.perCriminalPerDay` 300 → 500
- `outcome.bustedBand` 0.25 → 0.4

The Office L1 has no effect (4 leads, 1 active). Its levels add board and active slots, and the Garage is the first "+1 active" room.

## Interpretations used by the sim (Agent A, please match or tell us)
- **`unlockTier: v`:** tiers ≥ v may post once their success count is reached. Ops Room L1 has v = 3, which opens both tier 3 and tier 4.
- **The Big Score posting:** it posts as an **extra board entry that never expires** once it is unlocked. It stays on the board after a failed attempt.
- **Recruit levels:** recruits roll their level from `levelRange[unlockedTier-1]`, plus `recruitLevelAdd`.
- **Negative traits:** with `negativeTraitChance`, a recruit gets **one extra** negative trait on top of the positive count (1, 2 or 3).
- **Cut and loyalty:**
  - The `greedy` loyalty penalty applies at mission start when cut > FairCut.
  - The `successFair` loyalty gain applies on a success when cut ≤ FairCut.
- **Night jobs:** "Night" means the hour is ≥ duskHour or < dawnHour when the job starts.

## Threat coverage
Twelve single-threat abilities can't cover 8 threats twice each, since that needs 16 counters. Guards, Witnesses, Cameras and Alarms each have 2 ability types. Pursuit (Wheelman), Patrols (Wheelman), Locks (Ghost) and Vault (Safecracker) have one type each, plus shop Tools: Clean Plates, Police Radio, Bolt Cutters / Master Key and Thermal Drill. `validate.py` requires ≥ 2 counters per threat across abilities and gear, with at least one gear backup wherever only one type counters a threat.
