#!/usr/bin/env python3
"""Monte-Carlo balance check for Director's Cut (PLAN 2.7 formula, driven by balance.json + jobs/types).

Usage: python3 Content/Data/balance_check.py [trials]

Reports:
  1. Success % per tier-1 job with a 'sensible' fresh crew (required roles + first bonus role) at the default cut.
  2. Cut sweep (10..90%): success %, refusal rate and expected Director take per job.
  3. Early economy: a greedy simple-policy run from a fresh start (nights until the first room is affordable,
     cash curve, bankruptcy rate over 15 nights).
  4. Later-tier spot checks with a levelled, geared crew at the unlock heist count.
Assumptions the C++ Sim may differ on are marked ASSUME.
"""
import json
import math
import os
import random
import sys

DATA = os.path.dirname(os.path.abspath(__file__))


def load(n):
    with open(os.path.join(DATA, n), encoding="utf-8") as f:
        return json.load(f)


B = load("balance.json")
TYPES = {t["type"]: t for t in load("criminal_types.json")["types"]}
JOBS = {j["id"]: j for j in load("jobs.json")["jobs"]}
ROOMS = {r["type"]: r for r in load("rooms.json")["rooms"]}
STATS = ["Driving", "Muscle", "Tech", "Stealth", "Charm", "Finesse"]


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


class Crim:
    def __init__(self, rng, ctype, level=1, statbonus=0):
        t = TYPES[ctype]
        self.type = ctype
        self.base = {}
        for s in STATS:
            rg = t["primaryRange"] if s == t["primary"] else t["secondaryRange"] if s == t["secondary"] else t["otherRange"]
            self.base[s] = rng.randint(rg[0], rg[1]) + statbonus
        self.luck = rng.randint(*t["luckRange"])
        self.loyalty = rng.randint(*t["loyaltyRange"])
        self.level, self.xp, self.injured = 1, 0, 0
        self.gear = {}
        for _ in range(level - 1):
            self.level_up()

    def level_up(self):
        t, lu = TYPES[self.type], B["levelUp"]
        self.level += 1
        for s in STATS:
            self.base[s] += lu["primary"] if s == t["primary"] else lu["secondary"] if s == t["secondary"] else lu["other"]
        if self.level % lu["luckEveryNLevels"] == 0:
            self.luck = min(10, self.luck + 1)

    def add_xp(self, xp):
        # ASSUME: XP resets each level; next level needs levelCurveBase * level^levelCurveExp.
        self.xp += xp
        while self.level < B["maxLevel"] and self.xp >= B["levelCurveBase"] * self.level ** B["levelCurveExp"]:
            self.xp -= B["levelCurveBase"] * self.level ** B["levelCurveExp"]
            self.level_up()

    def eff(self, s):
        return self.base[s] + self.gear.get(s, 0)


def difficulty(job, heists, board_tier):
    return job["baseDifficulty"] * (1 + B["difficultyPerHeist"] * heists) * (1 + B["difficultyPerTier"] * board_tier)


def fair_cut(c):
    return B["fairCutBase"] + (c.loyalty - 50) / B["fairCutLoyaltyDiv"]


def willingness(c, cut):
    return clamp(1 - B["willSlope"] * max(0.0, cut - fair_cut(c)), B["willMin"], 1.0)


def chance(job, crew, cut, heists, board_tier=None, extra_bonus=0.0):
    """Returns (success, margin, refusers)."""
    board_tier = board_tier or job["tier"]
    score = 0.0
    for s, w in job["statWeights"].items():
        vals = sorted((c.eff(s) for c in crew), reverse=True)
        score += w * (vals[0] + B["supportWeight"] * sum(vals[1:]))
    diff = difficulty(job, heists, board_tier)
    margin = (score - diff) / diff
    skill = clamp(B["skillBase"] + B["skillSlope"] * margin, B["skillMin"], B["skillMax"])
    # Role fit: crew whose type is a bonus role (beyond the required ones).
    req = list(job["requiredRoles"])
    fit = 0
    for c in crew:
        if c.type in req:
            req.remove(c.type)
        elif c.type in job["bonusRoles"]:
            fit += 1
    rolefit = min(B["roleFitCap"], B["roleFitPer"] * fit)
    wills = [willingness(c, cut) for c in crew]
    crew_will = sum(wills) / len(wills)
    wf = B["willFactorBase"] + B["willFactorScale"] * crew_will
    gen = B["generosityBonus"] if cut < min(fair_cut(c) for c in crew) else 0.0
    succ = clamp((skill + rolefit + gen + extra_bonus) * wf, B["successMin"], B["successMax"])
    refusers = [c for c, w in zip(crew, wills) if w < B["refusalThreshold"]]
    return succ, margin, refusers


