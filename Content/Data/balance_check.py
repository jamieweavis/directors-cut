#!/usr/bin/env python3
"""Monte-Carlo balance check for Director's Cut v2 (realtime crew & missions).

Simulates the schema.md rules (success formula, board posting/expiry, durations, max active missions,
rooms with build timers and effects, payday at dawn, upkeep, recruits, XP/levels, perks, injuries,
arrests, The Big Score) with a simple greedy bot:
  - start every mission it can staff at >= 50% (best expected take first; The Big Score always first)
  - buy a Tool from the shop when it would counter an open threat on the plan (keeps the upkeep buffer)
  - build the cheapest affordable room/upgrade, keeping a 2-day upkeep buffer
  - hire the best affordable recruit while roster < cap (same buffer)
  - Director's cut 0.30, perks picked at random from the offer

Run:  python3 Content/Data/balance_check.py [--runs 200] [--minutes 120] [--seed 1] [--verbose]
Python 3 stdlib only. The summary table is printed first.

Sim assumptions where schema.md leaves room (see BALANCE.md): The Big Score posts as an extra, non-expiring
board slot once unlocked and re-posts after a failure; recruits roll levels from the unlocked tier's levelRange;
negative traits are rolled as an extra trait; unlockTier = v lets tiers >= v post; loot/gadgets/patch-up/bail
are not used by the bot.

Last tuned result: see BALANCE.md.
"""
import argparse
import json
import math
import os
import random
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
TYPES = ["Wheelman", "Muscle", "Hacker", "Ghost", "Face", "Safecracker"]
QUALITIES = ["Street", "Pro", "Elite"]
DT = 5  # sim step (s); durations are multiples of 5

TARGETS = {
    "first_success": "<= 2.5 min",
    "first_room": "<= 6 min",
    "tier2": "~10-12 min",
    "tier3": "~25-35 min",
    "final_unlock": "~55-75 min",
    "win_rate": "30-60%",
    "game_over": "10-25%",
}


