# Data schema v2: realtime crew & missions

> **LEAD-OWNED contract (Wave R1).** Agent B writes the data and Agent A writes the loader. Both build against this file.
> If a field is missing, the loader uses the default given here and logs a warning. Unknown fields are ignored.
> Ids are `snake_case`. Icons are Lucide names, which must exist in `Content/Fonts/lucide_codepoints.json`
> (validate.py checks this). **Time is always in run seconds**, which are real seconds at time scale 1.
> The UI is icon-first, so every display string must be **short**: names ≤ 22 chars, `description` ≤ 40 chars,
> flavour ≤ 60 chars, level effects ≤ 28 chars.

Files are all in `Content/Data/`: `balance.json`, `criminal_types.json`, `names.json`, `abilities.json`, `traits.json`,
`missions.json`, `gear.json`, `rooms.json`, `perks.json`.

---

## Enums (match C++ `DCTypes.h` names exactly)

- **type:** `Wheelman Muscle Hacker Ghost Face Safecracker`
- **threat:** `Guards Cameras Alarms Locks Vault Patrols Witnesses Pursuit`
- **quality:** `Street Pro Elite`
- **slot:** `Tool Outfit Gadget`
- **room:** `Office Lounge Workshop Garage OpsRoom Clinic ServerRoom LawyerOffice`

---

## Effects vocabulary (shared by traits, gear traits, perks and rooms)

An effect is `{ "kind": "<kind>", "value": <number>, "param": "<optional string>", "threshold": <optional number> }`.

**Scope rules:**
- **Trait** effects apply to a mission when a crew member *on that mission* has the trait, innate or from gear.
- **Perk** effects are global.
- **Room** effects come from `rooms.json` per level (the values for the current built level).

When an effect appears more than once, it **stacks**: chance effects add; `*Mult` effects multiply.

| kind | Applies in | Meaning |
|---|---|---|
| `chance` | trait, perk | +value success chance (may be negative) |
| `chanceNight` | trait, perk | +value if the mission **starts** at night |
| `chanceDay` | trait, perk | +value if it starts by day |
| `chanceShort` | trait | +value if final duration ≤ `threshold` s |
| `chanceLong` | trait | +value if final duration ≥ `threshold` s |
| `chanceSolo` | trait | +value if CrewSize == 1 |
| `chanceThreat` | trait, perk | +value if the mission has threat `param` |
| `chanceWithType` | trait | +value if another crew member is of type `param` |
| `chanceTierMin` | trait, perk, room | +value if mission tier ≥ `threshold` |
| `chanceIfType` | room, perk | +value if any crew member is type `param` (e.g. Garage + Wheelman) |
| `durationMult` | trait, perk, room | multiplies duration (the product is clamped to ≥ `balance.minDurationMult`) |
| `payoutMult` | trait, perk, room | multiplies payout. Room/perk may use `param` = type: only if a crew member is that type |
| `costMult` | trait, perk | multiplies mission cost |
| `xpMult` | trait, perk | multiplies XP gained |
| `injuryChanceMult` | trait, perk, room | multiplies injury chance (a trait affects only its holder) |
| `injuryTimeMult` | perk, room | multiplies injury duration |
| `arrestMult` | trait, perk, room | multiplies arrest chance (a trait affects only its holder) |
| `fairCutAdd` | trait, perk | + to the holder's (trait) or everyone's (perk) fair cut |
| `willMin` | perk | willingness never drops below value |
| `loyaltyIdlePerDay` | room | + loyalty at dawn to crew who aren't injured |
| `rosterCap` | room, perk | + roster cap |
| `maxActive` | room, perk | + concurrent missions |
| `boardSize` | room, perk | + missions kept on the board |
| `postIntervalMult` | room, perk | multiplies the time between new postings |
| `gearSlots` | room | + gear slots per criminal (base `balance.gear.baseSlots`) |
| `gearTier` | room | max purchasable gear tier = value (highest wins) |
| `gearPriceMult` | room, perk | multiplies shop prices |
| `upkeepMult` | perk | multiplies the daily upkeep |
| `recruitLevelAdd` | perk | recruits roll + value levels |
| `recruitQualityAdd` | room, perk | shifts recruit quality odds up value steps (fractional = chance) |
| `recruitBoardAdd` | room, perk | + recruits listed on the desk |
| `preventFirstArrest` | perk | the first arrest of the run is prevented |
| `patchUp` | room | enables paid instant heal; cost = value × remaining injury seconds |
| `bail` | room | enables bail within `threshold` s of the arrest; cost = value × criminal level |
| `unlockTier` | room | allows missions of tier ≥ value to post (OpsRoom: 3) |
| `unlockFinal` | room | allows The Big Score to post |
| `lootChanceAdd` | perk, room | + loot chance |