def resolve(rng, job, crew, succ, margin, heists):
    """Returns band. ASSUME: Busted = failed roll in the top bustedBand of the roll range."""
    luck = sum(c.luck for c in crew)
    for attempt in range(2):
        roll = rng.random()
        if roll < succ * B["cleanBand"]:
            return "Clean"
        if roll < succ:
            return "Messy"
        if attempt == 0 and rng.random() < B["luckyBreakPerLuck"] * luck:
            continue  # lucky break: free re-roll
        return "Busted" if roll >= 1 - B["bustedBand"] else "Failed"
    return "Failed"


def payout_mult(board_tier):
    # ASSUME: payout x tierPayoutMult^(boardTier-1); a job on its own tier counts as boardTier = job.tier.
    return B["tierPayoutMult"] ** (board_tier - 1)


def sensible_crew(rng, job, level=1, gear_primary=0):
    types = list(job["requiredRoles"])
    if len(types) < job["maxCrew"] and job["bonusRoles"]:
        types.append(job["bonusRoles"][0])
    crew = [Crim(rng, t, level) for t in types]
    for c in crew:
        c.loyalty = 50
        if gear_primary:
            c.gear[TYPES[c.type]["primary"]] = gear_primary
    return crew


def pct(x):
    return f"{100 * x:5.1f}%"


# ---------------------------------------------------------------- report 1+2
def report_tier1(trials):
    print("== 1. Tier-1 success, fresh crew, loyalty 50, heists 0 ==")
    print(f"{'job':16} {'crew':34} {'solo/min':>9} {'sensible@30%':>13} {'@40%':>7} {'@70%':>7}")
    rng = random.Random(1)
    for jid in ("corner_store", "car_boost", "data_siphon", "jewellery_grab"):
        job = JOBS[jid]
        acc = {"min": 0, 0.3: 0, 0.4: 0, 0.7: 0}
        for _ in range(trials):
            crew = sensible_crew(rng, job)
            acc["min"] += chance(job, crew[:job["minCrew"]], 0.3, 0)[0]
            for cut in (0.3, 0.4, 0.7):
                acc[cut] += chance(job, crew, cut, 0)[0]
        names = "+".join(c.type for c in sensible_crew(rng, job))
        print(f"{jid:16} {names:34} {pct(acc['min'] / trials):>9} {pct(acc[0.3] / trials):>13} "
              f"{pct(acc[0.4] / trials):>7} {pct(acc[0.7] / trials):>7}")

    print("\n== 2. Cut sweep: random fresh sensible crews with rolled loyalty (Jewellery Smash & Grab) ==")
    print("   EV take = P(job can run: nobody refuses) * success * mean payout * cut")
    job = JOBS["jewellery_grab"]
    mean_pay = sum(job["payoutRange"]) / 2
    print(f"{'cut':>5} {'success':>8} {'refuse':>7} {'EV take':>9}")
    for cut10 in range(10, 95, 10):
        cut = cut10 / 100
        s = r = ev = 0.0
        for _ in range(trials):
            crew = sensible_crew(rng, job)
            for c in crew:
                c.loyalty = rng.randint(*TYPES[c.type]["loyaltyRange"])
            succ, _, ref = chance(job, crew, cut, 0)
            s += succ
            if ref:
                r += 1
            else:
                ev += succ * mean_pay * cut
        print(f"{cut10:>4}% {pct(s / trials):>8} {pct(r / trials):>7} {ev / trials:>9,.0f}")


# ---------------------------------------------------------------- report 3
def doable(job, roster):
    avail = [c.type for c in roster if c.injured == 0]
    for r in job["requiredRoles"]:
        if r in avail:
            avail.remove(r)
        else:
            return False
    return len(job["requiredRoles"]) <= job["maxCrew"]


def best_assignment(job, roster, cut, heists, used):
    """Greedy: fill required roles with the best-scoring person, then add extras while chance improves."""
    pool = [c for c in roster if c.injured == 0 and c not in used]
    crew = []
    for r in job["requiredRoles"]:
        cands = [c for c in pool if c.type == r and c not in crew]
        if not cands:
            return None, 0
        crew.append(max(cands, key=lambda c: c.eff(TYPES[r]["primary"])))
    while len(crew) < job["maxCrew"]:
        base = chance(job, crew, cut, heists)[0]
        opts = [(chance(job, crew + [c], cut, heists)[0], c) for c in pool if c not in crew]
        if not opts:
            break
        best = max(opts, key=lambda x: x[0])
        if best[0] <= base + 0.01:
            break
        crew.append(best[1])
    while len(crew) < job["minCrew"]:
        rest = [c for c in pool if c not in crew]
        if not rest:
            return None, 0
        crew.append(rest[0])
    return crew, chance(job, crew, cut, heists)[0]


