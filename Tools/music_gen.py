#!/usr/bin/env python3
"""Director's Cut: procedural lounge score generator.

Writes looping, layered music stems into Content/RawAudio (MUS_<Set>_<Stem>.wav) and merges them into
manifest.json / LICENSES.md, so Tools/audio_import.py imports them with everything else. Every note is
composed and synthesised here from seeded rules (no samples, no recordings, no external material).

    venv/bin/python Tools/music_gen.py            # all sets
    venv/bin/python Tools/music_gen.py DayA       # one set

Each *set* is a 32-bar form (A A' B A'', four 8-bar phrases) with a fixed key, tempo and chord
progression, rendered as five stems of exactly the same length that loop seamlessly:

    Core    electric piano comping + bass           (always on)
    Groove  drums / brushes / shaker                 (light early on, full later)
    MelA    lead melody, variant A (vibraphone)
    MelB    lead melody, variant B (soft flute)      (a different tune over the same changes)
    Color   counter-colour: nylon guitar + congas by day, warm pad + bell figures by night

UDCAudioSubsystem starts all five stems of a set in the same frame and mixes them per 8-bar phrase,
from the clock (day / night) and the run's progress (unlocked mission tier). See Audio/DCAudioAssets.h.
"""
import json
import os
import sys
import wave

import numpy as np
from scipy import signal

SR = 32000                     # music only: warm lounge material, keeps the stems small
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Content", "RawAudio")
TAU = 2 * np.pi
BARS = 32
STEMS = ["Core", "Groove", "MelA", "MelB", "Color"]
MONO_STEMS = {"MelA", "MelB"}

# ───────────────────────────── DSP ─────────────────────────────


def ns(d):
    return int(round(d * SR))


def tt(n):
    return np.arange(n) / SR


def mhz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def _sos(bt, f, o):
    f = np.clip(np.atleast_1d(np.asarray(f, float)) / (SR / 2), 1e-4, 0.999)
    return signal.butter(o, f if len(f) > 1 else f[0], btype=bt, output="sos")


def lp(x, f, o=2):
    return signal.sosfilt(_sos("lowpass", f, o), x, axis=-1)


def hp(x, f, o=2):
    return signal.sosfilt(_sos("highpass", f, o), x, axis=-1)


def bp(x, lo, hi, o=2):
    return signal.sosfilt(_sos("bandpass", [lo, hi], o), x, axis=-1)


def rng(seed):
    return np.random.default_rng(seed)


def env_ar(n, a, rel_at, rel):
    """Attack a s, hold, release starting at rel_at s with time-constant rel."""
    t = tt(n)
    e = np.clip(t / max(a, 1e-4), 0, 1)
    e = e * e * (3 - 2 * e)
    off = t > rel_at
    e[off] *= np.exp(-(t[off] - rel_at) / rel)
    return e


def saw_blep(freq):
    inc = freq / SR
    ph = np.cumsum(inc) % 1.0
    y = 2 * ph - 1
    m = ph < inc
    x = ph[m] / inc[m]
    y[m] -= x + x - x * x - 1
    m = ph > 1 - inc
    x = (ph[m] - 1) / inc[m]
    y[m] -= x * x + x + x + 1
    return y


def make_ir(dur, seed, dark_tau, bright_tau=0.18, predelay=0.018):
    n = ns(dur)
    out = []
    for ch in range(2):
        w = rng(seed + ch).standard_normal(n)
        ir = (lp(w, 3000) * np.exp(-tt(n) / dark_tau) + hp(w, 3000) * np.exp(-tt(n) / bright_tau) * 0.5)
        ir *= 1 - np.exp(-tt(n) / 0.008)
        ir = np.concatenate([np.zeros(ns(predelay)), ir])
        rr = rng(seed + 20 + ch)
        for _ in range(8):
            ir[ns(rr.uniform(0.005, 0.05))] += rr.uniform(-1, 1) * 2.5
        out.append(ir / np.sqrt(np.sum(ir ** 2)))
    return np.vstack(out)


def wrap(x, L):
    """Fold everything past L back onto the start: a seamless loop of periodic material."""
    out = x[..., :L].copy()
    tail = x[..., L:]
    while tail.shape[-1] > 0:
        k = min(L, tail.shape[-1])
        out[..., :k] += tail[..., :k]
        tail = tail[..., k:]
    return out


def loop_reverb(x, ir, wet):
    """Reverb a loop (stereo or mono) so the tail of the end rings into the start."""
    L = x.shape[-1]
    if x.ndim == 1:
        y = signal.fftconvolve(x, ir[0])
        return x + wrap(y, L) * wet
    y = np.vstack([signal.fftconvolve(x[c], ir[c]) for c in range(2)])
    return x + wrap(y, L) * wet


def rms_db(x):
    return 20 * np.log10(np.sqrt(np.mean(np.asarray(x) ** 2)) + 1e-12)


def pan2(x, p):
    a = (p + 1) * np.pi / 4
    return np.vstack([x * np.cos(a), x * np.sin(a)])


# ───────────────────────────── instruments (mono, start at t=0) ─────────────────────────────