---

## balance.json

```jsonc
{
  "startingCash": 3500,              // ~2 Street L2 hires or 1 Pro L2
  "startingRoster": 0,              // free starting crew (0..6, distinct types); 0 = hire your first crew
  "startingRosterLevels": [1, 2],
  "clock": { "startHour": 7.0, "dawnHour": 6.0, "duskHour": 20.0, "gameMinutesPerSecond": 1.0 },
  "autosaveSeconds": 30,

  "board": {
    "baseSize": 4,                  // missions kept on the board, not counting in-progress ones
    "postIntervalSec": 40,          // a new mission posts every N s while the board isn't full
    "expireSecRange": [300, 600],   // how long a posting stays up
    "tierWeights": [[1,0,0,0],[0.5,0.5,0,0],[0.3,0.4,0.3,0],[0.2,0.3,0.3,0.2]], // indexed by unlocked tier - 1
    "rareChance": 0.06,
    "rarePayoutMult": 2.5,  // optional: rare postings pay more ...
    "rareLevelAdd": 1,      // optional: ... are a level harder ...
    "rareExtraThreats": 1,  // optional: ... and add threats (pool first, then any)
    "startingBoard": ["corner_store", "car_boost", "pickpocket_run"]  // first missions (template ids); the rest random
  },
  "tiers": {
    "unlockSuccesses": [0, 3, 8, 15],   // successes needed for tiers 1..4 (tier 3+ also needs the OpsRoom room effect)
    "levelRange": [[1,2],[3,4],[5,7],[8,10]]
  },

  "success": {
    "threatPoints": 1.5,           // each threat is worth this many points; each crew slot is worth 1
    "levelFactor": { "atOrAbove": 1.0, "perLevelAbove": 0.05, "maxFactor": 1.2,
                     "below": [0.75, 0.5, 0.1] },  // 1, 2, 3+ levels under the mission level
    "maxChance": 2.0                // 100..200% = bonus odds
  },
  "cut": { "min": 0.1, "max": 0.9, "step": 0.05, "default": 0.3,
           "fairCutBase": 0.30, "fairCutLoyaltyDiv": 500,
           "willSlope": 3.0, "willMin": 0.2, "refusalThreshold": 0.45,
           "willFactorBase": 0.6, "willFactorScale": 0.4 },
  "outcome": { "bustedBand": 0.25,   // the worst 25% of the failure range is Busted
               "bonusPayoutMult": 1.5, "failXpMult": 0.25 },
  "injury": { "failChance": 0.35, "bustedChance": 0.6, "secondsRange": [90, 240] },
  "levels": { "max": 10, "xpBase": 100, "xpExp": 1.4 },
  "loyalty": { "start": 60, "successFair": 5, "greedy": -8, "injury": -3, "min": 0, "max": 100, "quitBelow": 10 },
  "upkeep": { "perCriminalPerDay": 300 },
  "recruits": { "boardSize": 4, "refreshSec": 240, "refreshCost": 400,
                "costBase": 1200, "costPerLevel": 500, "qualityCostMult": [1.0, 1.8, 3.0],
                "qualityOdds": [[0.8,0.18,0.02],[0.6,0.32,0.08],[0.45,0.4,0.15],[0.3,0.45,0.25]] },  // by unlocked tier
  "quality": { "abilities": [1, 1, 2], "traits": [1, 2, 3], "negativeTraitChance": [0.35, 0.2, 0.1] },
  "gear": { "baseSlots": 2, "sellRefund": 0.5, "lootChance": 0.12, "rareLootChance": 0.6 },
  "night": { "missionBonus": 0.10 },      // for missions with "night": true that start at night
  "minDurationMult": 0.5,
  "perks": { "everyNSuccesses": 3, "offerSize": 3 },
  "final": { "templateId": "the_big_score", "requiredSuccesses": 15 },
  "gameOver": { "cashFloor": 0 }        // at payday, if cash after upkeep < floor -> game over
}
```

