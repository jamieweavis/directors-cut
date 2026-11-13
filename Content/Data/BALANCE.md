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

## Result (seed 1, 200 runs)
| Milestone | reached | p25 | median | p75 | target |
|---|---|---|---|---|---|
| First success | 100% | 1.2 | 1.4 | 1.6 | ≤ 2.5 min |
| First room (Garage) | 100% | 3.8 | 4.3 | 5.3 | ≤ 6 min |
| Tier 2 unlocked | 100% | 8.9 | 9.9 | 11.8 | ~10–12 min |
| Tier 3 unlocked | 99.5% | 22.3 | 25.8 | 32.9 | ~25–35 min |
| Tier 4 unlocked | 96.5% | 30.3 | 34.2 | 39.1 | |
| Big Score unlocked | 84.5% | 54.2 | 62.8 | 70.0 | ~55–75 min |
| **Big Score won** | **43%** | 88 | 98 | 108 | 30–60% |
| **Game over** | **19.5%** | 47 | 47 | 47 | 10–25% |

Seeds 7 and 13 give a win rate of 50% and 42%, with game-over rates of 12% and 20%.

- **Mission success by tier (bot, so ≥ 50% plans only):** tier 1 86%, tier 2 90%, tier 3 87%, tier 4 81%, Big Score 78%.
- **Arrests:** 0.17 per run, and 14% of runs lose someone.
- **Game overs:**
  - About 3.5% happen at payday 1 (23 min), after a tier 2 failure that ate the cash.
  - Most happen at payday 2 (47 min). That is the mid-game squeeze, when the Ops Room, the Lounge, hires and tier 3 costs all land together.
  - The rest come at payday 3 (71 min).
- **Upkeep by payday (median):** 2,350 → 3,100 → 3,900 → 4,850 → 5,375.
- **Median cash, every 10 min:** 6.0k, 5.4k, 6.0k, 6.0k, 6.8k, 6.7k, 9.1k, 11.1k, 11.6k, 14.1k, 14.0k, 17.4k. Cash stays tight because everything goes back into the business.
- **Typical room order:** Garage 4m → Clinic 11m → Lounge 20m → Ops L1 26m → Workshop 36m → Ops L2 51m (Big Score) → Clinic L2 → Office L2 → Server Room → Lounge L2 → Lawyer …

## Key numbers
- **Economy:**
  - You start with 6,000 cash and 4 Street crew at levels 1–2.
  - Upkeep is **500 per crew per day**, plus room upkeep. A day lasts 24 min, and payday is at 06:00 (first payday at t = 23 min).
- **Progression:**
  - The tier unlocks fall at **0 / 9 / 16 / 24 successes**. Tier 3 and above also need Ops Room L1.
  - The Big Score needs **34 successes and Ops Room L2**.
  - The level ranges by tier are **1–2 / 2–3 / 4–5 / 6–8**, with XP curve `100 × L^1.3`.
  - XP rewards by tier are about 60 / 165 / 420 / 850, and 1000 for the Big Score.
- **Base limits:**
  - **1 active mission**. The Garage adds +1, the Office L2 adds +1 and the Ops Room L3 adds +1.
  - A roster cap of 4. The Lounge adds +2, +3 or +5 by level.
  - A board of 4.
- **Payout ranges (the Director takes 30%):**

  | Tier | Payout | Cost |
  |---|---|---|
  | 1 | 1.6–3.2k | 0–200 |
  | 2 | 8.8–15.8k | 0.6–2.4k |
  | 3 | 31–53k | 4.5–9k |
  | 4 | 84–133k | 8–15k |

- **Arrest risk:** 0.04–0.06 at tier 1, 0.08–0.10 at tier 2, 0.20–0.30 at tier 3, 0.35–0.45 at tier 4 and the Big Score. `outcome.bustedBand` is 0.4, so 40% of the failure range is Busted.
- **Room L1 costs:**

  | Room | L1 cost |
  |---|---|
  | Garage | 2.0k |
  | Clinic | 3.0k |
  | Lounge | 3.5k |
  | Ops | 3.5k |
  | Workshop | 4.0k |
  | Server | 7.0k |
  | Lawyer | 8.0k |

  The L1 → L3 costs scale from 6k to 20k. Build times run from 60 s to 300 s.

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