def rhodes(m, dur, vel, seed):
    f = mhz(m)
    n = ns(dur + 1.6)
    t = tt(n)
    dec = 2.4 * (2 ** (-(m - 60) / 24))
    idx = vel * (1.6 * np.exp(-t / 0.22) + 0.35)
    car = np.sin(TAU * f * t + idx * np.sin(TAU * f * t))
    tine = np.sin(TAU * f * 7.0 * t + 0.6) * np.exp(-t / 0.035) * 0.12 * vel
    if f * 7 > SR * 0.45:
        tine[:] = 0
    body = 0.25 * np.sin(TAU * 2 * f * t) * np.exp(-t / (dec * 0.4))
    e = env_ar(n, 0.003, dur, 0.18) * np.exp(-t / dec)
    y = (car + body) * e + tine * env_ar(n, 0.001, dur, 0.1)
    return np.tanh(y * vel * 1.3) * 0.8


def bass(m, dur, vel, seed, upright=False):
    f = mhz(m)
    n = ns(dur + 0.5)
    t = tt(n)
    dec = 1.1 if upright else 1.8
    y = (np.sin(TAU * f * t) + 0.45 * np.sin(TAU * 2 * f * t) * np.exp(-t / 0.35)
         + 0.18 * np.sin(TAU * 3 * f * t) * np.exp(-t / 0.18) + 0.06 * np.sin(TAU * 4 * f * t) * np.exp(-t / 0.08))
    if upright:
        y *= 1 + 0.004 * np.sin(TAU * 5.2 * t)
    click = lp(rng(seed).standard_normal(n), 1800) * np.exp(-t / 0.006) * 0.25
    e = env_ar(n, 0.005, dur * 0.95, 0.05) * np.exp(-t / dec)
    return lp((y + click) * e * vel, 1400 if upright else 1100)


def vibes(m, dur, vel, seed, motor=5.2):
    f = mhz(m)
    n = ns(dur + 2.0)
    t = tt(n)
    y = (np.sin(TAU * f * t) * np.exp(-t / 2.2)
         + 0.22 * np.sin(TAU * 3.94 * f * t) * np.exp(-t / 0.35)
         + 0.06 * np.sin(TAU * 9.9 * f * t) * np.exp(-t / 0.08) * (9.9 * f < SR * 0.45))
    trem = 1 - 0.28 * (0.5 + 0.5 * np.sin(TAU * motor * t + rng(seed).uniform(0, TAU)))
    mallet = bp(rng(seed + 1).standard_normal(n), 2000, 6000) * np.exp(-t / 0.004) * 0.15
    e = env_ar(n, 0.002, dur + 0.25, 0.25)
    return (y * trem + mallet) * e * vel


def flute(m, dur, vel, seed, dark=False):
    f = mhz(m)
    n = ns(dur + 0.4)
    t = tt(n)
    vib = 1 + 0.0045 * np.clip((t - 0.25) / 0.4, 0, 1) * np.sin(TAU * 5.0 * t + rng(seed).uniform(0, TAU))
    ph = TAU * np.cumsum(f * vib) / SR
    y = np.sin(ph) + 0.22 * np.sin(2 * ph) + 0.07 * np.sin(3 * ph) + 0.03 * np.sin(4 * ph)
    breath = bp(rng(seed + 3).standard_normal(n), f * 1.5, min(f * 4, SR * 0.4)) * 0.05
    chiff = bp(rng(seed + 4).standard_normal(n), 2000, 7000) * np.exp(-t / 0.02) * 0.05
    e = env_ar(n, 0.06, dur, 0.09)
    swell = 0.85 + 0.15 * np.clip(t / max(dur, 0.1), 0, 1)
    y = (y + breath) * e * swell + chiff
    return lp(y * vel, 3500 if dark else 6000)


def nylon(m, dur, vel, seed):
    """Additive plucked nylon string (exact pitch)."""
    f = mhz(m)
    n = ns(dur + 1.2)
    t = tt(n)
    y = np.zeros(n)
    p = 0.2
    for k in range(1, 14):
        if k * f > SR * 0.42:
            break
        a = abs(np.sin(k * np.pi * p)) / k ** 1.1
        y += a * np.sin(TAU * k * f * t * (1 + 0.0004 * k)) * np.exp(-t / (1.4 / (1 + 0.35 * (k - 1))))
    pick = hp(rng(seed).standard_normal(n), 2500) * np.exp(-t / 0.004) * 0.08
    e = env_ar(n, 0.002, dur, 0.12)
    return (y + pick) * e * vel


def pad_voice(m, dur, vel, seed):
    f = mhz(m)
    n = ns(dur + 1.5)
    t = tt(n)
    r = rng(seed)
    y = np.zeros(n)
    for d in (-0.06, 0.06):
        fr = f * 2 ** (d / 12) * (1 + 0.0015 * np.sin(TAU * r.uniform(0.15, 0.35) * t + r.uniform(0, TAU)))
        y += saw_blep(fr)
    y = lp(y, min(f * 3.2, 2200), 2)
    e = env_ar(n, 0.9, dur, 0.7)
    return y * e * vel * 0.25