**Success formula** (Agent A implements it exactly; the UI shows the parts as icons):
```
LevelFactor(c)  = EffLevel(c) >= M.Level ? min(maxFactor, atOrAbove + perLevelAbove*(EffLevel-M.Level))
                                         : below[min(3, ceil(M.Level-EffLevel)) - 1]
EffLevel(c)     = Level + Σ gear Power + perk adds
Points          = CrewSize + threatPoints * NumThreats
CrewChance      = Σ_c LevelFactor(c) / Points
Counters        = each threat instance is matched to at most one unused counter (ability or gear) from the crew;
                  greedy in threat order, preferring the lowest-level crew member's counter
ThreatChance    = threatPoints * Countered / Points
ModifierChance  = Σ chance effects (traits on crew, perks, rooms, night bonus)
Willingness(c)  = clamp(1 - willSlope * max(0, Cut - FairCut(c)), willMin, 1);  FairCut = fairCutBase + (Loyalty-50)/fairCutLoyaltyDiv + fairCutAdd
WillFactor      = willFactorBase + willFactorScale * mean(Willingness)      // refusal if any Willingness < refusalThreshold
Success         = clamp((CrewChance + ThreatChance + ModifierChance) * WillFactor, 0, maxChance)
Roll r ∈ [0,1):  r < min(Success,1) → Success (bonus payout if a 2nd roll < Success-1)
                 else Failed; Busted if r >= 1 - (1-min(Success,1)) * bustedBand
Arrest (Busted)  = mission.arrestRisk × arrestMult (per member)
```

## criminal_types.json
```jsonc
{ "types": [ { "type": "Wheelman", "abilities": ["getaway", "hotwire"], "traitPool": ["lead_foot", "night_owl", ...] }, ... ] }
```
Abilities: 2 per type, and between them the types must cover every threat at least twice. Street rolls 1 ability from the pool,
Elite gets both. `traitPool` takes positive and negative trait ids (negative ones are rolled with `quality.negativeTraitChance`).

