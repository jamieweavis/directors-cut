#!/usr/bin/env python3
"""Validate Content/Data/*.json against schema.md. Dependency-free.

Usage: python3 Content/Data/validate.py      (exit code 1 on any error)
"""
import json
import os
import sys

DATA = os.path.dirname(os.path.abspath(__file__))

TYPES = ["Wheelman", "Muscle", "Hacker", "Ghost", "Face", "Safecracker"]
STATS = ["Driving", "Muscle", "Tech", "Stealth", "Charm", "Finesse"]
SLOTS = ["Tool", "Outfit", "Gadget"]
ROOMS = ["Office", "Lounge", "Workshop", "Garage", "ScreeningRoom", "Clinic", "ServerCloset", "LawyerOffice"]
PRIORITIES = ["Must", "Should", "Could"]
BANDS = ["Clean", "Messy", "Failed", "Busted"]

# kind -> (required fields, optional fields)
MODIFIER_KINDS = {
    "RosterCap": (["value"], []),
    "CrewsPerNight": (["value"], []),
    "JobBoardSize": (["value"], []),
    "IdleLoyalty": (["value"], []),
    "GearSlots": (["value"], []),
    "GearTier": (["value"], []),
    "GearDiscount": (["value"], ["type"]),
    "StatBonus": (["stat", "value"], ["type"]),
    "LuckBonus": (["value"], ["type"]),
    "ChanceBonus": (["value"], ["condition"]),
    "PayoutMult": (["value"], ["condition"]),
    "UpkeepMult": (["value"], []),
    "ArrestMult": (["value"], []),
    "InjuryMult": (["value"], []),
    "InjuryHealRate": (["value"], []),
    "FairCutBonus": (["value"], []),
    "MinWillingness": (["value"], []),
    "RecruitLevelBonus": (["value"], []),
    "PreventFirstArrest": ([], []),
    "Retakes": (["value"], []),
    "UnlockHeavyJobs": ([], []),
    "UnlockFinalCut": ([], []),
    "Intel": ([], []),
    "PatchUp": (["value"], []),
    "Bail": (["value"], []),
    "Dailies": ([], []),
    "Overtime": (["value"], []),
}
CHANCE_CONDITIONS = ["HasWheelman", "AllDifferentTypes", "PerExtraCrew", "TierAtLeast3"]
PAYOUT_CONDITIONS = ["TierAtLeast3", "HackerJob"]

BALANCE_KEYS = {
    "startingCash": int, "startingRoster": int, "startingBoard": int, "recruitBoardSize": int, "recruitRefreshNights": int,
    "cutMin": float, "cutMax": float, "cutStep": float, "defaultCut": float,
    "supportWeight": float,
    "difficultyPerHeist": float, "difficultyPerTier": float,
    "skillBase": float, "skillSlope": float, "skillMin": float, "skillMax": float,
    "roleFitPer": float, "roleFitCap": float,
    "fairCutBase": float, "fairCutLoyaltyDiv": float,
    "willSlope": float, "willMin": float, "refusalThreshold": float,
    "willFactorBase": float, "willFactorScale": float,
    "generosityBonus": float,
    "successMin": float, "successMax": float,
    "luckyBreakPerLuck": float,
    "cleanBand": float, "bustedBand": float,
    "messyInjuryChance": float, "messyInjuryNights": list,
    "failedInjuryChance": float, "failedInjuryNights": list,
    "bustedArrestMult": float,
    "xpOutcomeMult": dict,
    "levelCurveBase": float, "levelCurveExp": float, "maxLevel": int,
    "levelUp": dict, "loyalty": dict,
    "tierUnlockHeists": list, "tierPayoutMult": float,
    "upkeepPerCriminal": int, "upkeepPerHeistBlock": int, "upkeepHeistBlock": int,
    "arrestRiskPer5Heists": float,
    "recruitCostTierMult": float, "recruitStatBonusPerTier": float,
    "perkEveryNHeists": int, "perkOfferSize": int,
    "baseRosterCap": int, "baseCrewsPerNight": int, "baseGearSlots": int, "baseJobBoard": int,
    "finalCut": dict,
}
PROBABILITY_KEYS = [
    "cutMin", "cutMax", "cutStep", "defaultCut", "supportWeight", "skillBase", "skillMin", "skillMax",
    "roleFitPer", "roleFitCap", "fairCutBase", "willMin", "refusalThreshold", "willFactorBase", "willFactorScale",
    "generosityBonus", "successMin", "successMax", "luckyBreakPerLuck", "cleanBand", "bustedBand",
    "messyInjuryChance", "failedInjuryChance", "arrestRiskPer5Heists",
]