def bell(m, dur, vel, seed):
    f = mhz(m)
    n = ns(2.5)
    t = tt(n)
    y = (np.sin(TAU * f * t) * np.exp(-t / 1.4) + 0.3 * np.sin(TAU * 2.76 * f * t) * np.exp(-t / 0.5)
         + 0.12 * np.sin(TAU * 5.4 * f * t) * np.exp(-t / 0.15) * (5.4 * f < SR * 0.45))
    return y * env_ar(n, 0.002, 2.0, 0.3) * vel


# drums

def kick(vel, seed):
    n = ns(0.45)
    t = tt(n)
    f = 48 + 45 * np.exp(-t / 0.035)
    y = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / 0.22)
    y += lp(rng(seed).standard_normal(n), 1200) * np.exp(-t / 0.004) * 0.15
    return np.tanh(y * 1.4) * vel


def rim(vel, seed):
    n = ns(0.12)
    t = tt(n)
    y = (np.sin(TAU * 1700 * t) * np.exp(-t / 0.010) + 0.6 * np.sin(TAU * 520 * t) * np.exp(-t / 0.018)
         + bp(rng(seed).standard_normal(n), 2500, 6000) * np.exp(-t / 0.006) * 0.6)
    return y * vel * 0.7


def shaker(vel, seed, length=0.05):
    n = ns(length + 0.05)
    t = tt(n)
    w = hp(rng(seed).standard_normal(n), 5500, 2)
    e = np.clip(t / 0.008, 0, 1) * np.exp(-np.clip(t - 0.008, 0, None) / (length * 0.45))
    return w * e * vel * 0.35


def hat(vel, seed, open_=False):
    n = ns(0.6 if open_ else 0.12)
    t = tt(n)
    w = hp(rng(seed).standard_normal(n), 7000, 2)
    metal = sum(np.sign(np.sin(TAU * f * t)) for f in (3140, 4410, 5230, 6170)) * 0.04
    y = hp(w + metal, 6000) * np.exp(-t / (0.2 if open_ else 0.03))
    return y * vel * 0.45


def brush_tap(vel, seed):
    n = ns(0.3)
    t = tt(n)
    w = bp(rng(seed).standard_normal(n), 1500, 9000)
    y = w * np.exp(-t / 0.07) + np.sin(TAU * 190 * t) * np.exp(-t / 0.03) * 0.3
    return y * vel * 0.5


def ride(vel, seed):
    n = ns(1.6)
    t = tt(n)
    r = rng(seed)
    y = np.zeros(n)
    for f in (3020, 3580, 4170, 4890, 5330, 6020):
        y += np.sin(TAU * f * t + r.uniform(0, TAU)) * np.exp(-t / r.uniform(0.5, 1.1))
    y = hp(y * 0.1 + hp(r.standard_normal(n), 6000) * np.exp(-t / 0.4) * 0.3, 2500)
    y += bp(r.standard_normal(n), 3000, 8000) * np.exp(-t / 0.006) * 0.3
    return y * vel * 0.35


def conga(vel, seed, high=True):
    f0 = 330 if high else 220
    n = ns(0.35)
    t = tt(n)
    f = f0 * (1 + 0.12 * np.exp(-t / 0.02))
    y = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / (0.12 if high else 0.18))
    y += bp(rng(seed).standard_normal(n), 800, 3000) * np.exp(-t / 0.005) * 0.4
    return y * vel * 0.6


# ───────────────────────────── harmony ─────────────────────────────

NOTE = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "Gb": 6, "G": 7, "Ab": 8, "A": 9, "Bb": 10, "B": 11}
QUAL = {  # chord tones, rootless voicing, melody scale (all semitones above the root)
    "maj7": ([0, 4, 7, 11], [4, 7, 11, 14], [0, 2, 4, 7, 9, 11]),
    "6": ([0, 4, 7, 9], [4, 7, 9, 14], [0, 2, 4, 7, 9]),
    "m7": ([0, 3, 7, 10], [3, 7, 10, 14], [0, 2, 3, 5, 7, 9, 10]),
    "m9": ([0, 3, 7, 10, 14], [3, 7, 10, 14], [0, 2, 3, 5, 7, 9, 10]),
    "7": ([0, 4, 7, 10], [4, 9, 10, 14], [0, 2, 4, 7, 9, 10]),
    "9": ([0, 4, 7, 10, 14], [4, 7, 10, 14], [0, 2, 4, 7, 9, 10]),
    "m7b5": ([0, 3, 6, 10], [3, 6, 10, 12], [0, 3, 5, 6, 8, 10]),
    "mM7": ([0, 3, 7, 11], [3, 7, 11, 14], [0, 2, 3, 5, 7, 9, 11]),
}


def parse(sym):
    """'Fmaj7' -> (5, 'maj7')."""
    root = sym[:2] if len(sym) > 1 and sym[1] in "b" else sym[:1]
    return NOTE[root], sym[len(root):]