## names.json
`{ "firstMale": [...], "firstFemale": [...], "first": [...], "last": [...], "nicknames": { "Wheelman": [...], ... } }`: modern,
gritty, international, ≥ 30 per list. The list a first name comes from sets the crew member's gender, which picks their
body; `first` holds names for anyone (gender from the crew member's seed). A first name sits in one list only.

## abilities.json
`{ "abilities": [ { "id": "loop_feed", "displayName": "Loop the Feed", "icon": "cctv", "counters": "Cameras" } ] }`

## traits.json
`{ "traits": [ { "id": "night_owl", "displayName": "Night Owl", "description": "+15% at night", "icon": "moon", "negative": false, "effects": [ { "kind": "chanceNight", "value": 0.15 } ] } ] }`
Aim for about 18 traits: environment/threat specialists, Night Owl, Early Bird, Lead Foot (durationMult 0.75),
Marathoner (chanceLong), Quick Hands (chanceShort), Lone Wolf (chanceSolo), Partners (chanceWithType), Treasure Hunter (payoutMult),
Fast Learner (xpMult), Tough (injuryChanceMult), Slippery (arrestMult), Haggler (costMult). Negatives: Greedy (fairCutAdd +0.1),
Hothead (chance −0.05), Rat (arrestMult 1.5 for the holder), Slow (durationMult 1.15).

## missions.json
```jsonc
{ "missions": [ {
  "id": "corner_store", "displayName": "Corner Store", "icon": "store",
  "flavour": ["Till's full on Fridays.", "...", "..."],     // 3 short variants
  "tier": 1,
  "crewSize": [1, 1],                 // inclusive range, rolled per posting
  "durationSec": [60, 90],            // rolled, rounded to 5 s
  "threats": { "fixed": ["Witnesses"], "pool": ["Cameras", "Alarms"], "extra": [0, 1] },  // fixed + n random from pool
  "cost": [0, 0], "payout": [1500, 2200], "xpReward": 40, "arrestRisk": 0.05,
  "nightChance": 0.3,                 // chance the posting is a night job
  "weight": 1.0,                      // posting weight within its tier
  "final": false
} ] }
```
**Design targets:**
- **Tier 1:** 1 crew, 60–120 s, 1–2 threats, small or no cost.
- **Tier 2:** 2 crew, 3–5 min.
- **Tier 3:** 3–4 crew, 8–12 min, real arrest risk.
- **Tier 4:** 4–5 crew, 15–20 min, high cost, high risk, high reward.
- **`the_big_score`:** tier 4, `final: true`, 5 crew, 20 min, 6–7 threats. Success wins the run.

## gear.json
```jsonc
{ "gear": [ { "id": "bolt_cutters", "displayName": "Bolt Cutters", "description": "Counters Locks", "icon": "scissors",
  "slot": "Tool", "price": 800, "power": 0, "counters": "Locks", "trait": null, "tier": 1, "loot": false } ] }
```
- **Tools** counter threats. **Outfits** grant a trait (a `traits.json` id). **Gadgets** give `power` (0.5–2.0), sometimes a counter.
- Tier 1 is buyable from the start. Tier 2 needs `gearTier` 2 (Workshop L1), and tier 3 needs Workshop L3.
- `loot: true` items are never in the shop; they drop from missions only. About 15 shop items and 5 loot items.

## rooms.json
```jsonc
{ "rooms": [ { "type": "Garage", "displayName": "Garage", "description": "Run more jobs at once", "icon": "warehouse",
  "cost": [6000, 12000, 22000], "buildSec": [60, 120, 180], "requiredSuccesses": [2, 5, 9],
  "upkeepPerDay": [200, 300, 450],
  "levelEffects": ["+1 active mission", "+1 active, -10% time", "+1 active, -20% time"],
  "levels": [ { "effects": [ { "kind": "maxActive", "value": 1 } ] }, { "effects": [...] }, { "effects": [...] } ] } ] }
```
- Effects are **per level, not cumulative**: list the full effect set for each level.
- The Office starts Built L1. Its levels raise `boardSize`; its L1 entry is free (`cost[0] = 0`).
- Base values (before any room): roster cap 4, max active missions 1, board 4.
- Build times are short at first (60 s) and longer later (up to about 5 min). Only one room builds at a time.

## perks.json
`{ "perks": [ { "id": "burner_phones", "displayName": "Burner Phones", "description": "-20% arrest chance", "icon": "smartphone", "effects": [ { "kind": "arrestMult", "value": 0.8 } ] } ] }`
About 15 perks, modern-crime flavoured. No film or theatre puns.

---

## Clarifications (lead, Nov 13, after Wave R1 Data)
1. **`unlockTier: v`:** tiers ≥ v may post once their `tiers.unlockSuccesses` count is met. Until a room grants it, tiers ≥ 3 never post.
2. **The Big Score:** once unlocked (a room with `unlockFinal` plus `final.requiredSuccesses`), it is an extra board entry. It never expires, doesn't count toward board size, stays posted after a failure, and there is at most one.
3. **Recruit levels:** rolled from `tiers.levelRange[unlockedTier-1]`, plus `recruitLevelAdd`.
4. **Negative traits:** `negativeTraitChance` adds **one** negative trait on top of the positive count from `quality.traits`.
5. **Loyalty:** `greedy` applies at mission start to each member where Cut > FairCut. `successFair` applies on Success to each member where Cut ≤ FairCut.
6. **Night jobs:** a job counts as a night job if it starts when hour ≥ dusk or hour < dawn.
7. **Director's money:** the Director pays the full mission cost and receives Payout × Cut.
8. **Empty room levels:** an empty `effects` list for a room level is valid.
9. **Threat coverage:** ≥ 2 counters per threat counting abilities **and** gear. 12 single-threat abilities can't cover all 8 threats twice.