errors = []
warnings = []


def err(where, msg):
    errors.append(f"{where}: {msg}")


def warn(where, msg):
    warnings.append(f"{where}: {msg}")


def is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def load(name):
    path = os.path.join(DATA, name)
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        err(name, "file missing")
    except json.JSONDecodeError as e:
        err(name, f"invalid JSON: {e}")
    return None


def req(obj, keys, where):
    ok = True
    for k in keys:
        if k not in obj:
            err(where, f"missing key '{k}'")
            ok = False
    return ok


def check_range(v, where, lo=None, hi=None, integer=False):
    if not (is_int(v) if integer else is_num(v)):
        err(where, f"expected {'int' if integer else 'number'}, got {v!r}")
        return False
    if lo is not None and v < lo:
        err(where, f"{v} < {lo}")
    if hi is not None and v > hi:
        err(where, f"{v} > {hi}")
    return True


def check_pair(v, where, lo=None, hi=None, integer=True):
    if not (isinstance(v, list) and len(v) == 2):
        err(where, f"expected [min, max], got {v!r}")
        return
    for x in v:
        check_range(x, where, lo, hi, integer)
    if is_num(v[0]) and is_num(v[1]) and v[0] > v[1]:
        err(where, f"min > max in {v}")


def check_unique(items, key, where):
    seen = set()
    for it in items:
        k = it.get(key)
        if k in seen:
            err(where, f"duplicate {key} '{k}'")
        seen.add(k)


def check_modifier(m, where):
    if not isinstance(m, dict) or "kind" not in m:
        err(where, f"modifier without kind: {m!r}")
        return
    kind = m["kind"]
    if kind not in MODIFIER_KINDS:
        err(where, f"unknown modifier kind '{kind}'")
        return
    required, optional = MODIFIER_KINDS[kind]
    for f in required:
        if f not in m:
            err(where, f"{kind} missing '{f}'")
    for f in m:
        if f not in ["kind"] + required + optional and not f.startswith("_"):
            warn(where, f"{kind} has unexpected field '{f}' (ignored by loader)")
    if "value" in m and not is_num(m["value"]):
        err(where, f"{kind} value not a number")
    if "stat" in m and m["stat"] not in STATS:
        err(where, f"{kind} bad stat '{m['stat']}'")
    if "type" in m and m["type"] not in TYPES:
        err(where, f"{kind} bad type '{m['type']}'")
    if kind == "ChanceBonus" and "condition" in m and m["condition"] not in CHANCE_CONDITIONS:
        err(where, f"ChanceBonus bad condition '{m['condition']}'")
    if kind == "PayoutMult" and "condition" in m and m["condition"] not in PAYOUT_CONDITIONS:
        err(where, f"PayoutMult bad condition '{m['condition']}'")
    v = m.get("value")
    if is_num(v):
        if kind in ("GearDiscount", "FairCutBonus", "MinWillingness", "ChanceBonus") and not (0 <= v <= 1):
            err(where, f"{kind} value {v} outside 0..1")
        if kind in ("PayoutMult", "UpkeepMult", "ArrestMult", "InjuryMult", "InjuryHealRate", "Bail") and v <= 0:
            err(where, f"{kind} multiplier must be > 0")
        if kind in ("RosterCap", "CrewsPerNight", "JobBoardSize", "GearSlots", "GearTier", "Retakes", "RecruitLevelBonus") and not is_int(v):
            err(where, f"{kind} value should be an int")
        if kind == "GearSlots" and not (1 <= v <= 3):
            err(where, "GearSlots must be 1..3")
        if kind == "GearTier" and not (1 <= v <= 3):
            err(where, "GearTier must be 1..3")