def chord_grid(bars):
    """bars: list of strings, 'Fmaj7' or 'Gm7 C7' (two chords, two beats each). -> list per beat."""
    grid = []
    for b in bars:
        cs = b.split()
        if len(cs) == 1:
            grid += [parse(cs[0])] * 4
        else:
            grid += [parse(cs[0])] * 2 + [parse(cs[1])] * 2
    return grid


def chord_events(bars):
    """[(beat, dur_beats, (root, qual))] merging repeated chords within a bar."""
    ev = []
    for i, b in enumerate(bars):
        cs = b.split()
        if len(cs) == 1:
            ev.append((i * 4, 4, parse(cs[0])))
        else:
            ev.append((i * 4, 2, parse(cs[0])))
            ev.append((i * 4 + 2, 2, parse(cs[1])))
    return ev


def voice(chord, prev, lo, hi):
    """Rootless voicing in [lo, hi] that moves least from prev."""
    r, q = chord
    ivs = QUAL[q][1]
    best, bc = None, 1e9
    for base in range(lo - 12, hi):
        if (base - r) % 12:
            continue
        v = sorted(base + i for i in ivs)
        for inv in range(len(v)):
            w = sorted(v[inv:] + [x + 12 for x in v[:inv]])
            for sh in (-12, 0, 12):
                u = [x + sh for x in w]
                if u[0] < lo or u[-1] > hi:
                    continue
                c = sum(abs(a - b) for a, b in zip(u, prev)) if prev else abs(np.mean(u) - (lo + hi) / 2) * 4
                if c < bc:
                    best, bc = u, c
    return best


def nearest_pc(m, pcs, root):
    """Nearest midi to m whose (m - root) % 12 is in pcs."""
    for d in (0, -1, 1, -2, 2, -3, 3, -4, 4, -5, 5, -6, 6):
        if (m + d - root) % 12 in pcs:
            return m + d
    return m


# ───────────────────────────── sets ─────────────────────────────

SETS = {
    # Daytime, opening chapters: a sunny, easy bossa in F.
    "DayA": dict(bpm=96, swing=0.5, style="bossa", seed=1100, center=74, bars=[
        "Fmaj7", "Fmaj7", "Gm7", "C7", "Am7", "D7", "Gm7", "C9",
        "Fmaj7", "F6", "Bbmaj7", "Eb9", "Am7", "D7", "Gm7 C7", "Fmaj7",
        "Bbmaj7", "Bbm7 Eb9", "Am7", "D7", "Gm7", "Gm7 C7", "Am7 D7", "Gm7 C7",
        "Fmaj7", "Dm7", "Gm7", "C7", "Am7 D7", "Gm7 C7", "Fmaj7", "Gm7 C9"]),
    # Daytime, later chapters: brighter, busier, in Eb with a lift to Ab.
    "DayB": dict(bpm=104, swing=0.5, style="bossa", seed=1200, center=75, bars=[
        "Ebmaj7", "Cm7", "Fm7", "Bb7", "Gm7", "C7", "Fm7", "Bb9",
        "Ebmaj7", "Cm7", "Fm7", "Bb7", "Abmaj7", "Db9", "Gm7 C7", "Fm7 Bb7",
        "Abmaj7", "Abm7 Db9", "Gm7", "C7", "Fm7", "Bb7", "Gm7 C7", "Fm7 Bb7",
        "Ebmaj7", "Cm7", "Abmaj7", "Db9", "Gm7 C7", "Fm7 Bb7", "Ebmaj7", "Fm7 Bb9"]),
    # Night, opening chapters: slow brushed swing in D minor.
    "NightA": dict(bpm=82, swing=0.63, style="swing", seed=1300, center=72, bars=[
        "Dm9", "Dm9", "Em7b5 A7", "Dm9", "Gm7", "C9", "Fmaj7", "Em7b5 A7",
        "Dm9", "Bbmaj7", "Em7b5", "A7", "Dm9", "Gm7 C7", "Fmaj7", "Em7b5 A7",
        "Bbmaj7", "Bbmaj7", "Am7", "Dm9", "Gm7", "C7", "Fmaj7", "Em7b5 A7",
        "Dm9", "Dm9", "Gm7", "C9", "Bbmaj7", "Em7b5 A7", "Dm9", "Em7b5 A7"]),
    # Night, later chapters: a warmer late-hours groove in C / A minor with ride cymbal.
    "NightB": dict(bpm=88, swing=0.62, style="swing", seed=1400, center=74, bars=[
        "Am9", "Dm7", "G9", "Cmaj7", "Fmaj7", "Bm7b5 E7", "Am9", "Am9",
        "Am9", "Dm7", "G9", "Cmaj7", "Fmaj7", "Bm7b5 E7", "Am9", "Gm7 C7",
        "Fmaj7", "Fm7 Bb9", "Em7", "Am9", "Dm7", "G7", "Cmaj7", "Bm7b5 E7",
        "Am9", "Dm7", "G9", "Cmaj7", "Fmaj7", "Bm7b5 E7", "Am9", "Bm7b5 E7"]),
}

