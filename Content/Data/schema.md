# Director's Cut — data schema (LEAD-OWNED contract)

All files live in `Content/Data/`, UTF-8 JSON, loaded at runtime from `FPaths::ProjectContentDir()/Data/`.
Enum values are written as strings exactly as in `DCTypes.h`:

- **types**: `Wheelman, Muscle, Hacker, Ghost, Face, Safecracker`
- **stats**: `Driving, Muscle, Tech, Stealth, Charm, Finesse`
- **slots**: `Tool, Outfit, Gadget`
- **rooms**: `Office, Lounge, Workshop, Garage, ScreeningRoom, Clinic, ServerCloset, LawyerOffice`

Unknown keys are ignored by the loader (add `"_comment"` freely). Missing optional keys use the defaults shown.
Money is integer dollars. Probabilities and percentages are floats from 0 to 1.

---

## criminal_types.json
```json
{ "types": [
  { "type": "Wheelman", "displayName": "Wheelman", "fantasy": "Getaway, car boosts",
    "primary": "Driving", "secondary": "Muscle",
    "primaryRange": [45, 65], "secondaryRange": [30, 45], "otherRange": [10, 30],
    "luckRange": [0, 3], "loyaltyRange": [35, 65],
    "hireCostBase": 3500,
    "props": "flat cap" }
] }
```
There must be exactly 6 entries.

## names.json
```json
{ "first": ["Vinnie", "..."], "last": ["Caruso", "..."], "nicknames": ["The Wrench", "..."],
  "typeNicknames": { "Hacker": ["Modem", "..."] } }
```
The minimums are 40 first names, 40 last names and 60 nicknames. `typeNicknames` is optional; when present, the loader picks from it 50% of the time.

## jobs.json
```json
{ "jobs": [
  { "id": "corner_store", "displayName": "Corner Store Stickup", "tier": 1,
    "requiredRoles": ["Muscle"], "bonusRoles": ["Wheelman"],
    "minCrew": 1, "maxCrew": 2,
    "statWeights": { "Muscle": 0.6, "Driving": 0.4 },
    "baseDifficulty": 40, "payoutRange": [4000, 7000],
    "xpReward": 40, "arrestRisk": 0.15,
    "heavy": false, "finalCut": false,
    "flavour": ["Three variants of the 'script'...", "...", "..."] }
] }
```
- `statWeights` must sum to 1 (±0.01).
- `tier` is the job's minimum tier. Jobs appear on the board when `tier <= unlockedTier`. A job above tier 1 can also appear "upscaled": its payout is multiplied by `tierPayoutMult^(boardTier - 1)` and its difficulty is scaled by the formula.
- `heavy: true` means the job needs the Garage.
- `finalCut: true` (exactly one job: "The Premiere") means the job needs the Screening Room plus `balance.finalCut.requiredHeists`. It is always offered on the board once unlocked.

## gear.json
```json
{ "gear": [
  { "id": "crowbar", "displayName": "Crowbar", "description": "Persuasive, in a blunt way.",
    "slot": "Tool", "price": 800, "tier": 1,
    "stats": { "Muscle": 8 }, "luck": 0, "arrestRiskMult": 1.0,
    "typeAffinity": "Muscle" }
] }
```
- `tier`: 1 = office fence crate (always available); 2 = needs the Workshop L1; 3 = needs the Workshop L2.
- There must be at least 4 tier-1 items. Aim for about 15 items in total.
- `typeAffinity` is optional and only used by perks such as "Practical Effects".

## rooms.json
```json
{ "rooms": [
  { "type": "Lounge", "displayName": "The Lounge", "priority": "Must",
    "description": "Bunks, a bar, a pool table with a lean.",
    "buildCost": 8000, "upgradeCosts": [6000, 10000], "requiredHeists": 0,
    "upkeepPerNight": 200,
    "levels": [
      { "effectText": "Roster cap 6. Idle crew +1 loyalty/night.", "flavour": "...",
        "modifiers": [ { "kind": "RosterCap", "value": 6 }, { "kind": "IdleLoyalty", "value": 1 } ] },
      { "effectText": "Roster cap 8.", "modifiers": [ { "kind": "RosterCap", "value": 8 }, { "kind": "IdleLoyalty", "value": 1 } ] },
      { "effectText": "Roster cap 10.", "modifiers": [ { "kind": "RosterCap", "value": 10 }, { "kind": "IdleLoyalty", "value": 1 } ] }
    ] }
] }
```
- There must be 8 rooms. The Office has `buildCost: 0` and starts built at L1.
- **Level modifiers are not cumulative.** Each level lists the complete set of modifiers active at that level.

## perks.json
```json
{ "perks": [
  { "id": "method_acting", "displayName": "Method Acting",
    "description": "Faces get +10 Charm.", "flavour": "Stay in character.",
    "modifiers": [ { "kind": "StatBonus", "stat": "Charm", "value": 10, "type": "Face" } ] }
] }
```
There must be at least 15 perks.

## Modifier kinds (shared by rooms and perks)

The Sim implements every kind listed here. Fields other than `kind` are optional unless marked otherwise.