def check_types():
    d = load("criminal_types.json")
    if d is None:
        return
    types = d.get("types", [])
    if len(types) != 6:
        err("criminal_types", f"need exactly 6 types, got {len(types)}")
    check_unique(types, "type", "criminal_types")
    names = {t.get("type") for t in types}
    for t in TYPES:
        if t not in names:
            err("criminal_types", f"missing type {t}")
    for t in types:
        w = f"criminal_types[{t.get('type')}]"
        if not req(t, ["type", "displayName", "fantasy", "primary", "secondary", "primaryRange", "secondaryRange",
                       "otherRange", "luckRange", "loyaltyRange", "hireCostBase", "props"], w):
            continue
        if t["type"] not in TYPES:
            err(w, "bad type")
        for k in ("primary", "secondary"):
            if t[k] not in STATS:
                err(w, f"bad {k} stat '{t[k]}'")
        if t["primary"] == t["secondary"]:
            err(w, "primary == secondary")
        for k in ("primaryRange", "secondaryRange", "otherRange"):
            check_pair(t[k], f"{w}.{k}", 1, 100)
        check_pair(t["luckRange"], f"{w}.luckRange", 0, 10)
        check_pair(t["loyaltyRange"], f"{w}.loyaltyRange", 0, 100)
        check_range(t["hireCostBase"], f"{w}.hireCostBase", 1, None, integer=True)


def check_names():
    d = load("names.json")
    if d is None:
        return
    for k, n in (("first", 40), ("last", 40), ("nicknames", 60)):
        lst = d.get(k)
        if not isinstance(lst, list):
            err("names", f"missing list '{k}'")
            continue
        if len(lst) < n:
            err("names", f"'{k}' has {len(lst)} < {n}")
        if len(set(lst)) != len(lst):
            dup = sorted({x for x in lst if lst.count(x) > 1})
            err("names", f"'{k}' has duplicates: {dup}")
        if any(not isinstance(x, str) or not x.strip() for x in lst):
            err("names", f"'{k}' has empty/non-string entries")
    tn = d.get("typeNicknames")
    if tn is not None:
        if not isinstance(tn, dict):
            err("names", "typeNicknames must be an object")
        else:
            for k, v in tn.items():
                if k not in TYPES:
                    err("names", f"typeNicknames bad type '{k}'")
                if not isinstance(v, list) or not v:
                    err("names", f"typeNicknames[{k}] must be a non-empty list")