class Song:
    def __init__(self, cfg, name):
        self.cfg = cfg
        self.name = name
        self.bpm = cfg["bpm"]
        self.spb = 60.0 / self.bpm
        self.L = ns(BARS * 4 * self.spb)
        pad = ns(4.0)
        self.buf = {s: np.zeros((1 if s in MONO_STEMS else 2, self.L + pad)) for s in STEMS}
        self.grid = chord_grid(cfg["bars"])
        self.events = chord_events(cfg["bars"])
        self.r = rng(cfg["seed"])

    def bt(self, beat):
        """Beat -> seconds, with swung off-beat eighths and a little humanising."""
        w = np.floor(beat + 1e-6)
        fr = beat - w
        if abs(fr - 0.5) < 1e-6:
            fr = self.cfg["swing"]
        return (w + fr) * self.spb

    def add(self, stem, sig, beat, gain=1.0, pan=0.0, jitter=0.006):
        t = self.bt(beat) + (self.r.uniform(-jitter, jitter) if jitter else 0.0)
        i = max(0, ns(t))
        b = self.buf[stem]
        if b.shape[0] == 1:
            s = sig[None, :] if sig.ndim == 1 else sig[:1]
        else:
            s = pan2(sig, pan) if sig.ndim == 1 else sig
        end = min(b.shape[1], i + s.shape[1])
        b[:, i:end] += s[:, : end - i] * gain

    def chord_at(self, beat):
        return self.grid[int(beat) % len(self.grid)]


# ───────────────────────────── parts ─────────────────────────────


def part_core(S):
    st = S.cfg["style"]
    r = rng(S.cfg["seed"] + 1)
    prev = None
    epiano = np.zeros(1)
    # comping
    for k, (b0, d, ch) in enumerate(S.events):
        v = voice(ch, prev, 55, 76)
        prev = v
        bar = b0 // 4
        if st == "bossa":
            if d == 4:
                pats = [[(0, 1.4, 0.55), (2.5, 1.4, 0.42)], [(0, 3.4, 0.5)], [(0.5, 1.0, 0.45), (2, 1.9, 0.48)],
                        [(0, 1.0, 0.5), (1.5, 2.3, 0.44)]]
            else:
                pats = [[(0, 1.4, 0.5)], [(0.5, 1.4, 0.45)]]
        else:
            if d == 4:
                pats = [[(0, 3.6, 0.48)], [(0, 1.4, 0.48), (2.5, 1.4, 0.36)], [(-0.5, 3.8, 0.46)], [(0, 2.4, 0.46), (3.5, 0.4, 0.3)]]
            else:
                pats = [[(0, 1.8, 0.45)], [(-0.5, 2.2, 0.42)]]
        pat = pats[r.integers(len(pats))] if bar % 8 != 7 else pats[0]
        for (o, dd, vel) in pat:
            bb = b0 + o
            if bb < 0:
                bb += BARS * 4
            for j, m in enumerate(v):
                # slight roll, top voice a touch louder
                sig = rhodes(m, dd * S.spb, vel * (1.08 if j == len(v) - 1 else 0.92) * r.uniform(0.92, 1.05), S.cfg["seed"] + k * 7 + j)
                S.add("Core", sig, bb + j * 0.012 / S.spb, 0.55, pan=-0.15 + 0.1 * j, jitter=0.004)
    # bass
    last = 38
    for k, (b0, d, ch) in enumerate(S.events):
        root, q = ch
        nxt = S.events[(k + 1) % len(S.events)][2][0]
        rm = min((m for m in range(28, 46) if (m - root) % 12 == 0), key=lambda m: abs(m - last))
        fifth = rm + 7 if rm + 7 <= 45 else rm - 5
        nr = min((m for m in range(28, 46) if (m - nxt) % 12 == 0), key=lambda m: abs(m - rm))
        appr = nr + (1 if r.random() < 0.5 else -1)
        if st == "bossa":
            if d == 4:
                notes = [(0, rm, 1.4, 0.9), (1.5, fifth, 0.45, 0.55), (2, fifth, 1.4, 0.8), (3.5, rm if r.random() < 0.6 else appr, 0.45, 0.55)]
            else:
                notes = [(0, rm, 1.4, 0.9), (1.5, fifth, 0.45, 0.55)]
            up = False
        else:
            third = rm + (3 if q in ("m7", "m9", "m7b5", "mM7") else 4)
            if d == 4:
                if r.random() < 0.55:
                    notes = [(0, rm, 1.8, 0.85), (2, fifth, 1.4, 0.7), (3.5, appr, 0.45, 0.5)]
                else:
                    notes = [(0, rm, 0.95, 0.85), (1, third, 0.95, 0.65), (2, fifth, 0.95, 0.7), (3, appr, 0.9, 0.6)]
            else:
                notes = [(0, rm, 0.95, 0.85), (1, appr if k % 2 else fifth, 0.9, 0.6)]
            up = True
        for (o, m, dd, vel) in notes:
            S.add("Core", bass(m, dd * S.spb, vel, S.cfg["seed"] + 500 + k, upright=up), b0 + o, 0.9, pan=0.0, jitter=0.004)
        last = rm
    # gentle stereo tremolo on the whole core bed (period divides the loop)
    L = S.L
    cyc = max(1, round(L / SR * 0.18))
    lfo = np.sin(TAU * cyc * np.arange(S.buf["Core"].shape[1]) / L)
    S.buf["Core"][0] *= 1 + 0.06 * lfo
    S.buf["Core"][1] *= 1 - 0.06 * lfo