def start_roster(rng):
    while True:
        types = [rng.choice(list(TYPES)) for _ in range(B["startingRoster"])]
        if len(set(types)) >= 3:
            return [Crim(rng, t) for t in types]


def roll_board(rng, roster, size, unlocked_tier, heavy):
    pool = [j for j in JOBS.values() if j["tier"] <= unlocked_tier and not j["finalCut"] and (heavy or not j["heavy"])]
    for _ in range(50):
        board = [rng.choice(pool) for _ in range(size)]
        if sum(doable(j, roster) for j in board) >= min(2, size):
            return board
    return board


def sim_run(rng, nights, cut, build_first=None):
    """Simple policy: each night run the best-EV doable jobs (one per crew slot). Optionally build a room ASAP
    once cash >= cost + 2 nights of upkeep. Returns per-night cash list, nights to afford rooms, bankrupt flag."""
    roster = start_roster(rng)
    cash, heists = B["startingCash"], 0
    built = {"Office"}
    crews_per_night = B["baseCrewsPerNight"]
    afford = {}
    earned = 0
    cash_curve = []
    for night in range(1, nights + 1):
        tier = max(i + 1 for i, h in enumerate(B["tierUnlockHeists"]) if heists >= h)
        board = roll_board(rng, roster, B["startingBoard"] if night == 1 else B["baseJobBoard"], min(tier, 3),
                           "Garage" in built)
        used = []
        plans = []
        for _ in range(crews_per_night):
            best = None
            for job in board:
                if job in [p[0] for p in plans]:
                    continue
                crew, succ = best_assignment(job, roster, cut, heists, used)
                if not crew or chance(job, crew, cut, heists)[2]:
                    continue  # missing roles, or someone refuses the cut
                bt = job["tier"]
                ev = succ * sum(job["payoutRange"]) / 2 * payout_mult(bt) * cut
                if best is None or ev > best[3]:
                    best = (job, crew, succ, ev)
            if best:
                plans.append(best)
                used += best[1]
        night_take = 0
        for job, crew, succ, _ in plans:
            _, margin, _ = chance(job, crew, cut, heists)
            band = resolve(rng, job, crew, succ, margin, heists)
            if band in ("Clean", "Messy"):
                pay = rng.randint(*job["payoutRange"]) * payout_mult(job["tier"])
                night_take += int(pay * cut)
                heists += 1
                for c in crew:
                    c.loyalty = clamp(c.loyalty + (B["loyalty"]["successFair"] if cut <= fair_cut(c) else B["loyalty"]["greedy"]), 0, 100)
            for c in crew:
                c.add_xp(job["xpReward"] * B["xpOutcomeMult"][band])
            inj_p = {"Messy": B["messyInjuryChance"], "Failed": B["failedInjuryChance"], "Busted": B["failedInjuryChance"]}.get(band, 0)
            nights_rg = B["messyInjuryNights"] if band == "Messy" else B["failedInjuryNights"]
            if band in ("Failed", "Busted"):
                arrest = job["arrestRisk"] * (1 - margin) + B["arrestRiskPer5Heists"] * (heists // 5)
                if band == "Busted":
                    arrest *= B["bustedArrestMult"]
                for c in list(crew):
                    if rng.random() < clamp(arrest, 0, 1):
                        roster.remove(c)
            for c in crew:
                if c in roster and rng.random() < inj_p:
                    c.injured = rng.randint(*nights_rg)
                    c.loyalty = max(0, c.loyalty + B["loyalty"]["injury"])
        upkeep = B["upkeepPerCriminal"] * len(roster) + B["upkeepPerHeistBlock"] * (heists // B["upkeepHeistBlock"])
        upkeep += sum(ROOMS[r]["upkeepPerNight"] for r in built)
        cash += night_take - upkeep
        earned += night_take - upkeep
        for c in roster:
            c.injured = max(0, c.injured - 1)
        for rname in ("Lounge", "Workshop", "Garage"):
            if rname not in afford and earned >= ROOMS[rname]["buildCost"]:
                afford[rname] = night
        if build_first and build_first not in built:
            room = ROOMS[build_first]
            if cash >= room["buildCost"] + 2 * upkeep and heists >= room["requiredHeists"]:
                cash -= room["buildCost"]
                built.add(build_first)
                if build_first == "Garage":
                    crews_per_night = 2
                afford["built_" + build_first] = night
        # Recruit when short-handed (ASSUME: hire cost = hireCostBase * recruitCostTierMult^(tier-1)).
        # Policy: refill to the roster cap if cash stays above ~4 nights of upkeep; always hire if below 3.
        cap = 6 if "Lounge" in built else B["baseRosterCap"]
        if len(roster) < cap:
            t = rng.choice(list(TYPES))
            cost = int(TYPES[t]["hireCostBase"] * B["recruitCostTierMult"] ** (min(tier, 3) - 1))
            if cash >= cost + (upkeep if len(roster) < 3 else 4 * upkeep):
                cash -= cost
                roster.append(Crim(rng, t, statbonus=B["recruitStatBonusPerTier"] * (min(tier, 3) - 1)))
        cash_curve.append(cash)
        if cash < 0 or not roster:
            return cash_curve, afford, "Broke" if cash < 0 else "NoCrew", heists
    return cash_curve, afford, None, heists


def report_economy(trials):
    print("\n== 3. Early economy (greedy best-EV policy, no gear, refills roster to cap with a cash buffer) ==")
    n = max(200, trials // 10)
    rng = random.Random(7)
    nights = 15
    curves, bankrupt, heists_tot, reasons = [], 0, 0, {}
    payback = {"Lounge": [], "Workshop": [], "Garage": []}
    for _ in range(n):
        curve, afford, broke, heists = sim_run(rng, nights, 0.30)
        curves.append(curve)
        bankrupt += broke is not None
        reasons[broke] = reasons.get(broke, 0) + 1
        heists_tot += heists
        for k in payback:
            payback[k].append(afford.get(k, 99))
    def median(xs):
        xs = sorted(xs)
        return xs[len(xs) // 2]
    for k, v in payback.items():
        print(f"  net profit covers the {k:8} (${ROOMS[k]['buildCost']:,}) by night: median {median(v) if median(v) < 99 else '>15'}"
              f"  (within 4 nights: {pct(sum(x <= 4 for x in v) / n)})")
    for night in (1, 3, 5, 10, 15):
        vals = [c[night - 1] for c in curves if len(c) >= night]
        print(f"  cash after night {night:2}: median ${median(vals):>8,}  (runs alive {len(vals)}/{n})")
    print(f"  game over within {nights} nights: {pct(bankrupt / n)} {reasons}, mean heists {heists_tot / n:.1f}")
    for cut in (0.4, 0.7):
        rng = random.Random(7)
        res = [sim_run(rng, nights, cut) for _ in range(n)]
        print(f"  [cut {int(cut*100)}%] median cash after {nights}: ${median([r[0][-1] for r in res]):>8,}, "
              f"game over {pct(sum(r[2] is not None for r in res) / n)}, mean heists {sum(r[3] for r in res) / n:.1f}")
    for first in ("Lounge", "Workshop", "Garage"):
        rng = random.Random(11)
        res = [sim_run(rng, nights, 0.30, build_first=first) for _ in range(n)]
        bn = [r[1].get("built_" + first, 99) for r in res]
        why = {}
        for r in res:
            if r[2]:
                why[r[2]] = why.get(r[2], 0) + 1
        print(f"  build {first:8} ASAP: built by night median {median(bn) if median(bn) < 99 else '>15'}, {why} "
              f"game over {pct(sum(r[2] is not None for r in res) / n)}, median cash after {nights}: ${median([r[0][-1] for r in res]):,}")


# ---------------------------------------------------------------- report 4
def report_later(trials):
    print("\n== 4. Later tiers: sensible crew at unlock heist count (level L, +G gear on primary), cut 30% ==")
    rng = random.Random(3)
    tu = B["tierUnlockHeists"]
    cases = [("gallery_lift", tu[1], 2, 0), ("gallery_lift", tu[1], 2, 10), ("casino_con", tu[1], 2, 10),
             ("armoured_truck", tu[2], 3, 10), ("armoured_truck", tu[2], 4, 22), ("bank_vault", tu[2], 4, 22),
             ("corner_store", tu[2], 4, 10), ("jewellery_grab", tu[3], 5, 22),
             ("the_premiere", B["finalCut"]["requiredHeists"], 5, 22), ("the_premiere", B["finalCut"]["requiredHeists"], 6, 34)]
    for jid, heists, lvl, gear in cases:
        job = JOBS[jid]
        tot = 0.0
        for _ in range(trials // 4):
            crew = sensible_crew(rng, job, lvl, gear)
            if jid == "the_premiere":
                crew = crew[:5]
            tot += chance(job, crew, 0.30, heists)[0]
        pay = sum(job["payoutRange"]) / 2 * payout_mult(job["tier"])
        print(f"  {jid:15} heists {heists:2} L{lvl} +{gear:2}: success {pct(tot / (trials // 4))}, mean payout ${pay:,.0f}")


if __name__ == "__main__":
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    report_tier1(trials)
    report_economy(trials)
    report_later(trials)