def check_jobs():
    d = load("jobs.json")
    if d is None:
        return
    jobs = d.get("jobs", [])
    if len(jobs) < 9:
        err("jobs", f"expected 8 templates + The Premiere, got {len(jobs)}")
    check_unique(jobs, "id", "jobs")
    finals = [j for j in jobs if j.get("finalCut") is True]
    if len(finals) != 1:
        err("jobs", f"exactly one finalCut job required, got {len(finals)}")
    for j in jobs:
        w = f"jobs[{j.get('id')}]"
        if not req(j, ["id", "displayName", "tier", "requiredRoles", "bonusRoles", "minCrew", "maxCrew", "statWeights",
                       "baseDifficulty", "payoutRange", "xpReward", "arrestRisk", "heavy", "finalCut", "flavour"], w):
            continue
        check_range(j["tier"], f"{w}.tier", 1, 4, integer=True)
        for k in ("requiredRoles", "bonusRoles"):
            if not isinstance(j[k], list):
                err(w, f"{k} must be a list")
                continue
            for r in j[k]:
                if r not in TYPES:
                    err(w, f"{k} bad type '{r}'")
        if not j["requiredRoles"]:
            err(w, "requiredRoles empty")
        if set(j["requiredRoles"]) & set(j["bonusRoles"]):
            warn(w, "a role is both required and bonus")
        check_range(j["minCrew"], f"{w}.minCrew", 1, 6, integer=True)
        check_range(j["maxCrew"], f"{w}.maxCrew", 1, 6, integer=True)
        if is_int(j["minCrew"]) and is_int(j["maxCrew"]):
            if j["minCrew"] > j["maxCrew"]:
                err(w, "minCrew > maxCrew")
            if len(j["requiredRoles"]) > j["maxCrew"]:
                err(w, "more required roles than maxCrew")
            if len(j["requiredRoles"]) > j["minCrew"]:
                err(w, "minCrew smaller than number of required roles")
        sw = j["statWeights"]
        if not isinstance(sw, dict) or not sw:
            err(w, "statWeights must be a non-empty object")
        else:
            for s, v in sw.items():
                if s not in STATS:
                    err(w, f"statWeights bad stat '{s}'")
                check_range(v, f"{w}.statWeights.{s}", 0, 1)
            total = sum(v for v in sw.values() if is_num(v))
            if abs(total - 1.0) > 0.01:
                err(w, f"statWeights sum to {total:.3f}, not 1")
        check_range(j["baseDifficulty"], f"{w}.baseDifficulty", 1, 500)
        check_pair(j["payoutRange"], f"{w}.payoutRange", 0, None)
        check_range(j["xpReward"], f"{w}.xpReward", 0, None, integer=True)
        check_range(j["arrestRisk"], f"{w}.arrestRisk", 0, 1)
        for k in ("heavy", "finalCut"):
            if not isinstance(j[k], bool):
                err(w, f"{k} must be a bool")
        fl = j["flavour"]
        if not (isinstance(fl, list) and len(fl) >= 3 and all(isinstance(x, str) and x for x in fl)):
            err(w, "flavour must be a list of >= 3 strings")
        if j["finalCut"] is True:
            if j["tier"] != 4:
                err(w, "The Premiere should be tier 4")
            need = {"Face", "Ghost", "Safecracker", "Hacker", "Wheelman"}
            if not need <= set(j["requiredRoles"]):
                err(w, f"finalCut job must require {sorted(need)}")
            if j["minCrew"] < 5:
                err(w, "finalCut job needs minCrew >= 5")
    tiers = {j.get("tier") for j in jobs if not j.get("finalCut")}
    if 1 not in tiers:
        err("jobs", "no tier-1 jobs")


def check_gear():
    d = load("gear.json")
    if d is None:
        return
    gear = d.get("gear", [])
    check_unique(gear, "id", "gear")
    if len(gear) < 12:
        warn("gear", f"only {len(gear)} items (aim for ~15)")
    t1 = [g for g in gear if g.get("tier") == 1]
    if len(t1) < 4:
        err("gear", f"need >= 4 tier-1 items, got {len(t1)}")
    t1_stats = set()
    for g in t1:
        t1_stats |= set((g.get("stats") or {}).keys())
    if len(t1_stats) < 3:
        err("gear", "tier-1 items should cover at least 3 different stats")
    slots_seen = set()
    for g in gear:
        w = f"gear[{g.get('id')}]"
        if not req(g, ["id", "displayName", "description", "slot", "price", "tier", "stats", "luck", "arrestRiskMult"], w):
            continue
        if g["slot"] not in SLOTS:
            err(w, f"bad slot '{g['slot']}'")
        slots_seen.add(g["slot"])
        check_range(g["price"], f"{w}.price", 1, None, integer=True)
        check_range(g["tier"], f"{w}.tier", 1, 3, integer=True)
        if not isinstance(g["stats"], dict):
            err(w, "stats must be an object")
        else:
            for s, v in g["stats"].items():
                if s not in STATS:
                    err(w, f"bad stat '{s}'")
                check_range(v, f"{w}.stats.{s}", -50, 50, integer=True)
        check_range(g["luck"], f"{w}.luck", 0, 10, integer=True)
        check_range(g["arrestRiskMult"], f"{w}.arrestRiskMult", 0.01, 2)
        if "typeAffinity" in g and g["typeAffinity"] not in TYPES:
            err(w, f"bad typeAffinity '{g['typeAffinity']}'")
        if not g["stats"] and g["luck"] == 0 and g["arrestRiskMult"] == 1.0:
            warn(w, "item has no effect")
    for s in SLOTS:
        if s not in slots_seen:
            err("gear", f"no items for slot {s}")