def part_groove(S):
    st = S.cfg["style"]
    r = rng(S.cfg["seed"] + 2)
    sd = S.cfg["seed"] * 10
    busy = S.name.endswith("B")
    for bar in range(BARS):
        b = bar * 4
        phrase_end = bar % 8 == 7
        if st == "bossa":
            for o, v in ((0, 0.8), (1.5, 0.4), (2, 0.72), (3.5, 0.4)):
                S.add("Groove", kick(v, sd + bar), b + o, 0.85)
            clave = [0, 1.5, 3] if bar % 2 == 0 else [1, 2.5]
            if phrase_end:
                clave = [0, 1.5, 2.5, 3, 3.5]
            for o in clave:
                S.add("Groove", rim(r.uniform(0.75, 0.95), sd + bar * 9 + int(o * 2)), b + o, 0.6, pan=-0.25)
            for e in range(8):
                acc = 0.95 if e % 2 else 0.55
                S.add("Groove", shaker(acc * r.uniform(0.85, 1.05), sd + bar * 17 + e), b + e * 0.5, 0.55, pan=0.35)
                if busy:
                    S.add("Groove", shaker(0.35 * r.uniform(0.8, 1.1), sd + bar * 23 + e, 0.03), b + e * 0.5 + 0.25, 0.5, pan=0.35)
            if busy:
                for e in range(4):
                    S.add("Groove", hat(0.5 if e % 2 else 0.35, sd + bar * 29 + e), b + e + 0.5, 0.5, pan=0.2)
                if phrase_end:
                    S.add("Groove", hat(0.45, sd + bar, open_=True), b + 3.5, 0.5, pan=0.2)
        else:
            S.add("Groove", kick(0.6, sd + bar), b, 0.7)
            if r.random() < 0.4 or phrase_end:
                S.add("Groove", kick(0.38, sd + bar + 1), b + 2.5, 0.7)
            # brush swirl: a continuous noise swish swelling every beat
            n = ns(4 * S.spb + 0.2)
            t = tt(n)
            w = bp(rng(sd + bar * 3).standard_normal(n), 2500, 9000)
            sw = (0.35 + 0.65 * np.abs(np.sin(np.pi * t / (S.spb * 2)))) * 0.09
            S.add("Groove", w * sw, b, 0.6, pan=-0.15, jitter=0)
            for o in (1, 3):
                S.add("Groove", brush_tap(r.uniform(0.75, 0.9), sd + bar * 5 + o), b + o, 0.75, pan=-0.1)
            if phrase_end:
                S.add("Groove", brush_tap(0.5, sd + bar * 5 + 9), b + 3.5, 0.75, pan=-0.1)
            for o in (1, 3):
                S.add("Groove", hat(0.35, sd + bar * 11 + o), b + o + 0.02, 0.6, pan=0.25)
            if busy:
                for o in (0, 1, 1.5, 2, 3, 3.5):
                    v = 0.75 if o in (1, 3) else 0.55
                    S.add("Groove", ride(v * r.uniform(0.85, 1.05), sd + bar * 13 + int(o * 2)), b + o, 0.55, pan=0.3)