| kind | fields | effect |
|---|---|---|
| `RosterCap` | value | sets the roster cap (uses the max over all active modifiers) |
| `CrewsPerNight` | value | sets the crews per night (max) |
| `JobBoardSize` | value (additive when it comes from a perk, set/max when it comes from a room) | number of jobs on the board |
| `IdleLoyalty` | value | loyalty per night for idle (available, uncommitted) criminals |
| `GearSlots` | value | number of usable gear slots (Office = 2, Workshop = 3) |
| `GearTier` | value | maximum gear tier that can be bought |
| `GearDiscount` | value (0.2 = −20%), `type`? (gear `typeAffinity`) | price multiplier `(1 − value)` |
| `StatBonus` | stat, value, `type`? | + to the effective stat (for criminals of `type`, or for everyone) |
| `LuckBonus` | value, `type`? | + to luck |
| `ChanceBonus` | value, `condition`? | additive success chance. `condition` is one of: `HasWheelman`, `AllDifferentTypes`, `PerExtraCrew` (value × crew beyond the minimum), `TierAtLeast3` |
| `PayoutMult` | value (1.15), `condition`? (`TierAtLeast3`, `HackerJob`) | payout multiplier |
| `UpkeepMult` | value | upkeep multiplier |
| `ArrestMult` | value (0.85 = −15%) | arrest chance multiplier |
| `InjuryMult` | value | injury chance multiplier |
| `InjuryHealRate` | value (2 = twice as fast) | injured nights decrease by this much per night |
| `FairCutBonus` | value (0.05) | added to everyone's FairCut |
| `MinWillingness` | value (0.6) | floor for willingness (perk "Auteur") |
| `RecruitLevelBonus` | value | recruits arrive with + levels |
| `PreventFirstArrest` | — | the first arrest of the run is prevented (perk "Stunt Double") |
| `Retakes` | value | retakes per night |
| `UnlockHeavyJobs` | — | heavy jobs can appear |
| `UnlockFinalCut` | — | the Final Cut can appear (together with the heists requirement) |
| `Intel` | — | breakdown shows the exact arrest chance and bands (the UI shows "?" otherwise) |
| `PatchUp` | value (cost per injured night) | Clinic: heal for cash |
| `Bail` | value (cost multiplier × hire cost) | Lawyer: bail arrested crew |
| `Dailies` | — | once per night, preview a job's exact outcome (Sim may implement this as "show roll") |
| `Overtime` | value (loyalty cost) | criminals may do 2 jobs per night |

## balance.json
Every constant from PLAN §2.7–2.10. Key names:
```json
{
  "startingCash": 25000, "startingRoster": 4, "startingBoard": 4, "recruitBoardSize": 3, "recruitRefreshNights": 3,
  "cutMin": 0.10, "cutMax": 0.90, "cutStep": 0.05, "defaultCut": 0.30,
  "supportWeight": 0.25,
  "difficultyPerHeist": 0.10, "difficultyPerTier": 0.05,
  "skillBase": 0.55, "skillSlope": 0.75, "skillMin": 0.05, "skillMax": 0.95,
  "roleFitPer": 0.03, "roleFitCap": 0.09,
  "fairCutBase": 0.30, "fairCutLoyaltyDiv": 500,
  "willSlope": 2.0, "willMin": 0.2, "refusalThreshold": 0.35,
  "willFactorBase": 0.60, "willFactorScale": 0.40,
  "generosityBonus": 0.04,
  "successMin": 0.02, "successMax": 0.97,
  "luckyBreakPerLuck": 0.02,
  "cleanBand": 0.6, "bustedBand": 0.10,
  "messyInjuryChance": 0.25, "messyInjuryNights": [1, 2],
  "failedInjuryChance": 0.5, "failedInjuryNights": [1, 3],
  "bustedArrestMult": 2.0,
  "xpOutcomeMult": { "Clean": 1.0, "Messy": 0.7, "Failed": 0.25, "Busted": 0.25 },
  "levelCurveBase": 100, "levelCurveExp": 1.4, "maxLevel": 10,
  "levelUp": { "primary": 4, "secondary": 2, "other": 1, "luckEveryNLevels": 2 },
  "loyalty": { "successFair": 5, "greedy": -8, "injury": -3, "min": 0, "max": 100 },
  "tierUnlockHeists": [0, 3, 7, 12], "tierPayoutMult": 1.6,
  "upkeepPerCriminal": 300, "upkeepPerHeistBlock": 1000, "upkeepHeistBlock": 4,
  "arrestRiskPer5Heists": 0.02,
  "recruitCostTierMult": 1.4, "recruitStatBonusPerTier": 5,
  "perkEveryNHeists": 3, "perkOfferSize": 3,
  "baseRosterCap": 4, "baseCrewsPerNight": 1, "baseGearSlots": 2, "baseJobBoard": 3,
  "finalCut": { "requiredHeists": 12 }
}
```

## strings.json
```json
{
  "stamps": { "Clean": "IT'S A WRAP", "Messy": "NEEDS FIXING IN POST", "Failed": "CUT!", "Busted": "SHUT DOWN" },
  "outcomeLines": { "Clean": ["{name} got out clean, smelling of diamonds.", "..."], "Messy": ["..."], "Failed": ["..."], "Busted": ["..."] },
  "luckyBreak": ["Lucky break! {name} found the back door unlocked."],
  "gameOverReasons": { "Broke": "Couldn't make payroll.", "NoCrew": "Nobody left to cast." },
  "winReason": "The Premiere was a smash.",
  "titleTagline": "Your vision. Your edits. Your final cut.",
  "tutorial": ["Click the Job Board to pick a production.", "Cast your crew and set your cut.", "Press ROLL CAMERA to run the night."],
  "credits": { "starringHeader": "Starring", "memoriamHeader": "In Memoriam", "directedBy": "Directed by You" },
  "nightEvents": { "tierUnlocked": "Tier {tier} productions unlocked!", "recruitsRefreshed": "New faces at the desk." }
}
```
`{name}`, `{job}`, `{tier}` and `{amount}` are substituted by the Sim/UI.