def check_rooms():
    d = load("rooms.json")
    if d is None:
        return
    rooms = d.get("rooms", [])
    if len(rooms) != 8:
        err("rooms", f"need exactly 8 rooms, got {len(rooms)}")
    check_unique(rooms, "type", "rooms")
    present = {r.get("type") for r in rooms}
    for r in ROOMS:
        if r not in present:
            err("rooms", f"missing room {r}")
    for r in rooms:
        w = f"rooms[{r.get('type')}]"
        if not req(r, ["type", "displayName", "priority", "description", "buildCost", "upgradeCosts", "requiredHeists",
                       "upkeepPerNight", "levels"], w):
            continue
        if r["type"] not in ROOMS:
            err(w, "bad room type")
        if r["priority"] not in PRIORITIES:
            err(w, f"bad priority '{r['priority']}'")
        check_range(r["buildCost"], f"{w}.buildCost", 0, None, integer=True)
        if r["type"] == "Office" and r["buildCost"] != 0:
            err(w, "Office buildCost must be 0")
        if r["type"] != "Office" and r["buildCost"] <= 0:
            err(w, "buildCost must be > 0")
        uc = r["upgradeCosts"]
        if not (isinstance(uc, list) and len(uc) == 2):
            err(w, "upgradeCosts must be [L2, L3]")
        else:
            for c in uc:
                check_range(c, f"{w}.upgradeCosts", 1, None, integer=True)
        check_range(r["requiredHeists"], f"{w}.requiredHeists", 0, 50, integer=True)
        check_range(r["upkeepPerNight"], f"{w}.upkeepPerNight", 0, None, integer=True)
        lv = r["levels"]
        if not (isinstance(lv, list) and len(lv) == 3):
            err(w, "levels must have exactly 3 entries")
            continue
        for i, L in enumerate(lv):
            lw = f"{w}.levels[{i}]"
            if not req(L, ["effectText", "modifiers"], lw):
                continue
            if "flavour" not in L:
                warn(lw, "no flavour")
            kinds = []
            for m in L["modifiers"]:
                check_modifier(m, lw)
                kinds.append(m.get("kind"))
            if i > 0:
                prev = {m.get("kind") for m in lv[i - 1].get("modifiers", [])}
                missing = prev - set(kinds)
                if missing:
                    err(lw, f"levels are not cumulative - level {i + 1} drops {sorted(missing)} present at level {i}")


def check_perks():
    d = load("perks.json")
    if d is None:
        return
    perks = d.get("perks", [])
    if len(perks) < 15:
        err("perks", f"need >= 15 perks, got {len(perks)}")
    check_unique(perks, "id", "perks")
    for p in perks:
        w = f"perks[{p.get('id')}]"
        if not req(p, ["id", "displayName", "description", "flavour", "modifiers"], w):
            continue
        if not p["modifiers"]:
            err(w, "no modifiers")
        for m in p["modifiers"]:
            check_modifier(m, w)


def check_balance():
    d = load("balance.json")
    if d is None:
        return
    w = "balance"
    for k, t in BALANCE_KEYS.items():
        if k not in d:
            err(w, f"missing key '{k}'")
            continue
        v = d[k]
        if t is int and not is_int(v):
            err(w, f"{k} should be int")
        elif t is float and not is_num(v):
            err(w, f"{k} should be a number")
        elif t in (list, dict) and not isinstance(v, t):
            err(w, f"{k} should be {t.__name__}")
    for k in PROBABILITY_KEYS:
        if k in d and is_num(d[k]) and not (0 <= d[k] <= 1):
            err(w, f"{k}={d[k]} outside 0..1")
    if all(k in d for k in ("cutMin", "defaultCut", "cutMax")) and not d["cutMin"] <= d["defaultCut"] <= d["cutMax"]:
        err(w, "defaultCut outside [cutMin, cutMax]")
    for a, b in (("skillMin", "skillMax"), ("successMin", "successMax")):
        if a in d and b in d and d[a] >= d[b]:
            err(w, f"{a} >= {b}")
    if "willFactorBase" in d and "willFactorScale" in d and abs(d["willFactorBase"] + d["willFactorScale"] - 1.0) > 1e-6:
        warn(w, "willFactorBase + willFactorScale != 1 (full willingness won't give x1.0)")
    for k in ("messyInjuryNights", "failedInjuryNights"):
        if isinstance(d.get(k), list):
            check_pair(d[k], f"{w}.{k}", 0, 10)
    xm = d.get("xpOutcomeMult", {})
    for b in BANDS:
        if b not in xm:
            err(w, f"xpOutcomeMult missing {b}")
    lu = d.get("levelUp", {})
    for k in ("primary", "secondary", "other", "luckEveryNLevels"):
        if k not in lu:
            err(w, f"levelUp missing {k}")
    lo = d.get("loyalty", {})
    for k in ("successFair", "greedy", "injury", "min", "max"):
        if k not in lo:
            err(w, f"loyalty missing {k}")
    tu = d.get("tierUnlockHeists")
    if isinstance(tu, list):
        if len(tu) != 4 or tu[0] != 0 or tu != sorted(tu):
            err(w, "tierUnlockHeists must be 4 ascending values starting at 0")
    fc = d.get("finalCut", {})
    if "requiredHeists" not in fc:
        err(w, "finalCut.requiredHeists missing")