def compose_melody(S, variant):
    """AABA tune over the set's changes. Returns [(beat, midi, dur_beats, vel)]."""
    cfg = S.cfg
    r = rng(cfg["seed"] + 100 + variant * 37)
    center = cfg["center"] + (0 if variant == 0 else 3)
    lo, hi = center - 8, center + 8
    swing = cfg["style"] == "swing"
    cells = [
        [(0.5, 0.5), (1, 1), (2.5, 0.5), (3, 1.5), (5, 0.5), (5.5, 2)],
        [(0, 1.5), (1.5, 0.5), (2, 1), (3.5, 0.5), (4, 3)],
        [(1, 0.5), (1.5, 0.5), (2, 0.5), (2.5, 1.5), (4.5, 0.5), (5, 0.5), (5.5, 2)],
        [(0, 0.5), (0.5, 0.5), (1, 2), (3.5, 0.5), (4, 0.5), (4.5, 1), (6, 1.5)],
        [(0.5, 1), (1.5, 1), (2.5, 1.5), (4.5, 0.5), (5, 2.5)],
        [(2, 0.5), (2.5, 0.5), (3, 0.5), (3.5, 1), (4.5, 3)],
    ]
    if variant == 1:
        cells = [[(o, d * 1.3) for o, d in c if True] for c in cells]  # longer, more legato (flute)
    pick = r.permutation(len(cells))
    call, resp, bridge = cells[pick[0]], cells[pick[1]], cells[pick[2]]
    contour_call = list(r.choice([-2, -1, 1, 2, 3, -3], size=8))
    contour_resp = [-c for c in contour_call]
    contour_bridge = list(r.choice([-1, 1, 2, -2, 4], size=8))

    def ladder(ch):
        root, q = ch
        sc = QUAL[q][2]
        return [m for m in range(lo, hi + 1) if (m - root) % 12 in sc]

    def fit(cell, contour, start_bar, anchor):
        out = []
        pos = anchor
        for k, (o, d) in enumerate(cell):
            beat = start_bar * 4 + o
            root, q = S.chord_at(beat)
            lad = ladder((root, q))
            target = pos
            if k > 0:
                idx = int(np.argmin([abs(m - pos) for m in lad]))
                step = int(contour[k % len(contour)])
                j = idx + step
                if j < 0 or j >= len(lad):
                    j = idx - step            # bounce off the edge of the range
                target = lad[int(np.clip(j, 0, len(lad) - 1))]
            strong = (abs(o - round(o)) < 1e-6 and int(round(o)) % 2 == 0) or d >= 1.5
            if strong:
                tones = set(x % 12 for x in QUAL[q][0]) | ({2} if q in ("maj7", "6", "m9", "9", "m7") else set())
                cands = [m for m in lad if (m - root) % 12 in tones] or lad
            else:
                cands = lad
            pos = min(cands, key=lambda m: abs(m - target))
            out.append([beat, pos, d, 0.8 + (0.12 if k == 0 else 0) + r.uniform(-0.06, 0.06)])
        return out

    notes = []
    for ph in range(4):          # A, A', B, A''
        base = ph * 8
        if ph == 2:
            notes += fit(bridge, contour_bridge, base, center + 2)
            notes += fit(resp, contour_bridge[::-1], base + 2, center + 4)
            notes += fit(bridge, [c - 1 for c in contour_bridge], base + 4, center)
        else:
            a0 = center + (0 if ph == 0 else (2 if ph == 1 else -1))
            notes += fit(call, contour_call, base, a0)
            notes += fit(resp, contour_resp, base + 2, a0 + 1)
            notes += fit(call, [c + (1 if ph == 1 else 0) for c in contour_call], base + 4, a0 + 2)
        # cadence in bar 7 of the phrase, bar 8 breathes
        ch = S.chord_at(base * 4 + 24)
        root, q = ch
        land = nearest_pc(center, {QUAL[q][0][1] % 12, (QUAL[q][0][0] + 7) % 12}, root)
        notes.append([base * 4 + 24 + (0.5 if swing else 0), land, 3.0, 0.78])
    # drop anything spilling into a phrase's breathing bar, keep the cadence
    out = []
    for n in sorted(notes, key=lambda x: x[0]):
        bar_in = int(n[0] // 4) % 8
        if bar_in == 7:
            continue
        end_bar = (n[0] + n[2]) / 4
        if int(end_bar) % 8 == 7 and end_bar - int(end_bar) > 0.0 and bar_in != 6:
            n[2] = max(0.5, (int(n[0] // 4) + 1) * 4 - n[0] - 0.1)
        if out and n[0] < out[-1][0] + out[-1][2]:
            out[-1][2] = max(0.25, n[0] - out[-1][0] - 0.05)
        out.append(n)
    return out


def part_melody(S, variant):
    stem = "MelA" if variant == 0 else "MelB"
    dark = S.cfg["style"] == "swing"
    for k, (beat, m, d, vel) in enumerate(compose_melody(S, variant)):
        if variant == 0:
            sig = vibes(m, d * S.spb, vel, S.cfg["seed"] + 900 + k, motor=4.2 if dark else 5.4)
            S.add(stem, sig, beat, 0.5)
        else:
            sig = flute(m, d * S.spb, vel, S.cfg["seed"] + 1900 + k, dark=dark)
            S.add(stem, sig, beat, 0.45)


def part_color(S):
    r = rng(S.cfg["seed"] + 3)
    sd = S.cfg["seed"] * 7
    if S.cfg["style"] == "bossa":
        prev = None
        for k, (b0, d, ch) in enumerate(S.events):
            v = voice(ch, prev, 52, 69)
            prev = v
            hits = [(0.5, 0.45, 0.55), (1.5, 0.45, 0.5), (2.5, 0.6, 0.6), (3.5, 0.45, 0.45)] if d == 4 else [(0.5, 0.45, 0.55), (1.5, 0.45, 0.5)]
            for (o, dd, vel) in hits:
                for j, m in enumerate(v):
                    S.add("Color", nylon(m, dd * S.spb, vel * r.uniform(0.85, 1.0), sd + k * 11 + j), b0 + o + j * 0.008 / S.spb, 0.5, pan=-0.45, jitter=0.003)
            root = ch[0]
            S.add("Color", nylon(min((m for m in range(40, 53) if (m - root) % 12 == 0)), 1.2 * S.spb, 0.6, sd + k), b0, 0.45, pan=-0.45)
        for bar in range(BARS):
            b = bar * 4
            for o, hi_, v in ((1, True, 0.5), (1.5, True, 0.7), (3, False, 0.6), (3.5, True, 0.75)):
                S.add("Color", conga(v * r.uniform(0.85, 1.05), sd + bar * 4 + int(o * 2), hi_), b + o, 0.55, pan=0.5)
    else:
        prev = None
        for k, (b0, d, ch) in enumerate(S.events):
            v = voice(ch, prev, 50, 72)
            prev = v
            for j, m in enumerate(v):
                S.add("Color", pad_voice(m, d * S.spb, 0.6, sd + k * 5 + j), b0, 0.6, pan=-0.6 + 0.4 * j, jitter=0)
        # soft bell arpeggio answering the end of every 4 bars
        for bar in range(3, BARS, 4):
            ch = S.chord_at(bar * 4)
            root, q = ch
            tones = [root + 72 + i for i in QUAL[q][0][:4]]
            for i, m in enumerate(tones[:: (1 if bar % 8 == 3 else -1)]):
                S.add("Color", bell(m, 1.0, 0.4, sd + bar * 9 + i), bar * 4 + 1.5 + i * 0.5, 0.35, pan=0.5 - 0.25 * i)


# ───────────────────────────── render ─────────────────────────────

STEM_RMS = {"Core": -26.0, "Groove": -31.0, "MelA": -29.0, "MelB": -29.5, "Color": -31.5}
REV_WET = {"Core": 0.22, "Groove": 0.14, "MelA": 0.32, "MelB": 0.3, "Color": 0.28}
MIX_RMS = -21.5          # whole set, every layer up (MelA as the lead)


def render(name):
    cfg = SETS[name]
    S = Song(cfg, name)
    part_core(S)
    part_groove(S)
    part_melody(S, 0)
    part_melody(S, 1)
    part_color(S)
    ir = make_ir(2.6 if cfg["style"] == "swing" else 2.0, cfg["seed"] + 77, dark_tau=0.42 if cfg["style"] == "swing" else 0.33)
    out = {}
    for s in STEMS:
        x = wrap(S.buf[s], S.L)
        if x.shape[0] == 1:
            x = x[0]
        x = hp(np.concatenate([x[..., -ns(1.0):], x], axis=-1), 35)[..., ns(1.0):]
        x = loop_reverb(x, ir, REV_WET[s])
        x = x * 10 ** ((STEM_RMS[s] - rms_db(x)) / 20)
        out[s] = x
    full = out["Core"] + out["Groove"] + out["MelA"] + out["Color"]
    g = 10 ** ((MIX_RMS - rms_db(full)) / 20)
    pk = np.max(np.abs(full)) * g
    if pk > 0.89:
        g *= 0.89 / pk
    for s in STEMS:
        out[s] = out[s] * g
    print(f"{name}: {cfg['bpm']} bpm, {S.L / SR:.1f}s loop, mix {rms_db(full * g):.1f} dB rms, peak {20 * np.log10(np.max(np.abs(full * g))):.1f} dB")
    return out


def write_wav(path, y):
    y = np.clip(y, -1, 1)
    d = (y.T if y.ndim > 1 else y[:, None]) * 32767
    with wave.open(path, "wb") as w:
        w.setnchannels(d.shape[1])
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(np.round(d).astype("<i2").tobytes())


def merge_manifest(names):
    mf = os.path.join(OUT, "manifest.json")
    with open(mf) as f:
        man = json.load(f)
    keep = [s for s in man["sounds"] if not s["file"].startswith("MUS_")]
    for set_name in SETS:
        for st in STEMS:
            n = f"MUS_{set_name}_{st}"
            keep.append({"file": n + ".wav", "asset": f"/Game/DC/Audio/{n}.{n}", "category": "Music", "loop": True,
                         "loudness": STEM_RMS[st]})
    man["sounds"] = keep
    with open(mf, "w") as f:
        json.dump(man, f, indent=2)
    lic = os.path.join(OUT, "LICENSES.md")
    with open(lic) as f:
        lines = [ln for ln in f.read().splitlines() if not ln.startswith("| MUS_")]
    lines = [ln.replace("There is no music in the game.", "Music: MUS_* stems are composed and synthesised by `Tools/music_gen.py` (seeded rules, no samples).") for ln in lines]
    for set_name, cfg in SETS.items():
        for st in STEMS:
            lines.append(f"| MUS_{set_name}_{st}.wav | Music | yes | {STEM_RMS[st]} (rms) | {set_name} lounge set, {st} stem, "
                         f"{cfg['bpm']} bpm | procedurally composed and generated by the project (Tools/music_gen.py) | project-owned original |")
    with open(lic, "w") as f:
        f.write("\n".join(lines) + "\n")


def main(argv):
    names = argv or list(SETS)
    for name in names:
        stems = render(name)
        for s, x in stems.items():
            write_wav(os.path.join(OUT, f"MUS_{name}_{s}.wav"), x)
        if os.environ.get("DC_MUSIC_PREVIEW"):
            mix = stems["Core"] + stems["Groove"] + stems["Color"] + np.vstack([stems["MelA"]] * 2)
            write_wav(os.path.join(os.environ["DC_MUSIC_PREVIEW"], f"preview_{name}.wav"), mix)
    merge_manifest(names)


if __name__ == "__main__":
    main(sys.argv[1:])
