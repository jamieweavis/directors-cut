#!/usr/bin/env python3
"""Validate Content/Data/*.json against schema.md (v2: realtime crew & missions).

Run:  python3 Content/Data/validate.py      (exit code 1 on any error)
Python 3 stdlib only.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
ICON_FILE = os.path.join(ROOT, "Content", "Fonts", "lucide_codepoints.json")

FILES = ["balance.json", "criminal_types.json", "names.json", "abilities.json", "traits.json",
         "missions.json", "gear.json", "rooms.json", "perks.json"]

TYPES = ["Wheelman", "Muscle", "Hacker", "Ghost", "Face", "Safecracker"]
THREATS = ["Guards", "Cameras", "Alarms", "Locks", "Vault", "Patrols", "Witnesses", "Pursuit"]
QUALITIES = ["Street", "Pro", "Elite"]
SLOTS = ["Tool", "Outfit", "Gadget"]
ROOMS = ["Office", "Lounge", "Workshop", "Garage", "OpsRoom", "Clinic", "ServerRoom", "LawyerOffice"]

NAME_MAX, DESC_MAX, FLAVOUR_MAX, LEVEL_EFFECT_MAX = 22, 40, 60, 28

# kind -> set of scopes where it may appear
EFFECT_SCOPES = {
    "chance": {"trait", "perk"},
    "chanceNight": {"trait", "perk"},
    "chanceDay": {"trait", "perk"},
    "chanceShort": {"trait"},
    "chanceLong": {"trait"},
    "chanceSolo": {"trait"},
    "chanceThreat": {"trait", "perk"},
    "chanceWithType": {"trait"},
    "chanceTierMin": {"trait", "perk", "room"},
    "chanceIfType": {"room", "perk"},
    "durationMult": {"trait", "perk", "room"},
    "payoutMult": {"trait", "perk", "room"},
    "costMult": {"trait", "perk"},
    "xpMult": {"trait", "perk"},
    "injuryChanceMult": {"trait", "perk", "room"},
    "injuryTimeMult": {"perk", "room"},
    "arrestMult": {"trait", "perk", "room"},
    "fairCutAdd": {"trait", "perk"},
    "willMin": {"perk"},
    "loyaltyIdlePerDay": {"room"},
    "rosterCap": {"room", "perk"},
    "maxActive": {"room", "perk"},
    "boardSize": {"room", "perk"},
    "postIntervalMult": {"room", "perk"},
    "gearSlots": {"room"},
    "gearTier": {"room"},
    "gearPriceMult": {"room", "perk"},
    "upkeepMult": {"perk"},
    "recruitLevelAdd": {"perk"},
    "recruitQualityAdd": {"room", "perk"},
    "preventFirstArrest": {"perk"},
    "patchUp": {"room"},
    "bail": {"room"},
    "unlockTier": {"room"},
    "unlockFinal": {"room"},
    "lootChanceAdd": {"perk", "room"},
}
NEEDS_THRESHOLD = {"chanceShort", "chanceLong", "chanceTierMin", "bail"}
NEEDS_THREAT_PARAM = {"chanceThreat"}
NEEDS_TYPE_PARAM = {"chanceWithType", "chanceIfType"}
OPTIONAL_TYPE_PARAM = {"payoutMult"}
MULT_KINDS = {k for k in EFFECT_SCOPES if k.endswith("Mult")}

errors = []
warnings = []


def err(msg):
    errors.append(msg)


def warn(msg):
    warnings.append(msg)


def load(name):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        err(f"{name}: missing file")
        return None
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        err(f"{name}: invalid JSON: {e}")
        return None


def is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def req(obj, field, ctx, kind=None):
    if not isinstance(obj, dict) or field not in obj:
        err(f"{ctx}: missing required field '{field}'")
        return None
    v = obj[field]
    if kind == "num" and not is_num(v):
        err(f"{ctx}.{field}: expected number, got {v!r}")
    elif kind == "str" and not (isinstance(v, str) and v):
        err(f"{ctx}.{field}: expected non-empty string, got {v!r}")
    elif kind == "list" and not isinstance(v, list):
        err(f"{ctx}.{field}: expected list, got {type(v).__name__}")
    elif kind == "bool" and not isinstance(v, bool):
        err(f"{ctx}.{field}: expected bool, got {v!r}")
    elif kind == "dict" and not isinstance(v, dict):
        err(f"{ctx}.{field}: expected object, got {type(v).__name__}")
    return v


def check_len(s, limit, ctx):
    if isinstance(s, str) and len(s) > limit:
        err(f"{ctx}: '{s}' is {len(s)} chars (limit {limit})")


def check_range(v, ctx, lo=None, hi=None, integer=False):
    if not (isinstance(v, list) and len(v) == 2 and all(is_num(x) for x in v)):
        err(f"{ctx}: expected [min, max] numbers, got {v!r}")
        return False
    if v[0] > v[1]:
        err(f"{ctx}: min > max in {v}")
    if lo is not None and v[0] < lo:
        err(f"{ctx}: {v[0]} < {lo}")
    if hi is not None and v[1] > hi:
        err(f"{ctx}: {v[1]} > {hi}")
    if integer and not all(float(x).is_integer() for x in v):
        err(f"{ctx}: expected integers, got {v}")
    return True


def check_id(i, ctx, seen):
    if not isinstance(i, str) or not i or i != i.lower() or not all(c.isalnum() or c == "_" for c in i):
        err(f"{ctx}: id '{i}' is not snake_case")
    if i in seen:
        err(f"{ctx}: duplicate id '{i}'")
    seen.add(i)


def check_icon(icon, ctx, icons):
    if not isinstance(icon, str) or not icon:
        err(f"{ctx}: missing icon")
    elif icons is not None and icon not in icons:
        err(f"{ctx}: icon '{icon}' not in lucide_codepoints.json")


def check_effects(effects, scope, ctx):
    if not isinstance(effects, list):
        err(f"{ctx}: effects must be a list")
        return
    for i, e in enumerate(effects):
        c = f"{ctx}.effects[{i}]"
        if not isinstance(e, dict):
            err(f"{c}: not an object")
            continue
        kind = e.get("kind")
        if kind not in EFFECT_SCOPES:
            err(f"{c}: unknown effect kind '{kind}'")
            continue
        if scope not in EFFECT_SCOPES[kind]:
            err(f"{c}: kind '{kind}' not allowed in {scope} (allowed: {sorted(EFFECT_SCOPES[kind])})")
        v = e.get("value")
        if not is_num(v):
            err(f"{c}: value must be a number")
            continue
        if kind in NEEDS_THRESHOLD and not is_num(e.get("threshold")):
            err(f"{c}: kind '{kind}' needs a numeric threshold")
        if kind in NEEDS_THREAT_PARAM and e.get("param") not in THREATS:
            err(f"{c}: kind '{kind}' needs param = threat, got {e.get('param')!r}")
        if kind in NEEDS_TYPE_PARAM and e.get("param") not in TYPES:
            err(f"{c}: kind '{kind}' needs param = type, got {e.get('param')!r}")
        if kind in OPTIONAL_TYPE_PARAM and e.get("param") not in (None, "") and e.get("param") not in TYPES:
            err(f"{c}: payoutMult param must be a type, got {e.get('param')!r}")
        if kind in MULT_KINDS and not (0.1 <= v <= 3.0):
            err(f"{c}: multiplier {v} outside sane range [0.1, 3]")
        if kind.startswith("chance") and not (-0.5 <= v <= 0.5):
            err(f"{c}: chance {v} outside sane range [-0.5, 0.5]")


def main():
    data = {n: load(n) for n in FILES}
    icons = None
    if os.path.exists(ICON_FILE):
        with open(ICON_FILE, encoding="utf-8") as f:
            icons = set(json.load(f).keys())
    else:
        err(f"icon list not found: {ICON_FILE}")

    for stale in ("jobs.json", "strings.json"):
        if os.path.exists(os.path.join(HERE, stale)):
            err(f"{stale}: v1 file no longer in schema, delete it")

    # ---- abilities ----
    ability_ids, ability_counter = set(), {}
    ab = data["abilities.json"]
    if ab is not None:
        for i, a in enumerate(req(ab, "abilities", "abilities.json", "list") or []):
            c = f"abilities[{i}]"
            aid = req(a, "id", c, "str")
            check_id(aid, c, ability_ids)
            check_len(req(a, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_icon(req(a, "icon", c, "str"), c, icons)
            t = req(a, "counters", c, "str")
            if t not in THREATS:
                err(f"{c}: counters '{t}' not a threat")
            ability_counter[aid] = t
        if len(ability_ids) != 12:
            err(f"abilities.json: expected 12 abilities (2 per type), found {len(ability_ids)}")

    # ---- traits ----
    trait_ids, trait_negative = set(), {}
    tr = data["traits.json"]
    if tr is not None:
        for i, t in enumerate(req(tr, "traits", "traits.json", "list") or []):
            c = f"traits[{i}]"
            tid = req(t, "id", c, "str")
            check_id(tid, c, trait_ids)
            check_len(req(t, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_len(req(t, "description", c, "str"), DESC_MAX, f"{c}.description")
            check_icon(req(t, "icon", c, "str"), c, icons)
            neg = req(t, "negative", c, "bool")
            trait_negative[tid] = bool(neg)
            check_effects(req(t, "effects", c, "list"), "trait", c)
        n_neg = sum(trait_negative.values())
        if not (14 <= len(trait_ids) <= 26):
            warn(f"traits.json: {len(trait_ids)} traits (aim ~18)")
        if n_neg < 3:
            err(f"traits.json: only {n_neg} negative traits (need >= 3)")

    # ---- criminal types ----
    type_abilities = {}
    ct = data["criminal_types.json"]
    if ct is not None:
        seen_types = set()
        for i, t in enumerate(req(ct, "types", "criminal_types.json", "list") or []):
            c = f"types[{i}]"
            ty = req(t, "type", c, "str")
            if ty not in TYPES:
                err(f"{c}: unknown type '{ty}'")
            if ty in seen_types:
                err(f"{c}: duplicate type '{ty}'")
            seen_types.add(ty)
            abl = req(t, "abilities", c, "list") or []
            if len(abl) != 2:
                err(f"{c} ({ty}): needs exactly 2 abilities, has {len(abl)}")
            for a in abl:
                if a not in ability_ids:
                    err(f"{c} ({ty}): ability '{a}' not in abilities.json")
            type_abilities[ty] = abl
            pool = req(t, "traitPool", c, "list") or []
            for tid in pool:
                if tid not in trait_ids:
                    err(f"{c} ({ty}): trait '{tid}' not in traits.json")
            pos = [x for x in pool if x in trait_negative and not trait_negative[x]]
            negs = [x for x in pool if trait_negative.get(x)]
            if len(pos) < 3:
                err(f"{c} ({ty}): traitPool needs >= 3 positive traits (Elite rolls 3)")
            if not negs:
                warn(f"{c} ({ty}): traitPool has no negative traits")
        missing = set(TYPES) - seen_types
        if missing:
            err(f"criminal_types.json: missing types {sorted(missing)}")
        used = [a for l in type_abilities.values() for a in l]
        if len(set(used)) != len(used):
            warn("criminal_types.json: an ability is shared between types")

    # ---- gear ----
    gear_ids, gear_counter = set(), {}
    gr = data["gear.json"]
    if gr is not None:
        n_shop = n_loot = 0
        for i, g in enumerate(req(gr, "gear", "gear.json", "list") or []):
            c = f"gear[{i}]"
            gid = req(g, "id", c, "str")
            check_id(gid, c, gear_ids)
            check_len(req(g, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_len(req(g, "description", c, "str"), DESC_MAX, f"{c}.description")
            check_icon(req(g, "icon", c, "str"), c, icons)
            slot = req(g, "slot", c, "str")
            if slot not in SLOTS:
                err(f"{c}: slot '{slot}' invalid")
            price = req(g, "price", c, "num")
            if is_num(price) and price <= 0:
                err(f"{c}: price must be > 0")
            power = req(g, "power", c, "num")
            if is_num(power) and not (0 <= power <= 2.0):
                err(f"{c}: power {power} outside 0..2")
            cnt = g.get("counters", None) if isinstance(g, dict) else None
            if "counters" not in g:
                err(f"{c}: missing required field 'counters' (use null)")
            if cnt is not None and cnt not in THREATS:
                err(f"{c}: counters '{cnt}' not a threat")
            trait = g.get("trait", None)
            if "trait" not in g:
                err(f"{c}: missing required field 'trait' (use null)")
            if trait is not None and trait not in trait_ids:
                err(f"{c}: trait '{trait}' not in traits.json")
            tier = req(g, "tier", c, "num")
            if tier not in (1, 2, 3):
                err(f"{c}: tier must be 1..3")
            loot = req(g, "loot", c, "bool")
            if slot == "Tool" and cnt is None:
                err(f"{c}: Tool must counter a threat")
            if slot == "Outfit" and trait is None:
                err(f"{c}: Outfit must grant a trait")
            if slot == "Gadget" and is_num(power) and not (0.5 <= power <= 2.0):
                err(f"{c}: Gadget power must be 0.5..2.0")
            if cnt is not None:
                gear_counter.setdefault(cnt, []).append(gid)
            if loot:
                n_loot += 1
            else:
                n_shop += 1
        if n_shop < 12 or n_loot < 3:
            warn(f"gear.json: {n_shop} shop + {n_loot} loot items (aim ~15 + ~5)")
        for tier in (1, 2, 3):
            if not any(g.get("tier") == tier and not g.get("loot") for g in gr.get("gear", [])):
                err(f"gear.json: no shop item at tier {tier}")

    # ---- threat coverage ----
    if type_abilities:
        for th in THREATS:
            types_cov = sorted(ty for ty, abl in type_abilities.items()
                               if any(ability_counter.get(a) == th for a in abl))
            n_gear = len(gear_counter.get(th, []))
            if not types_cov:
                err(f"coverage: no crew type counters {th}")
            if len(types_cov) + n_gear < 2:
                err(f"coverage: {th} countered only {len(types_cov)} type(s) + {n_gear} gear (need >= 2)")
            if len(types_cov) < 2 and n_gear < 1:
                err(f"coverage: {th} has a single type counter and no gear backup")

    # ---- names ----
    nm = data["names.json"]
    if nm is not None:
        for key in ("first", "last"):
            lst = req(nm, key, "names.json", "list") or []
            if len(lst) < 30:
                err(f"names.json.{key}: {len(lst)} entries (need >= 30)")
            if len(set(lst)) != len(lst):
                err(f"names.json.{key}: duplicates")
            for s in lst:
                check_len(s, 12, f"names.json.{key}")
        nick = req(nm, "nicknames", "names.json", "dict") or {}
        total = 0
        for ty in TYPES:
            lst = nick.get(ty, [])
            if len(lst) < 6:
                err(f"names.json.nicknames.{ty}: {len(lst)} entries (need >= 6)")
            total += len(lst)
            for s in lst:
                check_len(s, 12, f"names.json.nicknames.{ty}")
        if total < 30:
            err(f"names.json.nicknames: {total} total (need >= 30)")

    # ---- missions ----
    mission_ids, final_ids, tiers_present = set(), [], {}
    ms = data["missions.json"]
    if ms is not None:
        for i, m in enumerate(req(ms, "missions", "missions.json", "list") or []):
            c = f"missions[{i}]"
            mid = req(m, "id", c, "str")
            check_id(mid, c, mission_ids)
            c = f"missions[{mid}]"
            check_len(req(m, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_icon(req(m, "icon", c, "str"), c, icons)
            fl = req(m, "flavour", c, "list") or []
            if len(fl) != 3:
                err(f"{c}: flavour needs 3 variants, has {len(fl)}")
            for s in fl:
                check_len(s, FLAVOUR_MAX, f"{c}.flavour")
            tier = req(m, "tier", c, "num")
            if tier not in (1, 2, 3, 4):
                err(f"{c}: tier must be 1..4")
                continue
            final = req(m, "final", c, "bool")
            cs = req(m, "crewSize", c, "list")
            check_range(cs, f"{c}.crewSize", 1, 5, True)
            du = req(m, "durationSec", c, "list")
            check_range(du, f"{c}.durationSec", 30, 1800, True)
            th = req(m, "threats", c, "dict") or {}
            fixed = req(th, "fixed", f"{c}.threats", "list") or []
            pool = req(th, "pool", f"{c}.threats", "list") or []
            extra = req(th, "extra", f"{c}.threats", "list")
            check_range(extra, f"{c}.threats.extra", 0, 4, True)
            for t in fixed + pool:
                if t not in THREATS:
                    err(f"{c}: threat '{t}' invalid")
            if set(fixed) & set(pool):
                warn(f"{c}: pool repeats a fixed threat")
            if isinstance(extra, list) and len(extra) == 2 and extra[1] > len(pool):
                err(f"{c}: extra max {extra[1]} > pool size {len(pool)}")
            nthreat = [len(fixed) + (extra[0] if extra else 0), len(fixed) + (extra[1] if extra else 0)]
            cost = req(m, "cost", c, "list")
            check_range(cost, f"{c}.cost", 0, None, True)
            pay = req(m, "payout", c, "list")
            check_range(pay, f"{c}.payout", 1, None, True)
            req(m, "xpReward", c, "num")
            ar = req(m, "arrestRisk", c, "num")
            nc = req(m, "nightChance", c, "num")
            w = req(m, "weight", c, "num")
            if is_num(ar) and not (0 <= ar <= 0.6):
                err(f"{c}: arrestRisk {ar} outside 0..0.6")
            if is_num(nc) and not (0 <= nc <= 1):
                err(f"{c}: nightChance {nc} outside 0..1")
            if is_num(w) and w <= 0:
                err(f"{c}: weight must be > 0")
            if isinstance(cost, list) and isinstance(pay, list) and len(cost) == 2 and len(pay) == 2 \
                    and cost[1] >= pay[0] * 0.3:
                err(f"{c}: max cost {cost[1]} >= Director's 30% of min payout ({pay[0] * 0.3:.0f})")
            # design targets
            if final:
                final_ids.append(mid)
                if tier != 4 or cs != [5, 5]:
                    err(f"{c}: final must be tier 4 with crewSize [5,5]")
                if not (6 <= nthreat[0] and nthreat[1] <= 7):
                    err(f"{c}: final needs 6-7 threats, has {nthreat}")
                continue
            tiers_present[tier] = tiers_present.get(tier, 0) + 1
            targets = {1: ((1, 1), (60, 120), (1, 2)), 2: ((2, 2), (180, 300), (2, 3)),
                       3: ((3, 4), (480, 720), (3, 4)), 4: ((4, 5), (900, 1200), (4, 5))}
            (cmin, cmax), (dmin, dmax), (tmin, tmax) = targets[tier]
            if isinstance(cs, list) and len(cs) == 2 and (cs[0] < cmin or cs[1] > cmax):
                err(f"{c}: crewSize {cs} outside tier {tier} target {cmin}-{cmax}")
            if isinstance(du, list) and len(du) == 2 and (du[0] < dmin or du[1] > dmax):
                err(f"{c}: durationSec {du} outside tier {tier} target {dmin}-{dmax}")
            if nthreat[0] < tmin or nthreat[1] > tmax:
                warn(f"{c}: threat count {nthreat} outside tier {tier} guide {tmin}-{tmax}")
            if tier == 1 and is_num(ar) and ar > 0.1:
                err(f"{c}: tier 1 arrestRisk {ar} too high (<= 0.1)")
            if tier >= 3 and is_num(ar) and not (0.15 <= ar <= 0.45):
                err(f"{c}: tier {tier} arrestRisk {ar} outside 0.15..0.45")
        for t, (lo, hi) in {1: (5, 10), 2: (4, 9), 3: (4, 8), 4: (2, 5)}.items():
            n = tiers_present.get(t, 0)
            if n == 0:
                err(f"missions.json: no tier {t} missions")
            elif not (lo <= n <= hi):
                warn(f"missions.json: {n} tier {t} missions (aim {lo}-{hi})")
        if len(final_ids) != 1:
            err(f"missions.json: need exactly one final mission, found {final_ids}")
        # payout should grow with tier
        avg = {}
        for m in ms.get("missions", []):
            if not m.get("final") and isinstance(m.get("payout"), list):
                avg.setdefault(m["tier"], []).append(sum(m["payout"]) / 2)
        means = [sum(avg[t]) / len(avg[t]) for t in sorted(avg)]
        if means != sorted(means):
            err(f"missions.json: mean payout does not rise with tier: {[round(x) for x in means]}")

    # ---- rooms ----
    room_effect_kinds = set()
    rm = data["rooms.json"]
    if rm is not None:
        seen = set()
        for i, r in enumerate(req(rm, "rooms", "rooms.json", "list") or []):
            c = f"rooms[{i}]"
            ty = req(r, "type", c, "str")
            if ty not in ROOMS:
                err(f"{c}: room type '{ty}' invalid")
            if ty in seen:
                err(f"{c}: duplicate room '{ty}'")
            seen.add(ty)
            c = f"rooms[{ty}]"
            check_len(req(r, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_len(req(r, "description", c, "str"), DESC_MAX, f"{c}.description")
            check_icon(req(r, "icon", c, "str"), c, icons)
            for f3 in ("cost", "buildSec", "requiredSuccesses", "upkeepPerDay", "levelEffects", "levels"):
                v = req(r, f3, c, "list")
                if isinstance(v, list) and len(v) != 3:
                    err(f"{c}.{f3}: needs 3 entries (levels 1..3), has {len(v)}")
            for f3 in ("cost", "buildSec", "requiredSuccesses", "upkeepPerDay"):
                v = r.get(f3) or []
                if not all(is_num(x) and x >= 0 for x in v):
                    err(f"{c}.{f3}: must be non-negative numbers")
                elif v != sorted(v):
                    err(f"{c}.{f3}: should not decrease by level: {v}")
            bs = r.get("buildSec") or []
            if any(x > 360 for x in bs):
                err(f"{c}.buildSec: {bs} exceeds ~5 min")
            for s in r.get("levelEffects") or []:
                check_len(s, LEVEL_EFFECT_MAX, f"{c}.levelEffects")
            for li, lv in enumerate(r.get("levels") or []):
                effs = req(lv, "effects", f"{c}.levels[{li}]", "list")
                check_effects(effs, "room", f"{c}.levels[{li}]")
                for e in effs or []:
                    room_effect_kinds.add(e.get("kind"))
            if ty == "Office":
                if (r.get("cost") or [None])[0] != 0:
                    err(f"{c}: Office L1 cost must be 0 (starts built)")
                kinds = [e.get("kind") for lv in r.get("levels", [])[1:] for e in lv.get("effects", [])]
                if "boardSize" not in kinds:
                    err(f"{c}: Office levels must raise boardSize")
            elif bs and bs[0] < 30:
                err(f"{c}: L1 buildSec {bs[0]} too short")
        missing = set(ROOMS) - seen
        if missing:
            err(f"rooms.json: missing rooms {sorted(missing)}")
        ops = next((r for r in rm.get("rooms", []) if r.get("type") == "OpsRoom"), None)
        if ops:
            l1 = [e.get("kind") for e in ops["levels"][0]["effects"]]
            if "unlockTier" not in l1:
                err("rooms[OpsRoom]: L1 must have unlockTier (tier 3+)")
        for k in ("unlockTier", "unlockFinal", "gearTier", "rosterCap", "maxActive"):
            if k not in room_effect_kinds:
                err(f"rooms.json: no room provides '{k}'")
        wk = next((r for r in rm.get("rooms", []) if r.get("type") == "Workshop"), None)
        if wk:
            gt = [max([e["value"] for e in lv["effects"] if e.get("kind") == "gearTier"] or [1]) for lv in wk["levels"]]
            if gt[0] < 2 or gt[2] < 3:
                err(f"rooms[Workshop]: gearTier per level {gt}; need 2 at L1 and 3 at L3")

    # ---- perks ----
    perk_ids = set()
    pk = data["perks.json"]
    if pk is not None:
        for i, p in enumerate(req(pk, "perks", "perks.json", "list") or []):
            c = f"perks[{i}]"
            pid = req(p, "id", c, "str")
            check_id(pid, c, perk_ids)
            check_len(req(p, "displayName", c, "str"), NAME_MAX, f"{c}.displayName")
            check_len(req(p, "description", c, "str"), DESC_MAX, f"{c}.description")
            check_icon(req(p, "icon", c, "str"), c, icons)
            check_effects(req(p, "effects", c, "list"), "perk", c)
        if len(perk_ids) < 12:
            err(f"perks.json: {len(perk_ids)} perks (need ~15)")

    # ---- balance ----
    b = data["balance.json"]
    if b is not None:
        c = "balance"
        req(b, "startingCash", c, "num")
        sr = req(b, "startingRoster", c, "num")
        if is_num(sr) and not (0 <= sr <= 6):
            err(f"{c}.startingRoster must be 0..6 (distinct types)")
        check_range(req(b, "startingRosterLevels", c, "list"), f"{c}.startingRosterLevels", 1, 10, True)
        clk = req(b, "clock", c, "dict") or {}
        for k in ("startHour", "dawnHour", "duskHour", "gameMinutesPerSecond"):
            v = req(clk, k, f"{c}.clock", "num")
            if k.endswith("Hour") and is_num(v) and not (0 <= v < 24):
                err(f"{c}.clock.{k} must be 0..24")
        req(b, "autosaveSeconds", c, "num")
        bd = req(b, "board", c, "dict") or {}
        for k in ("baseSize", "postIntervalSec", "rareChance"):
            req(bd, k, f"{c}.board", "num")
        check_range(req(bd, "expireSecRange", f"{c}.board", "list"), f"{c}.board.expireSecRange", 30)
        tw = req(bd, "tierWeights", f"{c}.board", "list") or []
        if len(tw) != 4 or any(not isinstance(r, list) or len(r) != 4 for r in tw):
            err(f"{c}.board.tierWeights must be 4x4")
        else:
            for ti, row in enumerate(tw):
                if abs(sum(row) - 1.0) > 1e-6:
                    err(f"{c}.board.tierWeights[{ti}] sums to {sum(row)}")
                if any(row[j] > 0 for j in range(ti + 1, 4)):
                    err(f"{c}.board.tierWeights[{ti}] weights a tier that is not unlocked yet")
        for sid in req(bd, "startingBoard", f"{c}.board", "list") or []:
            if sid not in mission_ids:
                err(f"{c}.board.startingBoard: '{sid}' not in missions.json")
            elif ms and next(m for m in ms["missions"] if m["id"] == sid)["tier"] != 1:
                err(f"{c}.board.startingBoard: '{sid}' is not tier 1")
        tiers = req(b, "tiers", c, "dict") or {}
        us = req(tiers, "unlockSuccesses", f"{c}.tiers", "list") or []
        if len(us) != 4 or us != sorted(us) or (us and us[0] != 0):
            err(f"{c}.tiers.unlockSuccesses must be 4 ascending values starting at 0: {us}")
        lr = req(tiers, "levelRange", f"{c}.tiers", "list") or []
        if len(lr) != 4:
            err(f"{c}.tiers.levelRange needs 4 ranges")
        for ti, r in enumerate(lr):
            check_range(r, f"{c}.tiers.levelRange[{ti}]", 1, 10, True)
        sc = req(b, "success", c, "dict") or {}
        req(sc, "threatPoints", f"{c}.success", "num")
        req(sc, "maxChance", f"{c}.success", "num")
        lf = req(sc, "levelFactor", f"{c}.success", "dict") or {}
        for k in ("atOrAbove", "perLevelAbove", "maxFactor"):
            req(lf, k, f"{c}.success.levelFactor", "num")
        bl = req(lf, "below", f"{c}.success.levelFactor", "list") or []
        if len(bl) != 3 or bl != sorted(bl, reverse=True):
            err(f"{c}.success.levelFactor.below must be 3 descending values")
        cut = req(b, "cut", c, "dict") or {}
        for k in ("min", "max", "step", "default", "fairCutBase", "fairCutLoyaltyDiv", "willSlope",
                  "willMin", "refusalThreshold", "willFactorBase", "willFactorScale"):
            req(cut, k, f"{c}.cut", "num")
        if all(is_num(cut.get(k)) for k in ("min", "max", "default")) and not (cut["min"] <= cut["default"] <= cut["max"]):
            err(f"{c}.cut.default outside [min, max]")
        if all(is_num(cut.get(k)) for k in ("willFactorBase", "willFactorScale")) \
                and abs(cut["willFactorBase"] + cut["willFactorScale"] - 1.0) > 1e-6:
            warn(f"{c}.cut: willFactorBase + willFactorScale != 1 (fair cut would not give x1.0)")
        oc = req(b, "outcome", c, "dict") or {}
        for k in ("bustedBand", "bonusPayoutMult", "failXpMult"):
            req(oc, k, f"{c}.outcome", "num")
        inj = req(b, "injury", c, "dict") or {}
        for k in ("failChance", "bustedChance"):
            v = req(inj, k, f"{c}.injury", "num")
            if is_num(v) and not (0 <= v <= 1):
                err(f"{c}.injury.{k} must be 0..1")
        check_range(req(inj, "secondsRange", f"{c}.injury", "list"), f"{c}.injury.secondsRange", 1)
        lv = req(b, "levels", c, "dict") or {}
        for k in ("max", "xpBase", "xpExp"):
            req(lv, k, f"{c}.levels", "num")
        loy = req(b, "loyalty", c, "dict") or {}
        for k in ("start", "successFair", "greedy", "injury", "min", "max", "quitBelow"):
            req(loy, k, f"{c}.loyalty", "num")
        up = req(b, "upkeep", c, "dict") or {}
        req(up, "perCriminalPerDay", f"{c}.upkeep", "num")
        rc = req(b, "recruits", c, "dict") or {}
        for k in ("boardSize", "refreshSec", "refreshCost", "costBase", "costPerLevel"):
            req(rc, k, f"{c}.recruits", "num")
        qcm = req(rc, "qualityCostMult", f"{c}.recruits", "list") or []
        if len(qcm) != 3:
            err(f"{c}.recruits.qualityCostMult needs 3 values")
        qo = req(rc, "qualityOdds", f"{c}.recruits", "list") or []
        if len(qo) != 4:
            err(f"{c}.recruits.qualityOdds needs 4 rows (by unlocked tier)")
        for ri, row in enumerate(qo):
            if len(row) != 3 or abs(sum(row) - 1) > 1e-6:
                err(f"{c}.recruits.qualityOdds[{ri}] must be 3 values summing to 1")
        q = req(b, "quality", c, "dict") or {}
        for k in ("abilities", "traits", "negativeTraitChance"):
            v = req(q, k, f"{c}.quality", "list") or []
            if len(v) != 3:
                err(f"{c}.quality.{k} needs 3 values (Street, Pro, Elite)")
        if (q.get("abilities") or [0, 0, 0])[2] > 2:
            err(f"{c}.quality.abilities: types only have 2 abilities")
        ge = req(b, "gear", c, "dict") or {}
        for k in ("baseSlots", "sellRefund", "lootChance", "rareLootChance"):
            req(ge, k, f"{c}.gear", "num")
        nt = req(b, "night", c, "dict") or {}
        req(nt, "missionBonus", f"{c}.night", "num")
        md = req(b, "minDurationMult", c, "num")
        if is_num(md) and not (0 < md <= 1):
            err(f"{c}.minDurationMult must be in (0, 1]")
        pe = req(b, "perks", c, "dict") or {}
        req(pe, "everyNSuccesses", f"{c}.perks", "num")
        req(pe, "offerSize", f"{c}.perks", "num")
        fi = req(b, "final", c, "dict") or {}
        tid = req(fi, "templateId", f"{c}.final", "str")
        if tid not in final_ids:
            err(f"{c}.final.templateId '{tid}' is not a final mission in missions.json")
        rs = req(fi, "requiredSuccesses", f"{c}.final", "num")
        if is_num(rs) and len(us) == 4 and rs < us[3]:
            warn(f"{c}.final.requiredSuccesses {rs} < tier 4 unlock {us[3]}")
        go = req(b, "gameOver", c, "dict") or {}
        req(go, "cashFloor", f"{c}.gameOver", "num")

    for w in warnings:
        print(f"WARN  {w}")
    for e in errors:
        print(f"ERROR {e}")
    n = {k: len(v) for k, v in {
        "abilities": ability_ids, "traits": trait_ids, "gear": gear_ids,
        "missions": mission_ids, "perks": perk_ids}.items()}
    print(f"\nChecked {len(FILES)} files: {n}; tiers {dict(sorted(tiers_present.items()))}")
    print(f"{len(errors)} error(s), {len(warnings)} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