def check_strings():
    d = load("strings.json")
    if d is None:
        return
    w = "strings"
    req(d, ["stamps", "outcomeLines", "luckyBreak", "gameOverReasons", "winReason", "titleTagline", "tutorial",
            "credits", "nightEvents"], w)
    for b in BANDS:
        if b not in d.get("stamps", {}):
            err(w, f"stamps missing {b}")
        lines = d.get("outcomeLines", {}).get(b)
        if not isinstance(lines, list) or len(lines) < 5:
            err(w, f"outcomeLines.{b} needs >= 5 lines")
        else:
            for ln in lines:
                if "{name}" not in ln:
                    err(w, f"outcomeLines.{b} line without {{name}}: {ln!r}")
    if not d.get("luckyBreak"):
        err(w, "luckyBreak empty")
    for k in ("Broke", "NoCrew"):
        if k not in d.get("gameOverReasons", {}):
            err(w, f"gameOverReasons missing {k}")
    tut = d.get("tutorial", [])
    if not (isinstance(tut, list) and 1 <= len(tut) <= 5):
        err(w, "tutorial should be 1..5 short steps")
    for k in ("starringHeader", "memoriamHeader", "directedBy"):
        if k not in d.get("credits", {}):
            err(w, f"credits missing {k}")
    for k in ("tierUnlocked", "recruitsRefreshed"):
        if k not in d.get("nightEvents", {}):
            err(w, f"nightEvents missing {k}")


def check_cross():
    """Cross-file sanity: every job's required roles exist; the Premiere is reachable; room unlock kinds exist."""
    jobs = (load("jobs.json") or {}).get("jobs", [])
    rooms = (load("rooms.json") or {}).get("rooms", [])
    kinds_by_room = {r.get("type"): {m.get("kind") for L in r.get("levels", []) for m in L.get("modifiers", [])} for r in rooms}
    if any(j.get("heavy") for j in jobs) and "UnlockHeavyJobs" not in kinds_by_room.get("Garage", set()):
        err("cross", "heavy jobs exist but the Garage has no UnlockHeavyJobs")
    if "UnlockFinalCut" not in kinds_by_room.get("ScreeningRoom", set()):
        err("cross", "Screening Room has no UnlockFinalCut")
    office = next((r for r in rooms if r.get("type") == "Office"), None)
    if office:
        sizes = [next((m.get("value") for m in L.get("modifiers", []) if m.get("kind") == "JobBoardSize"), None)
                 for L in office.get("levels", [])]
        if sizes != [3, 4, 5]:
            err("cross", f"Office JobBoardSize per level should be [3, 4, 5], got {sizes}")


def main():
    for fn in (check_types, check_names, check_jobs, check_gear, check_rooms, check_perks, check_balance,
               check_strings, check_cross):
        fn()
    for w in warnings:
        print("WARN ", w)
    for e in errors:
        print("ERROR", e)
    if errors:
        print(f"\n{len(errors)} error(s).")
        sys.exit(1)
    print(f"All data files valid ({len(warnings)} warning(s)).")


if __name__ == "__main__":
    main()