def load(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        return json.load(f)


class Data:
    def __init__(self):
        self.b = load("balance.json")
        self.abilities = {a["id"]: a for a in load("abilities.json")["abilities"]}
        self.traits = {t["id"]: t for t in load("traits.json")["traits"]}
        self.types = {t["type"]: t for t in load("criminal_types.json")["types"]}
        self.missions = load("missions.json")["missions"]
        self.by_id = {m["id"]: m for m in self.missions}
        self.by_tier = {t: [m for m in self.missions if m["tier"] == t and not m["final"]] for t in (1, 2, 3, 4)}
        self.gear = {g["id"]: g for g in load("gear.json")["gear"]}
        self.rooms = {r["type"]: r for r in load("rooms.json")["rooms"]}
        self.perks = load("perks.json")["perks"]


class Crew:
    __slots__ = ("id", "type", "quality", "level", "xp", "abilities", "traits", "loyalty", "busy_until",
                 "on_mission", "gear")

    def __init__(self, cid, ty, quality, level, abilities, traits, loyalty):
        self.id, self.type, self.quality, self.level = cid, ty, quality, level
        self.xp, self.abilities, self.traits, self.loyalty = 0.0, abilities, traits, loyalty
        self.busy_until, self.on_mission, self.gear = 0, False, []


class Sim:
    def __init__(self, data, seed, minutes, verbose=False):
        self.d, self.b = data, data.b
        self.rng = random.Random(seed)
        self.end_t = minutes * 60
        self.verbose = verbose
        self.t = 0
        self.cash = self.b["startingCash"]
        self.next_id = 1
        self.crew = []
        self.rooms = {"Office": 1}
        self.building = None  # (room, level, ends_at)
        self.perks = []
        self.successes = 0
        self.prevented_arrest = False
        self.board = []    # postings
        self.active = []   # running missions
        self.recruits = []
        self.stats = {"first_success": None, "first_room": None, "tier2": None, "tier3": None, "tier4": None,
                      "final_unlock": None, "won": None, "game_over": None, "arrests": 0, "quits": 0,
                      "by_tier": {t: [0, 0] for t in (1, 2, 3, 4, 5)}, "cash": [], "rooms_built": 0,
                      "upkeep_paid": [], "final_attempts": 0, "room_done": {}, "income": 0, "levels": {}, "hires": 0, "gear_bought": 0, "injuries": 0}
        self._eff_cache = None
        clk = self.b["clock"]
        self.start_hour, self.dawn, self.dusk = clk["startHour"], clk["dawnHour"], clk["duskHour"]
        self.gmps = clk["gameMinutesPerSecond"]
        # first dawn
        hours_to_dawn = (self.dawn - self.start_hour) % 24 or 24
        self.next_dawn = hours_to_dawn * 60 / self.gmps
        lo, hi = self.b["startingRosterLevels"]
        for ty in self.rng.sample(TYPES, self.b["startingRoster"]):
            self.crew.append(self.make_crew(ty, 0, self.rng.randint(lo, hi)))
        for tid in self.b["board"]["startingBoard"]:
            self.board.append(self.make_posting(self.d.by_id[tid]))
        while len(self.board) < self.board_size():
            self.post_random()
        self.next_post = self.t + self.post_interval()
        self.refresh_recruits()
        self.next_recruit_refresh = self.b["recruits"]["refreshSec"]

    # ---------------- effects ----------------
    def global_effects(self):
        """Perk + room effects (list of dicts), cached until rooms/perks change."""
        if self._eff_cache is None:
            effs = []
            for p in self.perks:
                effs.extend(p["effects"])
            for room, lvl in self.rooms.items():
                if lvl > 0:
                    effs.extend(self.d.rooms[room]["levels"][lvl - 1]["effects"])
            self._eff_cache = effs
        return self._eff_cache

    def gsum(self, kind):
        return sum(e["value"] for e in self.global_effects() if e["kind"] == kind)

    def gprod(self, kind, crew_types=None):
        v = 1.0
        for e in self.global_effects():
            if e["kind"] == kind:
                p = e.get("param")
                if p and crew_types is not None and p not in crew_types:
                    continue
                v *= e["value"]
        return v

    def gmax(self, kind, default):
        vals = [e["value"] for e in self.global_effects() if e["kind"] == kind]
        return max(vals) if vals else default

    def has(self, kind):
        return any(e["kind"] == kind for e in self.global_effects())

    def traits_of(self, c):
        ts = list(c.traits)
        for gid in c.gear:
            tr = self.d.gear[gid].get("trait")
            if tr:
                ts.append(tr)
        return ts

    def trait_effects(self, c):
        for tid in self.traits_of(c):
            for e in self.d.traits[tid]["effects"]:
                yield e

    def holder_prod(self, c, kind):
        v = 1.0
        for e in self.trait_effects(c):
            if e["kind"] == kind:
                v *= e["value"]
        return v

    # ---------------- derived limits ----------------
    def roster_cap(self):
        return 4 + int(self.gsum("rosterCap"))

    def max_active(self):
        return 1 + int(self.gsum("maxActive"))

    def board_size(self):
        return self.b["board"]["baseSize"] + int(self.gsum("boardSize"))

    def post_interval(self):
        return self.b["board"]["postIntervalSec"] * self.gprod("postIntervalMult")

    def gear_slots(self):
        return self.b["gear"]["baseSlots"] + int(self.gsum("gearSlots"))

    def gear_tier(self):
        return int(self.gmax("gearTier", 1))

    def upkeep(self, extra_crew=0, extra_room=0):
        crew = (len(self.crew) + extra_crew) * self.b["upkeep"]["perCriminalPerDay"] * self.gprod("upkeepMult")
        rooms = sum(self.d.rooms[r]["upkeepPerDay"][l - 1] for r, l in self.rooms.items() if l > 0)
        return int(round(crew + rooms + extra_room))

    def unlocked_tier(self):
        us = self.b["tiers"]["unlockSuccesses"]
        room_tier = self.gmax("unlockTier", 99)
        best = 1
        for t in (2, 3, 4):
            if self.successes >= us[t - 1] and (t < 3 or room_tier <= t):
                best = t
        return best

    def final_unlocked(self):
        return self.has("unlockFinal") and self.successes >= self.b["final"]["requiredSuccesses"]

    def hour(self, t=None):
        t = self.t if t is None else t
        return (self.start_hour + t * self.gmps / 60.0) % 24

    def is_night(self, t=None):
        h = self.hour(t)
        return h >= self.dusk or h < self.dawn

    # ---------------- generation ----------------
    def make_crew(self, ty, quality, level):
        tdef = self.d.types[ty]
        nab = self.b["quality"]["abilities"][quality]
        abilities = self.rng.sample(tdef["abilities"], nab)
        pos = [t for t in tdef["traitPool"] if not self.d.traits[t]["negative"]]
        neg = [t for t in tdef["traitPool"] if self.d.traits[t]["negative"]]
        traits = self.rng.sample(pos, min(len(pos), self.b["quality"]["traits"][quality]))
        if neg and self.rng.random() < self.b["quality"]["negativeTraitChance"][quality]:
            traits.append(self.rng.choice(neg))
        c = Crew(self.next_id, ty, quality, level, abilities, traits, self.b["loyalty"]["start"])
        self.next_id += 1
        return c

    def make_posting(self, m):
        rr = self.rng
        lo, hi = self.b["tiers"]["levelRange"][m["tier"] - 1]
        th = list(m["threats"]["fixed"])
        n_extra = rr.randint(*m["threats"]["extra"])
        th += rr.sample(m["threats"]["pool"], n_extra)
        exp_lo, exp_hi = self.b["board"]["expireSecRange"]
        bo = self.b["board"]
        p = {
            "m": m, "tier": m["tier"], "level": rr.randint(lo, hi),
            "crew": rr.randint(*m["crewSize"]),
            "dur": 5 * round(rr.randint(*m["durationSec"]) / 5),
            "threats": th,
            "cost": rr.randint(*m["cost"]), "payout": rr.randint(*m["payout"]),
            "night": rr.random() < m["nightChance"],
            "rare": not m["final"] and rr.random() < bo["rareChance"],
            "expires": math.inf if m["final"] else self.t + rr.uniform(exp_lo, exp_hi),
        }
        if p["rare"]:
            # rare postings: richer, a level tougher, an extra threat (pool first, then any)
            p["payout"] = int(p["payout"] * bo.get("rarePayoutMult", 1.0))
            p["level"] = min(self.b["levels"]["max"], p["level"] + bo.get("rareLevelAdd", 0))
            more = [t for t in m["threats"]["pool"] if t not in th]
            all_threats = ["Guards", "Cameras", "Alarms", "Locks", "Vault", "Patrols", "Witnesses", "Pursuit"]
            if len(more) < bo.get("rareExtraThreats", 0):
                more += [t for t in all_threats if t not in th and t not in more]
            rr.shuffle(more)
            p["threats"] = th + more[:bo.get("rareExtraThreats", 0)]
        return p

    def post_random(self):
        ut = self.unlocked_tier()
        w = self.b["board"]["tierWeights"][ut - 1]
        tier = self.rng.choices([1, 2, 3, 4], weights=w)[0]
        pool = self.d.by_tier[tier]
        if not pool:
            return
        m = self.rng.choices(pool, weights=[x["weight"] for x in pool])[0]
        self.board.append(self.make_posting(m))

    def refresh_recruits(self):
        rc = self.b["recruits"]
        ut = self.unlocked_tier()
        odds = list(rc["qualityOdds"][ut - 1])
        shift = self.gsum("recruitQualityAdd")
        lo, hi = self.b["tiers"]["levelRange"][ut - 1]
        self.recruits = []
        size = max(1, rc["boardSize"] + int(round(self.gsum("recruitBoardAdd"))))
        bag = []
        for _ in range(size):
            if not bag:
                bag = list(TYPES)
                self.rng.shuffle(bag)
            q = self.rng.choices([0, 1, 2], weights=odds)[0]
            s = shift
            while s > 0 and q < 2:
                if s >= 1 or self.rng.random() < s:
                    q += 1
                s -= 1
            lvl = min(self.b["levels"]["max"], self.rng.randint(lo, hi) + int(self.gsum("recruitLevelAdd")))
            c = self.make_crew(bag.pop(), q, lvl)
            self.recruits.append((c, self.hire_cost(lvl, q)))
        # under two crew: keep enough cheap Street hires listed (matches RegenerateRecruits)
        street = sum(1 for c, _ in self.recruits if c.quality == 0)
        for i, (c, _) in enumerate(self.recruits):
            if street >= 2 - len(self.crew):
                break
            if c.quality == 0:
                continue
            nc = self.make_crew(c.type, 0, c.level)
            self.recruits[i] = (nc, self.hire_cost(c.level, 0))
            street += 1

    def hire_cost(self, lvl, q):
        rc = self.b["recruits"]
        return int(round((rc["costBase"] + rc["costPerLevel"] * (lvl - 1)) * rc["qualityCostMult"][q] / 50) * 50)

    # ---------------- success formula ----------------
    def counters_of(self, c):
        out = [self.d.abilities[a]["counters"] for a in c.abilities]
        out += [self.d.gear[g]["counters"] for g in c.gear if self.d.gear[g].get("counters")]
        return out

    def eff_level(self, c):
        return c.level + sum(self.d.gear[g]["power"] for g in c.gear)

    def level_factor(self, c, mlvl):
        lf = self.b["success"]["levelFactor"]
        el = self.eff_level(c)
        if el >= mlvl:
            return min(lf["maxFactor"], lf["atOrAbove"] + lf["perLevelAbove"] * (el - mlvl))
        return lf["below"][min(3, math.ceil(mlvl - el)) - 1]

    def final_duration(self, p, team):
        mult = self.gprod("durationMult")
        for c in team:
            mult *= self.holder_prod(c, "durationMult")
        return max(self.b["minDurationMult"], mult) * p["dur"]

    def chance(self, p, team, cut=0.3, start_t=None):
        sc = self.b["success"]
        tp = sc["threatPoints"]
        points = len(team) + tp * len(p["threats"])
        crew_ch = sum(self.level_factor(c, p["level"]) for c in team) / points
        # counters: greedy in threat order, preferring the lowest-level crew member's counter
        pool = []
        for c in team:
            for th in self.counters_of(c):
                pool.append([c.level, th, False])
        pool.sort(key=lambda x: x[0])
        countered = 0
        for th in p["threats"]:
            for slot in pool:
                if not slot[2] and slot[1] == th:
                    slot[2] = True
                    countered += 1
                    break
        threat_ch = tp * countered / points
        night = self.is_night(start_t)
        dur = self.final_duration(p, team)
        types = [c.type for c in team]
        mod = 0.0
        for c in team:
            for e in self.trait_effects(c):
                mod += self.cond_chance(e, p, team, night, dur, types, holder=c)
        for e in self.global_effects():
            mod += self.cond_chance(e, p, team, night, dur, types, holder=None)
        if p["night"] and night:
            mod += self.b["night"]["missionBonus"]
        will = [self.willingness(c, cut) for c in team]
        cutb = self.b["cut"]
        wf = cutb["willFactorBase"] + cutb["willFactorScale"] * (sum(will) / len(will))
        return max(0.0, min(sc["maxChance"], (crew_ch + threat_ch + mod) * wf)), countered

    def cond_chance(self, e, p, team, night, dur, types, holder):
        k, v = e["kind"], e["value"]
        if k == "chance":
            return v
        if k == "chanceNight":
            return v if night else 0
        if k == "chanceDay":
            return v if not night else 0
        if k == "chanceShort":
            return v if dur <= e["threshold"] else 0
        if k == "chanceLong":
            return v if dur >= e["threshold"] else 0
        if k == "chanceSolo":
            return v if len(team) == 1 else 0
        if k == "chanceThreat":
            return v if e["param"] in p["threats"] else 0
        if k == "chanceWithType":
            return v if any(c is not holder and c.type == e["param"] for c in team) else 0
        if k == "chanceTierMin":
            return v if p["tier"] >= e["threshold"] else 0
        if k == "chanceIfType":
            return v if e["param"] in types else 0
        return 0

    def fair_cut(self, c):
        cb = self.b["cut"]
        add = sum(e["value"] for e in self.trait_effects(c) if e["kind"] == "fairCutAdd") + self.gsum("fairCutAdd")
        return cb["fairCutBase"] + (c.loyalty - 50) / cb["fairCutLoyaltyDiv"] + add

    def willingness(self, c, cut):
        cb = self.b["cut"]
        wmin = max(cb["willMin"], self.gmax("willMin", 0))
        return max(wmin, min(1.0, 1 - cb["willSlope"] * max(0.0, cut - self.fair_cut(c))))

    # ---------------- bot planning ----------------
    def idle_crew(self):
        return [c for c in self.crew if not c.on_mission and c.busy_until <= self.t]

    def plan(self, p, idle):
        n = p["crew"]
        if len(idle) < n:
            return None, 0.0
        best_team, best_ch = None, -1.0
        # candidate A: counter-first, then highest level
        cands = []
        team, need = [], list(p["threats"])
        for th in need:
            if len(team) >= n:
                break
            opts = [c for c in idle if c not in team and th in self.counters_of(c)]
            if opts:
                team.append(max(opts, key=self.eff_level))
        rest = sorted([c for c in idle if c not in team], key=self.eff_level, reverse=True)
        cands.append(team + rest[: n - len(team)])
        # candidate B: highest level
        cands.append(sorted(idle, key=self.eff_level, reverse=True)[:n])
        for team in cands:
            ch, _ = self.chance(p, team)
            if ch > best_ch:
                best_team, best_ch = team, ch
        # one improvement pass: try swapping each member with each bench player
        if len(idle) > n and n > 1:
            bench = [c for c in idle if c not in best_team]
            for i in range(n):
                for bc in bench[:8]:
                    t2 = best_team[:i] + [bc] + best_team[i + 1:]
                    ch, _ = self.chance(p, t2)
                    if ch > best_ch + 1e-9:
                        best_team, best_ch = t2, ch
        return best_team, best_ch

    def try_buy_tool(self, p, team, budget):
        """Buy one Tool that counters an uncountered threat for a team member with a free slot."""
        used = []
        for c in team:
            used += self.counters_of(c)
        open_threats = list(p["threats"])
        for th in used:
            if th in open_threats:
                open_threats.remove(th)
        if not open_threats:
            return False
        price_mult = self.gprod("gearPriceMult")
        tier = self.gear_tier()
        for th in open_threats:
            tools = [g for g in self.d.gear.values() if not g["loot"] and g["counters"] == th and g["tier"] <= tier]
            if not tools:
                continue
            g = min(tools, key=lambda x: x["price"])
            price = int(g["price"] * price_mult)
            if price > budget:
                continue
            holders = [c for c in team if len(c.gear) < self.gear_slots()]
            if not holders:
                return False
            min(holders, key=lambda c: len(c.gear)).gear.append(g["id"])
            self.cash -= price
            self.stats["gear_bought"] += 1
            return True
        return False

    def bot_missions(self):
        while len(self.active) < self.max_active():
            idle = self.idle_crew()
            if not idle:
                return
            best = None
            for p in self.board:
                if p["crew"] > len(idle):
                    continue
                cost = p["cost"]
                team, ch = self.plan(p, idle)
                if team is None:
                    continue
                cmult = 1.0
                for c in team:
                    cmult *= self.holder_prod(c, "costMult")
                cost = int(cost * cmult * self.gprod("costMult"))
                buffer = 0  # greedy: any mission it can afford
                budget = self.cash - cost - buffer
                for _ in range(3):
                    if ch >= 0.95 or budget <= 0:
                        break
                    if not self.try_buy_tool(p, team, budget):
                        break
                    budget = self.cash - cost - buffer
                    ch, _ = self.chance(p, team)
                if ch < 0.5 or cost > self.cash:
                    continue
                take = p["payout"] * 0.3 * self.payout_mult(p, team)
                ev = ch * take - cost
                score = (1 if p["m"]["final"] else 0, ev)
                if best is None or score > best[0]:
                    best = (score, p, team, ch, cost)
            if best is None:
                return
            _, p, team, ch, cost = best
            self.start(p, team, ch, cost)

    def payout_mult(self, p, team):
        v = self.gprod("payoutMult", crew_types={c.type for c in team})
        for c in team:
            v *= self.holder_prod(c, "payoutMult")
        return v

    def start(self, p, team, ch, cost):
        self.board.remove(p)
        self.cash -= cost
        for c in team:
            c.on_mission = True
            if 0.3 > self.fair_cut(c) + 1e-9:
                c.loyalty += self.b["loyalty"]["greedy"]
        dur = self.final_duration(p, team)
        self.active.append({"p": p, "team": team, "ch": ch, "ends": self.t + dur})
        tier_key = 5 if p["m"]["final"] else p["tier"]
        self.stats["by_tier"][tier_key][0] += 1
        if p["m"]["final"]:
            self.stats["final_attempts"] += 1

    def resolve(self, a):
        p, team, ch = a["p"], a["team"], a["ch"]
        b = self.b
        r = self.rng.random()
        s1 = min(ch, 1.0)
        types = {c.type for c in team}
        xp_base = p["m"]["xpReward"] * self.gprod("xpMult")
        for c in team:
            c.on_mission = False
        if r < s1:
            payout = p["payout"] * self.payout_mult(p, team)
            if ch > 1 and self.rng.random() < ch - 1:
                payout *= b["outcome"]["bonusPayoutMult"]
            self.cash += int(payout * 0.3)
            self.stats["income"] += int(payout * 0.3)
            self.successes += 1
            tier_key = 5 if p["m"]["final"] else p["tier"]
            self.stats["by_tier"][tier_key][1] += 1
            if self.stats["first_success"] is None:
                self.stats["first_success"] = self.t
            for c in team:
                if 0.3 <= self.fair_cut(c) + 1e-9:
                    c.loyalty = min(b["loyalty"]["max"], c.loyalty + b["loyalty"]["successFair"])
                self.give_xp(c, xp_base * self.holder_prod(c, "xpMult"))
            if self.successes % b["perks"]["everyNSuccesses"] == 0:
                owned = {x["id"] for x in self.perks}
                offer = [x for x in self.d.perks if x["id"] not in owned]
                if offer:
                    self.perks.append(self.rng.choice(self.rng.sample(offer, min(len(offer), b["perks"]["offerSize"]))))
                    self._eff_cache = None
            if p["m"]["final"]:
                self.stats["won"] = self.t
            return
        busted = r >= 1 - (1 - s1) * b["outcome"]["bustedBand"]
        inj_ch = b["injury"]["bustedChance"] if busted else b["injury"]["failChance"]
        for c in list(team):
            self.give_xp(c, xp_base * self.holder_prod(c, "xpMult") * b["outcome"]["failXpMult"])
            if busted:
                arrest = p["m"]["arrestRisk"] * self.gprod("arrestMult") * self.holder_prod(c, "arrestMult")
                if self.rng.random() < arrest:
                    if self.has("preventFirstArrest") and not self.prevented_arrest:
                        self.prevented_arrest = True
                    else:
                        self.crew.remove(c)
                        self.stats["arrests"] += 1
                        continue
            ic = inj_ch * self.gprod("injuryChanceMult") * self.holder_prod(c, "injuryChanceMult")
            if self.rng.random() < ic:
                secs = self.rng.uniform(*b["injury"]["secondsRange"]) * self.gprod("injuryTimeMult")
                c.busy_until = self.t + secs
                c.loyalty += b["loyalty"]["injury"]
                self.stats["injuries"] += 1
        if p["m"]["final"]:
            p["expires"] = math.inf
            self.board.append(p)  # The Big Score stays available after a failure

    def give_xp(self, c, xp):
        lv = self.b["levels"]
        c.xp += xp
        while c.level < lv["max"]:
            need = lv["xpBase"] * c.level ** lv["xpExp"]
            if c.xp < need:
                break
            c.xp -= need
            c.level += 1

    def bot_rooms(self):
        if self.building:
            return
        opts = []
        for rt, rd in self.d.rooms.items():
            lvl = self.rooms.get(rt, 0)
            if lvl >= 3:
                continue
            if self.successes < rd["requiredSuccesses"][lvl]:
                continue
            cost = rd["cost"][lvl]
            extra_upkeep = rd["upkeepPerDay"][lvl] - (rd["upkeepPerDay"][lvl - 1] if lvl else 0)
            if self.cash - cost < 2 * self.upkeep(extra_room=extra_upkeep):
                continue
            opts.append((cost, rt, lvl + 1))
        if opts:
            cost, rt, lvl = min(opts)
            self.cash -= cost
            self.building = (rt, lvl, self.t + self.d.rooms[rt]["buildSec"][lvl - 1])

    def bot_hire(self):
        if len(self.crew) >= self.roster_cap() or not self.recruits:
            return
        best = None
        for i, (c, cost) in enumerate(self.recruits):
            if self.cash - cost < 2 * self.upkeep(extra_crew=1):
                continue
            score = c.level + c.quality * 1.5
            if best is None or score > best[0]:
                best = (score, i)
        if best:
            c, cost = self.recruits.pop(best[1])
            self.cash -= cost
            self.crew.append(c)
            self.stats["hires"] += 1

    # ---------------- main loop ----------------
    def tick(self):
        st = self.stats
        # finished missions
        for a in [a for a in self.active if a["ends"] <= self.t]:
            self.active.remove(a)
            self.resolve(a)
            if st["won"] is not None:
                return False
        # room builds
        if self.building and self.building[2] <= self.t:
            rt, lvl, _ = self.building
            self.rooms[rt] = lvl
            self.building = None
            self._eff_cache = None
            st["rooms_built"] += 1
            st["room_done"].setdefault(f"{rt} L{lvl}", self.t)
            if st["first_room"] is None:
                st["first_room"] = self.t
        # tiers
        ut = self.unlocked_tier()
        for t in (2, 3, 4):
            key = f"tier{t}"
            if ut >= t and st[key] is None:
                st[key] = self.t
        if self.final_unlocked():
            if st["final_unlock"] is None:
                st["final_unlock"] = self.t
                self.board.append(self.make_posting(self.d.by_id[self.b["final"]["templateId"]]))
        # board
        self.board = [p for p in self.board if p["expires"] > self.t]
        normal = [p for p in self.board if not p["m"]["final"]]
        if len(normal) >= self.board_size():
            self.next_post = self.t + self.post_interval()
        elif self.t >= self.next_post:
            self.post_random()
            self.next_post = self.t + self.post_interval()
        # recruits
        if self.t >= self.next_recruit_refresh:
            self.refresh_recruits()
            self.next_recruit_refresh = self.t + self.b["recruits"]["refreshSec"]
        # payday
        if self.t >= self.next_dawn:
            up = self.upkeep()
            self.cash -= up
            st["upkeep_paid"].append(up)
            if self.cash < self.b["gameOver"]["cashFloor"]:
                st["game_over"] = self.t
                return False
            idle_loy = self.gsum("loyaltyIdlePerDay")
            for c in list(self.crew):
                if c.busy_until <= self.t:
                    c.loyalty = min(self.b["loyalty"]["max"], c.loyalty + idle_loy)
                if c.loyalty < self.b["loyalty"]["quitBelow"] and not c.on_mission:
                    self.crew.remove(c)
                    st["quits"] += 1
            self.next_dawn += 24 * 60 / self.gmps
        # bot
        self.bot_missions()
        self.bot_rooms()
        self.bot_hire()
        return True

    def run(self):
        while self.t < self.end_t:
            if self.t % 600 == 0:
                self.stats["cash"].append(self.cash)
                lv = sorted((c.level for c in self.crew), reverse=True)
                self.stats["levels"][self.t // 60] = (lv[4] if len(lv) > 4 else 0, len(lv), self.successes)
            if not self.tick():
                break
            self.t += DT
        return self.stats


def pct(xs, q):
    xs = sorted(xs)
    if not xs:
        return None
    k = (len(xs) - 1) * q
    f, c = math.floor(k), math.ceil(k)
    return xs[f] + (xs[c] - xs[f]) * (k - f)


def fmt_min(v):
    return "-" if v is None else f"{v / 60:.1f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=200)
    ap.add_argument("--minutes", type=int, default=120)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    d = Data()
    results = [Sim(d, args.seed * 100003 + i, args.minutes).run() for i in range(args.runs)]
    n = len(results)

    def times(key):
        return [r[key] for r in results if r[key] is not None]

    rows = []
    for key, label in [("first_success", "First success"), ("first_room", "First room built"),
                       ("tier2", "Tier 2 unlocked"), ("tier3", "Tier 3 unlocked"), ("tier4", "Tier 4 unlocked"),
                       ("final_unlock", "Big Score unlocked"), ("won", "Big Score won"),
                       ("game_over", "Game over")]:
        ts = times(key)
        rows.append((label, f"{len(ts) / n * 100:5.1f}%", fmt_min(pct(ts, 0.25)), fmt_min(pct(ts, 0.5)),
                     fmt_min(pct(ts, 0.75)), TARGETS.get(key if key != "won" else "win_rate",
                                                         TARGETS.get(key, ""))))
    print(f"== Director's Cut balance check: {n} runs x {args.minutes} min, greedy bot, cut 0.30 ==\n")
    print(f"{'Milestone':<20}{'reached':>9}{'p25':>7}{'median':>8}{'p75':>7}   target (min / rate)")
    for r in rows:
        print(f"{r[0]:<20}{r[1]:>9}{r[2]:>7}{r[3]:>8}{r[4]:>7}   {r[5]}")
    wins = len(times("won")) / n * 100
    gos = len(times("game_over")) / n * 100
    print(f"\nWin rate {wins:.1f}% (target 30-60%) | game-over rate {gos:.1f}% (target 10-25%)")

    print("\nMission success rate by tier (all runs):")
    for t in (1, 2, 3, 4, 5):
        att = sum(r["by_tier"][t][0] for r in results)
        suc = sum(r["by_tier"][t][1] for r in results)
        name = "Big Score" if t == 5 else f"Tier {t}"
        rate = f"{suc / att * 100:5.1f}%" if att else "    -"
        print(f"  {name:<10} attempts/run {att / n:6.2f}   success {rate}")
    arrests = [r["arrests"] for r in results]
    print(f"\nArrests/run: mean {statistics.mean(arrests):.2f}, max {max(arrests)}, "
          f"runs with >=1 arrest {sum(1 for a in arrests if a) / n * 100:.0f}%")
    print(f"Quits/run {statistics.mean(r['quits'] for r in results):.2f} | hires/run "
          f"{statistics.mean(r['hires'] for r in results):.1f} | tools bought/run "
          f"{statistics.mean(r['gear_bought'] for r in results):.1f} | rooms built/run "
          f"{statistics.mean(r['rooms_built'] for r in results):.1f} | injuries/run "
          f"{statistics.mean(r['injuries'] for r in results):.1f}")
    ups = [r["upkeep_paid"] for r in results]
    for day in range(5):
        vals = [u[day] for u in ups if len(u) > day]
        if vals:
            print(f"  Payday {day + 1} (t={23 + 24 * day} min): median upkeep {statistics.median(vals):,.0f} "
                  f"({len(vals)} runs reached it)")

    print("\nMedian cash every 10 min (runs still alive):")
    line = []
    for i in range(args.minutes // 10 + 1):
        vals = [r["cash"][i] for r in results if len(r["cash"]) > i]
        if vals:
            line.append(f"{i * 10:>3}m {statistics.median(vals):>8,.0f}")
    for i in range(0, len(line), 4):
        print("  " + " | ".join(line[i:i + 4]))
    print("\nRoom completion (median min, % of runs):")
    keys = sorted({k for r in results for k in r["room_done"]}, key=lambda k: statistics.median(
        [r["room_done"][k] for r in results if k in r["room_done"]]))
    line = [f"{k} {statistics.median([r['room_done'][k] for r in results if k in r['room_done']]) / 60:.0f}m "
            f"({sum(1 for r in results if k in r['room_done']) / n * 100:.0f}%)" for k in keys]
    for i in range(0, len(line), 5):
        print("  " + " | ".join(line[i:i + 5]))
    print(f"Director income/run median {statistics.median(r['income'] for r in results):,.0f}")
    print("Roster at t (median 5th-best level L, roster size n, successes s):")
    line = []
    for m in range(0, args.minutes + 1, 20):
        vals = [r["levels"][m] for r in results if m in r["levels"]]
        if vals:
            line.append(f"{m}m L{statistics.median(v[0] for v in vals):.0f} n{statistics.median(v[1] for v in vals):.0f}"
                        f" s{statistics.median(v[2] for v in vals):.0f}")
    print("  " + " | ".join(line))
    go = times("game_over")
    if go:
        print(f"\nGame-over times (min): {sorted(round(x / 60) for x in go)}")


if __name__ == "__main__":
    main()
