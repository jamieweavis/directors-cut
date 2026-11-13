#!/usr/bin/env python3
"""Director's Cut: procedural audio generator.

Every sound in Content/RawAudio is synthesised from scratch by this script (no samples, no
third-party material), so it is owned by the project. Requires numpy + scipy:

    python3 -m venv /tmp/dcvenv && /tmp/dcvenv/bin/pip install numpy scipy
    /tmp/dcvenv/bin/python Tools/audio_gen.py               # all
    /tmp/dcvenv/bin/python Tools/audio_gen.py SFX_Click MUS_Planning   # some

Output: 16-bit PCM WAV, 44.1 kHz (mono SFX, stereo music/ambience/stingers),
plus Content/RawAudio/manifest.json and Content/RawAudio/LICENSES.md.
"""
import json
import os
import sys
import wave

import numpy as np
from scipy import signal

SR = 44100
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Content", "RawAudio")
TAU = 2 * np.pi


# ───────────────────────────── DSP helpers ─────────────────────────────

def R(seed):
    return np.random.default_rng(seed)


def ns(dur):
    return int(round(dur * SR))


def tt(n):
    return np.arange(n) / SR


def mhz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def _sos(btype, f, order):
    f = np.atleast_1d(np.asarray(f, dtype=float)) / (SR / 2)
    f = np.clip(f, 1e-4, 0.999)
    return signal.butter(order, f if len(f) > 1 else f[0], btype=btype, output="sos")


def lp(x, f, o=2):
    return signal.sosfilt(_sos("lowpass", f, o), x, axis=-1)


def hp(x, f, o=2):
    return signal.sosfilt(_sos("highpass", f, o), x, axis=-1)


def bp(x, lo, hi, o=2):
    return signal.sosfilt(_sos("bandpass", [lo, hi], o), x, axis=-1)


def peak(x, f, q, gain_db):
    """RBJ peaking EQ."""
    A = 10 ** (gain_db / 40)
    w = TAU * f / SR
    al = np.sin(w) / (2 * q)
    b = [1 + al * A, -2 * np.cos(w), 1 - al * A]
    a = [1 + al / A, -2 * np.cos(w), 1 - al / A]
    return signal.lfilter(b, a, x, axis=-1)


def white(n, seed):
    return R(seed).standard_normal(n)


def pink(n, seed, slope=0.5):
    X = np.fft.rfft(R(seed).standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    f[0] = f[1]
    X /= f ** slope
    y = np.fft.irfft(X, n)
    return y / (np.std(y) + 1e-12)


def edec(n, tau):
    return np.exp(-tt(n) / tau)


def att(n, a):
    return 1 - np.exp(-tt(n) / max(a, 1e-5))


def fades(x, fin=0.002, fout=0.01):
    x = np.array(x, dtype=float)
    n = x.shape[-1]
    a, b = min(ns(fin), n // 2), min(ns(fout), n // 2)
    if a > 0:
        x[..., :a] *= np.sin(np.linspace(0, np.pi / 2, a)) ** 2
    if b > 0:
        x[..., n - b:] *= np.cos(np.linspace(0, np.pi / 2, b)) ** 2
    return x


def modal(n, freqs, amps, decays, phase_seed=None):
    t = tt(n)
    y = np.zeros(n)
    ph = R(phase_seed).uniform(0, TAU, len(freqs)) if phase_seed is not None else np.zeros(len(freqs))
    for f, a, d, p in zip(freqs, amps, decays, ph):
        if f < SR * 0.45:
            y += a * np.sin(TAU * f * t + p) * np.exp(-t / d)
    return y


def pad_to(x, n):
    if x.shape[-1] >= n:
        return x[..., :n]
    pw = [(0, 0)] * (x.ndim - 1) + [(0, n - x.shape[-1])]
    return np.pad(x, pw)


def saw_blep(freq_arr):
    inc = freq_arr / SR
    ph = np.cumsum(inc) % 1.0
    y = 2 * ph - 1
    m = ph < inc
    x = ph[m] / inc[m]
    y[m] -= x + x - x * x - 1
    m = ph > 1 - inc
    x = (ph[m] - 1) / inc[m]
    y[m] -= x * x + x + x + 1
    return y


def stereo(x, pan=0.0):
    """Constant-power pan of a mono signal; pan -1..1."""
    a = (pan + 1) * np.pi / 4
    return np.vstack([x * np.cos(a), x * np.sin(a)])


class Mix:
    """Growable stereo buffers (one per stem) with timed, panned inserts."""

    def __init__(self, dur=1.0):
        self.n0 = ns(dur)
        self.stems = {}

    @property
    def b(self):
        return self.get()

    def add(self, sig, t0, gain=1.0, pan=0.0, stem="main"):
        s = stereo(sig, pan) if sig.ndim == 1 else sig
        i = ns(t0)
        if i < 0:
            s = s[:, -i:]
            i = 0
        buf = self.stems.setdefault(stem, np.zeros((2, self.n0)))
        end = i + s.shape[1]
        if end > buf.shape[1]:
            buf = np.pad(buf, ((0, 0), (0, end - buf.shape[1] + SR)))
            self.stems[stem] = buf
        buf[:, i:end] += s * gain

    def get(self, n=None):
        m = max([b.shape[1] for b in self.stems.values()] + [self.n0])
        out = np.zeros((2, m))
        for b in self.stems.values():
            out[:, : b.shape[1]] += b
        return out if n is None else pad_to(out, n)

    def balance(self, targets_db, L=None):
        """Scale each stem so its RMS (over the first L samples) hits the target dBFS."""
        for k, b in self.stems.items():
            if k not in targets_db:
                continue
            seg = b[:, :L] if L else b
            r = np.sqrt(np.mean(seg ** 2)) + 1e-12
            g = 10 ** (targets_db[k] / 20) / r
            self.stems[k] = b * g
            if os.environ.get("DC_AUDIO_DEBUG"):
                print(f"   stem {k:10s} {20 * np.log10(r):6.1f} dB -> {targets_db[k]} dB")


def make_ir(dur, seed, bright_tau=0.25, dark_tau=None, predelay=0.012, er=True):
    """Stereo synthetic hall/room IR with frequency-dependent decay."""
    n = ns(dur)
    dark_tau = dark_tau or dur / 6.9 * 1.0
    out = []
    for ch in range(2):
        w = white(n, seed + ch)
        lo = lp(w, 2500) * np.exp(-tt(n) / dark_tau)
        hi = hp(w, 2500) * np.exp(-tt(n) / bright_tau) * 0.6
        ir = (lo + hi) * att(n, 0.006)
        ir = np.concatenate([np.zeros(ns(predelay)), ir])
        if er:
            rr = R(seed + 10 + ch)
            for _ in range(10):
                k = ns(rr.uniform(0.004, 0.06))
                ir[k] += rr.uniform(-0.6, 0.6) * 3
        out.append(ir / np.sqrt(np.sum(ir ** 2)))
    m = max(len(o) for o in out)
    return np.vstack([pad_to(o, m) for o in out])


def reverb(x, ir, wet=0.25, dry=1.0):
    """x: stereo (2,N). Returns (2, N+len(ir)-1)."""
    if x.ndim == 1:
        x = stereo(x)
    y = np.vstack([signal.fftconvolve(x[c], ir[c]) for c in range(2)])
    out = y * wet
    out[:, : x.shape[1]] += x * dry
    return out


def wrap_loop(x, L):
    """Fold everything past L back onto the start: perfect loop for periodic material."""
    out = x[..., :L].copy()
    tail = x[..., L:]
    while tail.shape[-1] > 0:
        k = min(L, tail.shape[-1])
        out[..., :k] += tail[..., :k]
        tail = tail[..., k:]
    return out


def xfade_loop(x, L, xf):
    """For non-periodic textures rendered L+xf long: crossfade tail into head."""
    out = x[..., :L].copy()
    tail = x[..., L:L + xf]
    w = np.sin(np.linspace(0, np.pi / 2, xf)) ** 2
    out[..., :xf] = out[..., :xf] * w + tail * (1 - w)
    return out


def loopf(x, fn, pre=0.75):
    """Apply a (causal) filter to a loop without a seam: pre-roll with the loop's own tail."""
    P = min(ns(pre), x.shape[-1])
    y = fn(np.concatenate([x[..., -P:], x], axis=-1))
    return y[..., P:]


def sat(x, drive=1.0):
    return np.tanh(x * drive) / np.tanh(drive)


def normalize(x, peak_db=-1.0):
    p = np.max(np.abs(x)) + 1e-12
    return x * (10 ** (peak_db / 20) / p)


# ───────────────────────────── instruments ─────────────────────────────

def rhodes(f, dur, vel=0.7, seed=0):
    n = ns(dur + 1.5)
    t = tt(n)
    I = (0.5 + 1.8 * vel) * np.exp(-t / 0.22) + 0.2
    mod = np.sin(TAU * f * t)
    car = np.sin(TAU * f * t + I * mod)
    bark = 0.18 * vel * np.sin(TAU * 2 * f * t + 0.6 * mod) * np.exp(-t / 0.35)
    tine = 0.0
    if f * 14 < SR * 0.45:
        tine = 0.22 * vel * np.sin(TAU * f * 14.0 * t) * np.exp(-t / 0.018)
    body = np.exp(-t / (2.4 * (220.0 / f) ** 0.35))
    rel = np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.18))
    y = (car + bark + tine) * att(n, 0.002) * body * rel * vel
    return y


def ep_chord(notes, dur, vel, seed, roll=0.012, tremolo=4.2, pan_depth=0.35, t_offset=0.0):
    """Rhodes chord with gentle roll and stereo auto-pan tremolo; returns stereo."""
    rr = R(seed)
    m = Mix(dur + 1.6)
    for i, nt in enumerate(sorted(notes)):
        v = vel * rr.uniform(0.85, 1.0)
        m.add(rhodes(mhz(nt), dur - i * roll, v), i * roll + rr.uniform(0, 0.006))
    y = m.get()
    n = y.shape[1]
    lfo = np.sin(TAU * tremolo * (tt(n) + t_offset))
    mono = y.sum(0) * 0.5
    return np.vstack([mono * (1 + pan_depth * lfo), mono * (1 - pan_depth * lfo)])


def upright(f, dur, vel=0.8, seed=0):
    n = ns(dur + 0.5)
    t = tt(n)
    finst = f * (1 + 0.012 * np.exp(-t / 0.025))
    ph = TAU * np.cumsum(finst) / SR
    y = np.zeros(n)
    for k in range(1, 16):
        if f * k > 6000:
            break
        amp = abs(np.sin(np.pi * k * 0.2)) / k ** 1.2
        dec = 1.6 / (1 + 0.45 * (k - 1))
        y += amp * np.sin(k * ph * (1 + 0.0003 * k * k)) * np.exp(-t / dec)
    thump = lp(white(n, seed), 250) * np.exp(-t / 0.02) * 0.5
    finger = bp(white(n, seed + 1), 900, 3000) * np.exp(-t / 0.006) * 0.12
    rel = np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.05))
    y = (y + thump + finger) * att(n, 0.0015) * rel * vel
    return lp(y, 2400)


def brush_slap(vel=1.0, seed=0, dur=0.35):
    n = ns(dur)
    w = white(n, seed)
    y = bp(w, 1200, 9000) * np.exp(-tt(n) / 0.09) * att(n, 0.003)
    y += bp(w, 180, 600) * np.exp(-tt(n) / 0.03) * 0.5
    return y * vel


def brush_swirl(n, beat, seed, depth=1.0):
    """Continuous brush sweep in time with the beat (circular stroke per beat)."""
    t = tt(n)
    w = bp(white(n, seed), 1800, 10000)
    ph = (t / beat) % 1.0
    env = 0.35 + 0.65 * np.sin(np.pi * ph) ** 2
    slow = 0.8 + 0.2 * np.sin(TAU * t / (beat * 8))
    return w * env * slow * 0.08 * depth


def ride(vel=0.6, seed=0, dur=1.8):
    n = ns(dur)
    t = tt(n)
    ratios = [1.0, 1.4655, 1.7945, 2.5234, 2.6466, 3.9213]
    y = np.zeros(n)
    for r in ratios:
        y += np.sign(np.sin(TAU * 420 * r * t + R(seed).uniform(0, TAU)))
    y = hp(y, 5200, 2) * np.exp(-t / 0.55)
    ping = modal(n, [2950, 4310, 5680], [0.3, 0.2, 0.1], [0.25, 0.18, 0.12], seed)
    y = y * 0.12 + ping * 0.25
    y += hp(white(n, seed + 3), 7000) * np.exp(-t / 0.01) * 0.15
    return y * vel


def hat(vel=0.5, seed=0, dur=0.08):
    n = ns(dur)
    y = hp(white(n, seed), 7000, 2) * np.exp(-tt(n) / 0.018)
    return y * vel


def soft_kick(vel=0.6, dur=0.35):
    n = ns(dur)
    t = tt(n)
    f = 52 * (1 + 1.2 * np.exp(-t / 0.015))
    y = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / 0.15) * att(n, 0.002)
    return y * vel


def rim(vel=0.6, seed=0):
    n = ns(0.12)
    t = tt(n)
    y = bp(white(n, seed), 1500, 6000) * np.exp(-t / 0.012)
    y += np.sin(TAU * 1750 * t) * np.exp(-t / 0.015) * 0.5
    y += np.sin(TAU * 520 * t) * np.exp(-t / 0.03) * 0.4
    return y * vel


def tom(f, vel=0.8, seed=0, dur=1.0):
    n = ns(dur)
    t = tt(n)
    fi = f * (1 + 0.45 * np.exp(-t / 0.03))
    y = np.sin(TAU * np.cumsum(fi) / SR) * np.exp(-t / (0.35 * 110 / f))
    y += 0.3 * np.sin(TAU * np.cumsum(fi * 1.58) / SR) * np.exp(-t / 0.12)
    y += lp(white(n, seed), 2000) * np.exp(-t / 0.012) * 0.35
    return y * att(n, 0.001) * vel


def timpani(f, vel=1.0, seed=0, dur=2.5):
    n = ns(dur)
    t = tt(n)
    fi = f * (1 + 0.06 * np.exp(-t / 0.08))
    ph = TAU * np.cumsum(fi) / SR
    y = np.zeros(n)
    for r, a, d in [(1, 1, 1.2), (1.5, 0.5, 0.8), (1.98, 0.3, 0.6), (2.44, 0.2, 0.4)]:
        y += a * np.sin(r * ph) * np.exp(-t / d)
    y += lp(white(n, seed), 900) * np.exp(-t / 0.03) * 0.6
    return y * att(n, 0.002) * vel


def brass(f, dur, vel=0.8, seed=0, mute=False, bright=1.0, scoop=40, vib=12, fall=0.0, swell=0.03,
          voices=1, detune=6):
    """Additive brass with brightness envelope. mute=True => Harmon-mute nasal trumpet."""
    n = ns(dur + 0.35)
    t = tt(n)
    rr = R(seed)
    out = np.zeros(n)
    for v in range(voices):
        dc = (v - (voices - 1) / 2) * detune if voices > 1 else 0
        vib_d = np.clip((t - 0.25) / 0.4, 0, 1) * vib
        cents = -scoop * np.exp(-t / 0.035) + vib_d * np.sin(TAU * 5.3 * t + rr.uniform(0, TAU)) + dc
        if fall:
            cents += -fall * np.clip((t - dur * 0.55) / (dur * 0.45), 0, 1) ** 2
        fi = f * 2 ** (cents / 1200)
        ph = TAU * np.cumsum(fi) / SR
        a_env = np.minimum(1, t / swell) ** 1.5
        a_env = a_env * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.07))
        sus = 0.75 + 0.25 * np.exp(-t / 0.15)
        K = (1.5 + 9 * vel * bright) * (0.55 + 0.45 * np.minimum(1, t / (swell * 2)) * sus)
        y = np.zeros(n)
        for k in range(1, 40):
            if f * k > SR * 0.42:
                break
            y += np.sin(k * ph) * np.exp(-k / K) / k ** 0.6
        out += y * a_env * sus
    out /= voices
    if mute:
        out = peak(hp(out, 700, 2), 1700, 1.8, 12) * 0.35
        out += bp(white(n, seed + 7), 1500, 4000) * 0.01 * np.minimum(1, t / 0.02)
        out = lp(out, 5000)
    else:
        out = peak(out, 1100, 1.0, 4)
        out += bp(white(n, seed + 7), 800, 3000) * 0.006
    return out * vel


def vibes(f, dur=2.5, vel=0.7, seed=0, motor=5.5):
    n = ns(dur)
    t = tt(n)
    y = modal(n, [f, f * 3.93, f * 9.87], [1, 0.22, 0.07], [2.2 * (440 / f) ** 0.3, 0.5, 0.12])
    y *= 1 - 0.28 * (0.5 + 0.5 * np.sin(TAU * motor * t))
    y += bp(white(n, seed), 2000, 7000) * np.exp(-t / 0.004) * 0.15
    return y * att(n, 0.001) * vel


def celesta(f, dur=1.8, vel=0.6, seed=0):
    n = ns(dur)
    y = modal(n, [f, 2.0 * f, 3.01 * f, 4.03 * f], [1, 0.15, 0.25, 0.08], [0.9, 0.4, 0.25, 0.15])
    return y * att(n, 0.0007) * vel


def piano(f, dur, vel=0.8, seed=0):
    n = ns(dur + 0.8)
    t = tt(n)
    B = 0.0004
    y = np.zeros(n)
    for k in range(1, 30):
        fk = f * k * np.sqrt(1 + B * k * k)
        if fk > SR * 0.42:
            break
        amp = abs(np.sin(np.pi * k * 0.12)) / k ** (1.0 + 0.4 * (1 - vel))
        dec = 3.5 * (110 / f) ** 0.4 / (1 + 0.25 * (k - 1))
        for det in (-0.6, 0.6):
            y += 0.5 * amp * np.sin(TAU * (fk + det * k * 0.2) * t) * np.exp(-t / dec)
    y += lp(white(n, seed), 3000) * np.exp(-t / 0.006) * 0.08 * vel
    rel = np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.25))
    return y * att(n, 0.0015) * rel * vel


def strings_trem(notes, dur, vel, seed, rate, swell_in=1.0):
    """Tremolo string section from detuned PolyBLEP saws."""
    n = ns(dur + 0.6)
    t = tt(n)
    rr = R(seed)
    out = np.zeros((2, n))
    for i, m in enumerate(notes):
        f = mhz(m)
        for v in range(3):
            det = rr.uniform(-9, 9)
            vib = 6 * np.sin(TAU * rr.uniform(4.5, 5.8) * t + rr.uniform(0, TAU))
            fi = f * 2 ** ((det + vib) / 1200)
            y = saw_blep(fi)
            pan = rr.uniform(-0.7, 0.7)
            out += stereo(y, pan)
    trem = 0.55 + 0.45 * np.abs(np.sin(np.pi * rate * t + 0.2 * np.sin(TAU * 0.7 * t)))
    env = np.minimum(1, t / swell_in) * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.2))
    out = lp(hp(out, 180), 3200) * trem * env * vel / (3 * len(notes)) ** 0.5
    return out


def vinyl(n, seed, amt=1.0):
    rr = R(seed)
    y = lp(hp(white(n, seed), 600), 6000) * 0.006
    k = rr.integers(0, n, int(n / SR * 9))
    imp = np.zeros(n)
    imp[k] = rr.uniform(-1, 1, len(k)) * rr.uniform(0.05, 0.4, len(k)) ** 2
    y += bp(imp, 1500, 9000) * 0.9
    return y * amt


# ───────────────────────────── music ─────────────────────────────

CH = {
    "Cm9": [58, 62, 63, 67], "Abmaj7": [55, 60, 63, 68], "G7b9": [59, 63, 65, 68],
    "Fm9": [56, 60, 63, 67], "Dm7b5": [60, 62, 65, 68], "Ebmaj7": [62, 63, 67, 70],
    "Dbmaj7": [60, 61, 65, 68], "Bb7": [56, 62, 65, 68],
}
BROOT = {"Cm9": 36, "Abmaj7": 44, "G7b9": 43, "Fm9": 41, "Dm7b5": 38, "Ebmaj7": 39,
        "Dbmaj7": 37, "Bb7": 34}
TONES = {  # chord tones above root (semitones) for the walking line
    "Cm9": [0, 3, 7, 10, 14], "Abmaj7": [0, 4, 7, 11], "G7b9": [0, 4, 7, 10, 13],
    "Fm9": [0, 3, 7, 10], "Dm7b5": [0, 3, 6, 10], "Ebmaj7": [0, 4, 7, 11],
    "Dbmaj7": [0, 4, 7, 11], "Bb7": [0, 4, 7, 10],
}

# bars as list of (chord, beats)
PLAN_BARS = [
    [("Cm9", 4)], [("Cm9", 4)], [("Abmaj7", 4)], [("G7b9", 4)],
    [("Cm9", 4)], [("Fm9", 4)], [("Dm7b5", 2), ("G7b9", 2)], [("G7b9", 4)],
    [("Cm9", 4)], [("Ebmaj7", 4)], [("Abmaj7", 4)], [("Dbmaj7", 4)],
    [("Fm9", 4)], [("G7b9", 4)], [("Cm9", 4)], [("Dm7b5", 2), ("G7b9", 2)],
]

S = 2 / 3  # swing offbeat
MEL_A = [  # (beat, dur, midi)
    (3 + S, 1 / 3, 67),
    (4, 1.5, 75), (5 + S, 1 / 3, 74), (6, 1, 72), (7, 1, 70),
    (8, 3, 72), (11 + S, 1 / 3, 67),
    (12, 1, 68), (13, 1, 71), (14, 1, 74), (15, 1, 77),
    (16, 3.5, 75),
    (21 + S, 1 / 3, 68), (22, 1, 72), (23, S, 75), (23 + S, 1 / 3, 74),
    (24, 1.5, 72), (25 + S, 1 / 3, 68), (26, 1.5, 71), (27 + S, 1 / 3, 74),
    (28, S, 75), (28 + S, 1 / 3, 74), (29, 1.5, 71),
]
MEL_B = [
    (32, 1, 72), (33, 1, 75), (34, 2, 79),
    (36, S, 77), (36 + S, 1 / 3, 75), (37, 2, 74), (39 + S, 1 / 3, 70),
    (40, 2, 72), (42, 1, 75), (43, 1, 79),
    (44, 2.5, 77), (46 + S, 1 / 3, 75), (47, 1, 72),
    (48, 1.5, 72), (49 + S, 1 / 3, 68), (50, 2, 67),
    (52, S, 68), (52 + S, 1 / 3, 71), (53, S, 74), (53 + S, 1 / 3, 77), (54, 1.5, 80), (55 + S, 1 / 3, 79),
    (56, 1, 75), (57, 1, 74), (58, 2.5, 72),
]


def chord_timeline(bars):
    out, b = [], 0.0
    for bar in bars:
        for ch, beats in bar:
            out.append((b, beats, ch))
            b += beats
    return out, b


def walking_line(bars, seed):
    rr = R(seed)
    tl, total = chord_timeline(bars)
    notes = []
    for i, (b0, beats, ch) in enumerate(tl):
        nxt = tl[(i + 1) % len(tl)][2]
        r, tones = BROOT[ch], TONES[ch]
        target = BROOT[nxt]
        for k in range(int(beats)):
            if k == 0:
                p = r
            elif k == int(beats) - 1:
                p = target + (1 if rr.random() < 0.5 else -1)
            else:
                p = r + tones[rr.integers(1, len(tones))]
                if p > 48:
                    p -= 12
            notes.append((b0 + k, p))
    return notes, total


def comp_pattern(bars, seed):
    """Swing comping hits: (beat, dur, chord)."""
    rr = R(seed)
    tl, _ = chord_timeline(bars)
    hits = []
    pats4 = [[(0, 1.5), (1 + S, 1.0)], [(0, 0.6), (1 + S, 2.0)], [(0, 3.5)], [(S, 1.2), (2 + S, 1.3)],
             [(0, 1.0), (2, 0.6), (3 + S, 0.33)]]
    pats2 = [[(0, 1.5)], [(0, 0.6), (1 + S, 0.33)]]
    for b0, beats, ch in tl:
        pats = pats4 if beats == 4 else pats2
        for off, d in pats[rr.integers(0, len(pats))]:
            hits.append((b0 + off, d, ch))
    return hits


def render_trumpet_line(m, mel, beat, t0=0.0, gain=0.5, pan=0.15, seed=100, mute=True):
    for i, (b, d, nt) in enumerate(mel):
        dur = max(0.12, d * beat * 0.95)
        legato = i + 1 < len(mel) and abs(mel[i + 1][0] - (b + d)) < 0.01
        y = brass(mhz(nt), dur + (0.03 if legato else 0), vel=0.75, seed=seed + i, mute=mute,
                  scoop=25 if d < 0.5 else 55, vib=14 if d >= 1 else 4)
        m.add(y, t0 + b * beat, gain, pan)


def mus_planning():
    bpm = 80
    beat = 60 / bpm
    bars = PLAN_BARS
    total_beats = 64
    L = ns(total_beats * beat)
    m = Mix(total_beats * beat + 4)
    # bass
    line, _ = walking_line(bars, 11)
    for i, (b, p) in enumerate(line):
        acc = 0.95 if b % 4 == 0 else 0.8
        m.add(upright(mhz(p), beat * 0.92, acc, seed=200 + i), b * beat + 0.005, 0.9, -0.1, stem="bass")
    # rhodes comping
    for i, (b, d, ch) in enumerate(comp_pattern(bars, 21)):
        m.add(ep_chord(CH[ch], d * beat, 0.45, 300 + i, t_offset=b * beat), b * beat, 0.42, stem="keys")
    # drums: ride + brushes + feathered kick
    rr = R(31)
    for bar in range(16):
        b0 = bar * 4
        for off, v in [(0, 0.7), (1, 0.8), (1 + S, 0.45), (2, 0.7), (3, 0.8), (3 + S, 0.45)]:
            m.add(ride(v * rr.uniform(0.85, 1.0), seed=400 + bar * 8 + int(off * 3)), (b0 + off) * beat, 0.32, 0.35, stem="drums")
        for off in (1, 3):
            m.add(brush_slap(rr.uniform(0.8, 1.0), seed=500 + bar * 4 + off), (b0 + off) * beat, 0.32, -0.15, stem="drums")
        if rr.random() < 0.5:
            m.add(brush_slap(0.35, seed=550 + bar), (b0 + 2 + S) * beat, 0.3, -0.15, stem="drums")
        for off in range(4):
            m.add(soft_kick(0.5), (b0 + off) * beat, 0.35, stem="drums")
    sw = brush_swirl(L, beat, 600)
    m.add(stereo(sw, -0.2), 0, 1.0, stem="swirl")
    # muted trumpet melody
    tr = Mix(total_beats * beat + 4)
    render_trumpet_line(tr, MEL_A + MEL_B, beat, seed=700)
    m.add(tr.get(), 0, 0.55, stem="lead")
    ir = make_ir(2.4, 900, bright_tau=0.18, dark_tau=0.45)
    m.balance({"bass": -19, "keys": -19.5, "drums": -25, "swirl": -34, "lead": -19}, L)
    y = reverb(m.get(), ir, wet=0.28)
    y = wrap_loop(y, L)
    y = loopf(y, lambda z: hp(lp(z, 12000), 25))
    y = sat(y / np.max(np.abs(y)) * 1.1, 1.3)
    return normalize(y, -1.0)


def mus_title():
    bpm = 64
    beat = 60 / bpm
    bars = PLAN_BARS[:8] + [[("Cm9", 4)], [("Abmaj7", 4)], [("Fm9", 4)], [("G7b9", 4)],
                            [("Cm9", 4)], [("Abmaj7", 4)], [("Dm7b5", 2), ("G7b9", 2)], [("G7b9", 4)]]
    total_beats = 64
    L = ns(total_beats * beat)
    m = Mix(total_beats * beat + 5)
    tl, _ = chord_timeline(bars)
    rr = R(41)
    for i, (b0, beats, ch) in enumerate(tl):
        # long rolled chords + a second soft re-strike
        m.add(ep_chord(CH[ch], beats * beat * 0.98, 0.42, 1000 + i, roll=0.05, tremolo=3.0, pan_depth=0.5,
                       t_offset=b0 * beat), b0 * beat, 0.5, stem="keys")
        if beats == 4:
            m.add(ep_chord([n + 12 for n in CH[ch][2:]], beat * 1.4, 0.25, 1100 + i, roll=0.03, t_offset=b0),
                  (b0 + 2 + S) * beat, 0.3, stem="keys")
        # bass: half notes
        r = BROOT[ch]
        m.add(upright(mhz(r), beat * 1.9, 0.8, 1200 + i), b0 * beat, 0.85, -0.1, stem="bass")
        if beats == 4:
            m.add(upright(mhz(r + 7 if r + 7 <= 45 else r - 5), beat * 1.9, 0.7, 1300 + i), (b0 + 2) * beat, 0.8, -0.1, stem="bass")
    # vibes counter line in second half
    for i, (b, nt) in enumerate([(32, 79), (33.5, 75), (35, 74), (40, 75), (41.5, 72), (43, 70),
                                 (48, 72), (49.5, 68), (51, 67), (56, 70), (57, 74), (58, 77), (60, 74)]):
        m.add(vibes(mhz(nt), 3.0, 0.5, 1400 + i), b * beat, 0.35, 0.4, stem="vibes")
    # brushes very soft, ride on 2 & 4 only
    m.add(stereo(brush_swirl(L, beat, 1500, 0.7), -0.25), 0, 1.0, stem="swirl")
    for bar in range(16):
        for off in (1, 3):
            m.add(brush_slap(0.6, seed=1600 + bar * 4 + off), (bar * 4 + off) * beat, 0.25, -0.1, stem="drums")
        m.add(ride(0.5, seed=1700 + bar), (bar * 4) * beat, 0.18, 0.35, stem="drums")
    tr = Mix(total_beats * beat + 4)
    render_trumpet_line(tr, MEL_A, beat, seed=1800)
    m.add(tr.get(), 0, 0.6, stem="lead")
    ir = make_ir(3.2, 1900, bright_tau=0.22, dark_tau=0.65)
    m.balance({"keys": -18, "bass": -21, "vibes": -28, "swirl": -36, "drums": -31, "lead": -19.5}, L)
    y = reverb(m.get(), ir, wet=0.4)
    y = wrap_loop(y, L)
    y += stereo(vinyl(L, 1950), 0) * 1.0
    y = loopf(y, lambda z: hp(lp(z, 11000), 25))
    y = sat(y / np.max(np.abs(y)) * 1.1, 1.2)
    return normalize(y, -1.0)


def mus_dailies():
    bpm = 100
    beat = 60 / bpm
    e = beat / 2
    s16 = beat / 4
    total_beats = 64
    L = ns(total_beats * beat)
    m = Mix(total_beats * beat + 4)
    roots = [0, 0, 0, 0, -4, -4, -5, -5, 0, 0, 1, 1, -4, -4, -5, -5]
    pat = [0, 0, 12, 0, 1, 0, 12, -2]
    for bar in range(16):
        r = 36 + roots[bar]
        b0 = bar * 4 * beat
        for k, p in enumerate(pat):
            v = 0.95 if k % 4 == 0 else 0.7
            m.add(upright(mhz(r + p), e * 0.8, v, 2000 + bar * 8 + k), b0 + k * e, 0.7, -0.05, stem="bass")
            m.add(piano(mhz(r + p + 12), e * 0.7, v * 0.6, 2100 + k), b0 + k * e, 0.18, 0.1, stem="bass")
        # toms (16th grid)
        tp = [1, 0, 0, 2, 0, 0, 1, 0, 1, 0, 0, 0, 0, 2, 2, 0]
        for k, v in enumerate(tp):
            if v:
                m.add(tom(95 if v == 1 else 140, 0.8 if v == 1 else 0.55, 2200 + bar * 16 + k),
                      b0 + k * s16, 0.45, -0.3 if v == 1 else 0.3, stem="toms")
        # rims on 2 and 4
        for off in (1, 3):
            m.add(rim(0.7, 2300 + bar * 4 + off), b0 + off * beat, 0.3, 0.1, stem="drums")
        # hats: 8ths first half, 16ths second half
        step = e if bar < 8 else s16
        cnt = 8 if bar < 8 else 16
        for k in range(cnt):
            acc = 1.0 if (bar < 8 or k % 2 == 0) else 0.55
            m.add(hat(0.5 * acc, 2400 + bar * 16 + k), b0 + k * step, 0.35, 0.45, stem="drums")
        # brass stabs in the second half
        if bar >= 8 and bar % 2 == 0:
            for j, nt in enumerate([48 + roots[bar], 55 + roots[bar], 63 + roots[bar], 71 + roots[bar]]):
                m.add(brass(mhz(nt), 0.28, 0.9, 2500 + bar * 4 + j, bright=1.2, voices=2, swell=0.01, scoop=0),
                      b0, 0.22, -0.4 + j * 0.27, stem="brass")
    # tremolo strings
    for seg in range(8):
        r = 48 + roots[seg * 2]
        notes = [r, r + 7, r + 15, r + 14] if seg < 4 else [r, r + 7, r + 15, r + 14, r + 24, r + 26]
        vel = 0.55 if seg < 4 else 0.55 + 0.1 * (seg - 4)
        m.add(strings_trem(notes, 8 * beat, vel, 2600 + seg, rate=4 / beat, swell_in=0.3), seg * 8 * beat, 0.5, stem="strings")
    # downbeat boom
    m.add(timpani(mhz(36), 1.0, 2700), 0, 0.6, stem="boom")
    m.add(timpani(mhz(36), 0.8, 2701), 32 * beat, 0.5, stem="boom")
    # ticking clock (projector-ish) on quarters, very soft
    for k in range(total_beats):
        m.add(rim(0.25, 2800 + k), k * beat + s16 * 2, 0.12, -0.5, stem="drums")
    ir = make_ir(1.8, 2900, bright_tau=0.12, dark_tau=0.35)
    m.balance({"bass": -18, "toms": -23, "drums": -25, "brass": -27, "strings": -21, "boom": -31}, L)
    y = reverb(m.get(), ir, wet=0.22)
    y = wrap_loop(y, L)
    y = loopf(y, lambda z: hp(lp(z, 13000), 25))
    y = sat(y / np.max(np.abs(y)) * 1.15, 1.4)
    return normalize(y, -1.0)


def mus_win():
    bpm = 132
    beat = 60 / bpm
    m = Mix(12)
    # snare roll crescendo, beats 0-4
    k = 0
    tpos = 0.0
    while tpos < 4 * beat:
        v = 0.25 + 0.75 * (tpos / (4 * beat)) ** 1.5
        m.add(brush_slap(v, 3000 + k, 0.2) * 1.5, tpos, 0.4, -0.1)
        tpos += beat / (4 if tpos < 2 * beat else 6)
        k += 1
    sw = hp(white(ns(4 * beat), 3050), 4000) * np.linspace(0, 1, ns(4 * beat)) ** 3 * 0.25
    m.add(stereo(sw), 0, 1.0)
    # bass walk up
    for i, (b, p) in enumerate([(0, 34), (1, 36), (2, 38), (3, 39), (4, 41), (5.5, 46)]):
        m.add(upright(mhz(p), beat * 0.9, 0.9, 3100 + i), b * beat, 0.9)
    # brass hits: Fm9 (beat 4), Bb13 (beat 5.5)
    for j, nt in enumerate([56, 60, 63, 67, 72]):
        m.add(brass(mhz(nt), beat * 0.9, 0.9, 3200 + j, voices=2, swell=0.015), 4 * beat, 0.3, -0.5 + j * 0.25)
    for j, nt in enumerate([56, 62, 67, 70, 74]):
        m.add(brass(mhz(nt), beat * 1.6, 0.95, 3300 + j, voices=2, swell=0.015), 5.5 * beat, 0.3, -0.5 + j * 0.25)
    # trumpet pickup
    for i, (b, d, nt) in enumerate([(4, 0.5, 67), (4.5, 0.5, 70), (5, 0.5, 72), (5.5, 0.5, 74), (6, 0.5, 75),
                                    (6.5, 0.5, 77)]):
        m.add(brass(mhz(nt), d * beat, 0.85, 3400 + i, scoop=20, vib=0), b * beat, 0.35, 0.2)
    # final Ebmaj9 held
    tf = 8 * beat
    for j, nt in enumerate([51, 58, 62, 65, 67, 70]):
        m.add(brass(mhz(nt), 3.6, 0.85, 3500 + j, voices=3, swell=0.05, vib=6), tf, 0.26, -0.6 + j * 0.24)
    m.add(brass(mhz(82), 3.8, 0.85, 3550, vib=18, scoop=60), tf, 0.32, 0.15)
    m.add(ep_chord([63, 67, 70, 74, 77], 4.0, 0.6, 3600, roll=0.04), tf, 0.4)
    for i, nt in enumerate([63, 67, 70, 74, 75, 79, 82, 86, 87]):
        m.add(vibes(mhz(nt), 3.0, 0.4, 3700 + i), tf + i * 0.045, 0.25, -0.5 + i * 0.12)
    m.add(upright(mhz(27 + 12), 3.5, 1.0, 3800), tf, 1.0)
    m.add(piano(mhz(27), 3.5, 0.9, 3801), tf, 0.4)
    m.add(timpani(mhz(39), 1.0, 3802, 3.0), tf, 0.6)
    m.add(ride(1.0, 3803, 4.0), tf, 0.5, 0.3)
    m.add(ride(0.9, 3804, 4.0), tf + 0.01, 0.4, -0.3)
    y = reverb(fades(m.get(ns(tf + 4.5)), 0, 0.8), make_ir(2.6, 3900, 0.2, 0.5), wet=0.3)
    y = fades(y[:, :ns(tf + 6.0)], 0.001, 1.2)
    y = sat(y / np.max(np.abs(y)) * 1.1, 1.3)
    return normalize(y, -1.0)


def mus_gameover():
    beat = 60 / 56
    m = Mix(20)
    prog = [("Cm9", 0), ("Abmaj7", 4), ("Fm9", 8), ("G7b9", 12)]
    for i, (ch, b) in enumerate(prog):
        m.add(ep_chord(CH[ch], 4 * beat * 0.98, 0.45, 4000 + i, roll=0.06, tremolo=2.5, pan_depth=0.5), b * beat, 0.5)
        m.add(upright(mhz(BROOT[ch] - (12 if BROOT[ch] > 40 else 0)), 4 * beat * 0.9, 0.8, 4100 + i), b * beat, 0.8)
    mel = [(1, 1.5, 67), (2.5, 0.5, 63), (3, 1, 62), (4, 2, 60), (6, 1, 63), (7, 1, 62),
           (8, 1.5, 60), (9.5, 0.5, 56), (10, 2, 55), (12, 1, 59), (13, 1, 62), (14, 2, 59)]
    tr = Mix(20)
    render_trumpet_line(tr, mel, beat, seed=4200)
    m.add(tr.get(), 0, 0.6)
    tf = 16 * beat
    m.add(ep_chord([48, 55, 62, 63], 5.0, 0.5, 4300, roll=0.12, tremolo=2.0), tf, 0.5)
    m.add(piano(mhz(24), 5.0, 0.9, 4301), tf, 0.5)
    m.add(piano(mhz(36), 5.0, 0.7, 4302), tf, 0.3)
    m.add(brass(mhz(60), 4.0, 0.6, 4303, mute=True, vib=18, fall=120), tf + 0.2, 0.6, 0.15)
    m.add(timpani(mhz(36), 0.7, 4304, 4), tf, 0.4)
    # film running off the reel: flapping, slowing clicks
    tpos, rate = tf + 2.0, 24.0
    k = 0
    while rate > 3:
        m.add(rim(0.35, 4400 + k) * 0.8, tpos, 0.25, -0.3)
        tpos += 1 / rate
        rate *= 0.93
        k += 1
    y = reverb(fades(m.get(ns(tf + 6)), 0, 1.0), make_ir(3.0, 4500, 0.2, 0.6), wet=0.4)
    y = fades(y[:, :ns(tf + 8)], 0.001, 1.5)
    y += stereo(vinyl(y.shape[1], 4600), 0) * 0.8
    y = sat(y / np.max(np.abs(y)) * 1.1, 1.2)
    return normalize(y, -1.0)


# ───────────────────────────── stingers ─────────────────────────────

def _finish_sting(m, n, wet=0.3, ir_dur=2.2, seed=5000):
    y = reverb(fades(m.get(n), 0, 0.4), make_ir(ir_dur, seed, 0.2, 0.45), wet=wet)
    y = fades(y, 0.001, 0.4)
    y = sat(y / np.max(np.abs(y)) * 1.1, 1.3)
    return normalize(y, -1.5)


def sting_wrap():
    m = Mix(4)
    beat = 60 / 150
    for i, (b, nt) in enumerate([(0, 70), (0.5, 72)]):
        m.add(brass(mhz(nt), beat * 0.45, 0.9, 5100 + i, scoop=15, vib=0), b * beat, 0.35, 0.2)
    t = beat
    for j, nt in enumerate([51, 58, 62, 65, 67, 70, 74]):
        m.add(brass(mhz(nt), 1.2, 0.95, 5200 + j, voices=2, swell=0.012, vib=8), t, 0.24, -0.6 + j * 0.2)
    m.add(brass(mhz(79), 1.3, 0.9, 5250, vib=16, scoop=30), t, 0.3, 0.2)
    m.add(upright(mhz(39), 1.2, 1.0, 5300), t, 0.9)
    m.add(ride(1.0, 5301, 2.5), t, 0.45, 0.3)
    m.add(soft_kick(1.0), t, 0.7)
    m.add(brush_slap(1.0, 5302), t, 0.5)
    return _finish_sting(m, ns(2.6), seed=5400)


def sting_cut():
    m = Mix(4)
    for j, nt in enumerate([48, 54, 57, 63, 66]):
        m.add(brass(mhz(nt), 1.4, 0.95, 5500 + j, voices=2, swell=0.01, fall=350, vib=0, scoop=0), 0, 0.26,
              -0.5 + j * 0.25)
    m.add(piano(mhz(24), 2.5, 1.0, 5600), 0, 0.5)
    m.add(piano(mhz(25), 2.5, 0.9, 5601), 0, 0.35)
    m.add(piano(mhz(30), 2.5, 0.8, 5602), 0, 0.3)
    m.add(timpani(mhz(31), 1.0, 5603, 2.5), 0, 0.7)
    m.add(rim(1.0, 5604), 0, 0.4)
    return _finish_sting(m, ns(2.8), wet=0.35, seed=5700)


def sting_levelup():
    m = Mix(3)
    for i, nt in enumerate([63, 67, 70, 74, 77, 82]):
        m.add(vibes(mhz(nt), 2.0, 0.6, 5800 + i), i * 0.07, 0.4, -0.5 + i * 0.2)
    t = 6 * 0.07
    for j, nt in enumerate([58, 62, 65, 70]):
        m.add(brass(mhz(nt), 0.7, 0.85, 5900 + j, voices=2, swell=0.02), t, 0.22, -0.4 + j * 0.27)
    m.add(upright(mhz(39), 0.8, 0.9, 5950), t, 0.7)
    m.add(ride(0.7, 5951, 1.5), t, 0.3, 0.3)
    return _finish_sting(m, ns(2.0), seed=6000)


def sting_room():
    m = Mix(5)
    # brass swell into big chord
    for j, nt in enumerate([44, 51, 55, 60, 62, 67]):
        m.add(brass(mhz(nt), 1.1, 0.7, 6100 + j, voices=3, swell=0.9, vib=0, scoop=0), 0, 0.2, -0.6 + j * 0.24)
    cym = hp(white(ns(1.1), 6150), 3500) * np.linspace(0, 1, ns(1.1)) ** 2.5 * 0.4
    m.add(stereo(cym), 0, 1.0)
    t = 1.1
    for j, nt in enumerate([51, 58, 62, 65, 67, 70, 74]):
        m.add(brass(mhz(nt), 1.6, 0.9, 6200 + j, voices=3, swell=0.02, vib=8), t, 0.22, -0.6 + j * 0.2)
    m.add(ep_chord([63, 67, 70, 74], 2.0, 0.6, 6300, roll=0.03), t, 0.4)
    for i, nt in enumerate([75, 79, 82, 86, 87, 91]):
        m.add(vibes(mhz(nt), 2.2, 0.35, 6400 + i), t + 0.1 + i * 0.06, 0.25, -0.5 + i * 0.2)
    m.add(upright(mhz(39), 1.8, 1.0, 6500), t, 0.9)
    m.add(timpani(mhz(39), 1.0, 6501), t, 0.6)
    m.add(ride(1.0, 6502, 2.5), t, 0.45, -0.3)
    return _finish_sting(m, ns(3.4), wet=0.35, seed=6600)


def sting_perk():
    m = Mix(3)
    seq = [80, 75, 72, 67, 74, 79, 84, 86]
    for i, nt in enumerate(seq):
        m.add(celesta(mhz(nt), 1.6, 0.55, 6700 + i), i * 0.085, 0.35, np.sin(i) * 0.6)
    m.add(strings_trem([56, 63, 67, 74], 1.6, 0.35, 6800, rate=12, swell_in=0.5), 0, 0.5)
    m.add(vibes(mhz(68), 2.0, 0.5, 6900), 0.68, 0.3)
    return _finish_sting(m, ns(2.4), wet=0.45, ir_dur=2.8, seed=7000)


# ───────────────────────────── UI SFX (mono) ─────────────────────────────

def _room(y, wet=0.08, dur=0.35, seed=7100):
    ir = make_ir(dur, seed, 0.05, 0.08, predelay=0.004)
    s = reverb(stereo(y), ir, wet=wet)
    return s.mean(0)


def sfx_click(var=0):
    n = ns(0.09)
    t = tt(n)
    y = bp(white(n, 7200 + var), 2000, 8000) * np.exp(-t / 0.004)
    y += modal(n, [1850 + 120 * var, 3400, 5200], [0.5, 0.3, 0.15], [0.012, 0.008, 0.005], 7210 + var)
    y += np.sin(TAU * 180 * t) * np.exp(-t / 0.012) * 0.4
    return fades(_room(y, 0.06), 0.0005, 0.01)


def sfx_hover():
    n = ns(0.05)
    t = tt(n)
    y = modal(n, [3100, 4700], [0.4, 0.2], [0.006, 0.004])
    y += hp(white(n, 7300), 6000) * np.exp(-t / 0.002) * 0.3
    return fades(y, 0.0005, 0.005)


def paper_slide(dur, seed, up=True, end_thump=False):
    n = ns(dur)
    t = tt(n)
    x = t / dur
    env = np.sin(np.pi * x) ** 1.2 if up else (np.sin(np.pi * x ** 0.7)) ** 1.5
    w = white(n, seed)
    # time-varying brightness via crossfade of two bands
    lo_b = bp(w, 600, 2500)
    hi_b = bp(w, 2500, 9000)
    mixk = x if up else 1 - x
    y = (lo_b * (1 - mixk) + hi_b * mixk * 0.8) * env
    # paper fibre grit
    grit = np.zeros(n)
    rr = R(seed + 1)
    idx = rr.integers(0, n, int(dur * 300))
    grit[idx] = rr.uniform(-1, 1, len(idx))
    y += bp(grit, 2000, 8000) * env * 0.6
    if end_thump:
        k = ns(dur * 0.85)
        th = lp(white(n - k, seed + 2), 400) * np.exp(-tt(n - k) / 0.02) * 1.2
        y[k:] += th
    return fades(_room(y * 0.6, 0.06), 0.002, 0.02)


def sfx_typewriter(var=0):
    n = ns(0.22)
    t = tt(n)
    rr = R(7400 + var)
    y = hp(white(n, 7400 + var), 2500) * np.exp(-t / 0.0025) * 1.2
    y += modal(n, np.array([2300, 3650, 5100, 6900]) * rr.uniform(0.93, 1.07),
               [0.6, 0.4, 0.25, 0.15], [0.03, 0.02, 0.015, 0.01], 7410 + var)
    # typebar hitting platen: low body knock
    y += np.sin(TAU * 140 * t) * np.exp(-t / 0.025) * 0.6
    y += lp(white(n, 7420 + var), 800) * np.exp(-t / 0.01) * 0.6
    # key return rattle ~40 ms later
    k = ns(0.04 + 0.01 * var)
    y[k:] += hp(white(n - k, 7430 + var), 3000) * np.exp(-tt(n - k) / 0.004) * 0.3
    return fades(_room(y, 0.1), 0.0003, 0.02)


def sfx_stamp():
    n = ns(0.5)
    t = tt(n)
    f = 95 * (1 + 0.8 * np.exp(-t / 0.012))
    y = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / 0.08) * 1.0
    y += lp(white(n, 7500), 1200) * np.exp(-t / 0.018) * 0.9
    y += modal(n, [420, 690, 1150], [0.3, 0.2, 0.1], [0.06, 0.04, 0.03], 7501)  # desk wood
    # rubber peel-off at the end
    k = ns(0.22)
    peel = bp(white(n - k, 7502), 1500, 5000) * np.sin(np.pi * np.clip(tt(n - k) / 0.08, 0, 1)) * 0.12
    y[k:] += peel
    return fades(_room(y, 0.12, 0.5), 0.0003, 0.03)


def sfx_cash():
    n = ns(1.5)
    t = tt(n)
    y = np.zeros(n)
    # key press clunk
    y += lp(white(n, 7600), 1500) * np.exp(-t / 0.015) * 0.8
    y += modal(n, [320, 610], [0.4, 0.3], [0.05, 0.03])
    # bell ding at 0.06
    k = ns(0.06)
    bell = modal(n - k, [2093, 2093 * 2.76, 2093 * 5.4, 2093 * 1.002], [1.0, 0.35, 0.12, 0.6],
                 [0.9, 0.35, 0.12, 0.9])
    y[k:] += bell * 0.7
    # drawer sliding out with coin rattle
    k2 = ns(0.12)
    m = n - k2
    dr = bp(white(m, 7601), 300, 2500) * np.sin(np.pi * np.clip(tt(m) / 0.3, 0, 1)) * 0.25
    y[k2:] += dr
    k3 = ns(0.42)
    y[k3:] += lp(white(n - k3, 7602), 600) * np.exp(-tt(n - k3) / 0.03) * 0.9  # drawer stop
    rr = R(7603)
    for i in range(7):  # coins
        kk = k3 + ns(rr.uniform(0.0, 0.12))
        coin = modal(ns(0.15), np.array([4200, 6100, 8300]) * rr.uniform(0.9, 1.1), [0.2, 0.15, 0.1],
                     [0.05, 0.03, 0.02], 7604 + i)
        y[kk:kk + len(coin)] += coin * rr.uniform(0.4, 1.0)
    return fades(_room(y * 0.8, 0.1), 0.0003, 0.1)


def sfx_counter_tick():
    n = ns(0.04)
    t = tt(n)
    y = bp(white(n, 7700), 3000, 9000) * np.exp(-t / 0.0015)
    y += modal(n, [2600, 4100], [0.3, 0.15], [0.006, 0.004])
    return fades(y, 0.0002, 0.005)


def sfx_clapper():
    n = ns(0.55)
    t = tt(n)
    # tiny pre-swish of the stick closing
    pre = ns(0.06)
    y = np.zeros(n)
    y[:pre] = bp(white(pre, 7800), 1500, 6000) * np.linspace(0, 1, pre) ** 3 * 0.15
    m = n - pre
    tm = tt(m)
    crack = hp(white(m, 7801), 1200) * np.exp(-tm / 0.003) * 1.4
    wood = modal(m, [780, 1290, 2150, 3320, 4400], [0.7, 0.5, 0.35, 0.25, 0.15],
                 [0.03, 0.022, 0.016, 0.01, 0.008], 7802)
    body = np.sin(TAU * 210 * tm) * np.exp(-tm / 0.02) * 0.4
    y[pre:] += crack + wood + body
    return fades(_room(y, 0.18, 0.6, 7803), 0.0003, 0.05)


def sfx_diceroll():
    """Film-sprocket ratchet spinning up and coasting down, with a soft landing click."""
    dur = 0.95
    n = ns(dur)
    y = np.zeros(n)
    rr = R(7900)
    tpos = 0.0
    k = 0
    while tpos < dur - 0.12:
        x = tpos / (dur - 0.12)
        rate = 18 + 42 * np.sin(np.pi * min(1, x * 1.15)) ** 0.7
        click = modal(ns(0.03), np.array([2900, 4300, 6100]) * rr.uniform(0.92, 1.08), [0.4, 0.25, 0.12],
                      [0.006, 0.004, 0.003], 7901 + k)
        click += hp(white(ns(0.03), 8000 + k), 3000) * np.exp(-tt(ns(0.03)) / 0.0015) * 0.4
        i = ns(tpos)
        amp = 0.4 + 0.6 * np.sin(np.pi * x) ** 0.5
        y[i:i + len(click)] += click[: n - i] * amp * rr.uniform(0.7, 1.0)
        tpos += 1 / rate
        k += 1
    # reel whirr underneath
    t = tt(n)
    wh = bp(white(n, 8100), 700, 2200) * np.sin(np.pi * t / dur) ** 2 * 0.12
    y += wh
    kk = ns(dur - 0.1)
    land = modal(n - kk, [1600, 2700], [0.5, 0.3], [0.03, 0.02]) + lp(white(n - kk, 8101), 900) * np.exp(
        -tt(n - kk) / 0.01) * 0.6
    y[kk:] += land
    return fades(_room(y, 0.08), 0.001, 0.02)


def sfx_commit():
    """Typewriter carriage return: ratchet zip + bell ding + carriage thunk."""
    n = ns(1.2)
    y = np.zeros(n)
    rr = R(8200)
    zip_len = 0.32
    tpos, k = 0.0, 0
    while tpos < zip_len:
        c = hp(white(ns(0.02), 8201 + k), 2500) * np.exp(-tt(ns(0.02)) / 0.0015) * 0.5
        c += modal(ns(0.02), [3300 * rr.uniform(0.95, 1.05)], [0.2], [0.004])
        i = ns(tpos)
        y[i:i + len(c)] += c * (0.5 + 0.5 * tpos / zip_len)
        tpos += 1 / (55 + 40 * tpos / zip_len)
        k += 1
    kb = ns(0.05)
    bell = modal(n - kb, [2637, 2637 * 2.71, 2637 * 5.2, 2637 * 1.003], [1, 0.3, 0.08, 0.5], [0.9, 0.3, 0.1, 0.9])
    y[kb:] += bell * 0.55
    kt = ns(zip_len)
    th = lp(white(n - kt, 8300), 900) * np.exp(-tt(n - kt) / 0.02) * 0.9
    th += modal(n - kt, [260, 520, 1300], [0.5, 0.3, 0.2], [0.05, 0.03, 0.02])
    y[kt:] += th
    return fades(_room(y * 0.8, 0.12), 0.0005, 0.1)


def sfx_error():
    """Dull double 'nope' knock plus a low muted-horn blat."""
    n = ns(0.45)
    t = tt(n)
    y = np.zeros(n)
    for j, (t0, f) in enumerate([(0.0, 155), (0.13, 146)]):
        i = ns(t0)
        m = n - i
        tm = tt(m)
        horn = brass(f, 0.1, 0.8, 8400 + j, mute=True, scoop=0, vib=0, swell=0.005)[:m]
        knock = np.sin(TAU * 180 * tm) * np.exp(-tm / 0.03) + lp(white(m, 8410 + j), 900) * np.exp(-tm / 0.008)
        y[i:i + len(horn)] += horn * 1.5
        y[i:] += knock * 0.5
    return fades(_room(y, 0.08), 0.0005, 0.03)


def sfx_garage_door():
    """Roll-up metal shutter: rattling slats, rumble, and a clang at the top."""
    dur = 2.6
    n = ns(dur)
    t = tt(n)
    rr = R(8500)
    y = np.zeros(n)
    rate = 22.0
    tpos, k = 0.05, 0
    while tpos < 2.15:
        f0 = rr.uniform(700, 1300)
        sl = modal(ns(0.08), [f0, f0 * 2.31, f0 * 3.7], [0.3, 0.2, 0.1], [0.03, 0.02, 0.01], 8501 + k)
        sl += bp(white(ns(0.08), 8600 + k), 1500, 6000) * np.exp(-tt(ns(0.08)) / 0.006) * 0.3
        i = ns(tpos)
        y[i:i + len(sl)] += sl * rr.uniform(0.4, 1.0)
        tpos += 1 / rate * rr.uniform(0.7, 1.3)
        k += 1
    env = np.clip(t / 0.15, 0, 1) * np.clip((2.2 - t) / 0.1, 0, 1)
    rumble = lp(white(n, 8700), 180) * 3.0 + bp(white(n, 8701), 200, 800) * 0.4
    y += rumble * env * 0.6
    motor = np.sin(TAU * 98 * t) * 0.08 + np.sin(TAU * 196 * t) * 0.04
    y += motor * env
    i = ns(2.18)
    m = n - i
    clang = modal(m, [310, 587, 911, 1430, 2210], [0.8, 0.5, 0.4, 0.25, 0.15], [0.35, 0.25, 0.2, 0.12, 0.08], 8702)
    clang += lp(white(m, 8703), 1500) * np.exp(-tt(m) / 0.02)
    y[i:] += clang * 0.9
    return fades(_room(y * 0.6, 0.15, 0.7, 8704), 0.005, 0.1)


def sfx_car_depart():
    """1970s V8 rumble revving and pulling away (Doppler-less fade with LP)."""
    dur = 3.8
    n = ns(dur)
    t = tt(n)
    rpm = 750 + 2400 * np.clip((t - 0.3) / 1.0, 0, 1) ** 0.7 - 800 * np.clip((t - 1.5) / 0.25, 0, 1) \
        + 900 * np.clip((t - 1.75) / 1.5, 0, 1)
    fire = rpm / 60 * 4  # V8 firing frequency
    rr = R(8800)
    ph = np.cumsum(fire) / SR
    # pulse train via sharpened sine + per-cylinder jitter
    base = np.sin(TAU * ph)
    pulses = np.maximum(0, base) ** 6
    jitter = 1 + 0.25 * lp(white(n, 8801), 30)
    eng = pulses * jitter
    eng = lp(eng, 900) * 2.5 + bp(eng, 900, 3000) * 0.4
    sub = np.sin(TAU * ph / 2) * 0.35
    exhaust = bp(white(n, 8802), 80, 600) * (0.3 + 0.5 * np.clip((t - 0.3) / 1, 0, 1))
    y = eng + sub + exhaust
    # distance: amp fade and darkening after 1.8 s
    dist = np.clip(1 - (t - 1.8) / 2.0, 0.08, 1) ** 1.5
    dark = lp(y, 500)
    w = np.clip((t - 1.8) / 1.6, 0, 1)
    y = (y * (1 - w) + dark * w) * dist
    # door clunk + tyre chirp
    y[:ns(0.12)] += lp(white(ns(0.12), 8803), 700) * np.exp(-tt(ns(0.12)) / 0.02) * 1.2
    k = ns(1.75)
    chirp = bp(white(ns(0.25), 8804), 1800, 3500) * np.sin(np.pi * tt(ns(0.25)) / 0.25) * 0.25
    y[k:k + len(chirp)] += chirp
    y = sat(hp(y, 30) * 0.8, 1.5)
    return fades(_room(y * 0.6, 0.1, 0.6, 8805), 0.002, 0.3)


# ───────────────────────────── ambience ─────────────────────────────

def amb_rain():
    L = ns(32.0)
    XF = ns(3.0)
    n = L + XF
    out = np.zeros((2, n))
    t = tt(n)
    for ch in range(2):
        bed = bp(pink(n, 9000 + ch, 0.35), 350, 9000)
        swell = 1 + 0.15 * np.sin(TAU * t / 11.0 + ch) + 0.08 * np.sin(TAU * t / 4.3)
        out[ch] += bed * 0.05 * swell
        low = lp(pink(n, 9010 + ch, 0.9), 220) * 0.06
        out[ch] += low
        # fine droplets
        rr = R(9020 + ch)
        cnt = int(n / SR * 900)
        imp = np.zeros(n)
        idx = rr.integers(0, n, cnt)
        imp[idx] = rr.uniform(-1, 1, cnt) * rr.uniform(0.1, 1, cnt) ** 3
        out[ch] += bp(imp, 2500, 10000) * 0.6
        # window taps: resonant plinks
        cnt2 = int(n / SR * 14)
        for i in range(cnt2):
            k = rr.integers(0, n - ns(0.05))
            f = rr.uniform(1800, 4200)
            d = modal(ns(0.05), [f, f * 1.7], [1, 0.4], [0.006, 0.003]) * rr.uniform(0.02, 0.07)
            out[ch, k:k + len(d)] += d
    # gutter drip (shared, slightly right)
    rr = R(9040)
    tpos = 0.3
    while tpos < n / SR - 0.1:
        k = ns(tpos)
        m = ns(0.06)
        tm = tt(m)
        f = 700 * (1 + 0.8 * tm / 0.06)
        plop = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-tm / 0.012) * 0.05 * rr.uniform(0.6, 1)
        s = stereo(plop, 0.55)
        out[:, k:k + m] += s[:, : n - k]
        tpos += rr.uniform(1.1, 1.9)
    y = xfade_loop(out, L, XF)
    y = loopf(y, lambda z: hp(lp(z, 9000), 25))
    return normalize(y, -3.0)


def amb_projector():
    L = ns(10.0)  # 240 frames at 24 fps: periodic
    out = np.zeros((2, L + SR))
    t = tt(L)
    rr = R(9100)
    frames = 240
    for fr in range(frames):
        k = int(fr * SR / 24)
        cl = hp(white(ns(0.02), 9200 + fr), 1800) * np.exp(-tt(ns(0.02)) / 0.0018)
        cl += modal(ns(0.02), [1450, 2900], [0.4, 0.2], [0.004, 0.003])
        a = (0.9 if fr % 2 == 0 else 0.65) * rr.uniform(0.85, 1.0)
        s = stereo(cl * a, 0.1)
        e = k + s.shape[1]
        out[:, k:e] += s[:, : e - k]
        # claw pull-down: softer second click half a frame later
        k2 = k + int(SR / 48)
        e2 = k2 + s.shape[1]
        out[:, k2:e2] += s[:, : e2 - k2] * 0.35
    out = wrap_loop(out, L) * 0.35
    # motor hum (periodic in 10 s: 120 Hz, 240 Hz, 360) + gear whine 960 Hz
    hum = 0.05 * np.sin(TAU * 120 * t) + 0.03 * np.sin(TAU * 240 * t) + 0.012 * np.sin(TAU * 360 * t) \
        + 0.006 * np.sin(TAU * 960 * t + 0.4 * np.sin(TAU * 0.3 * t))
    # fan noise modulated by the 48 Hz shutter (2 blades x 24 fps); make noise periodic by tiling
    seg = ns(2.5)
    fan = bp(white(seg + ns(0.5), 9300), 300, 3000)
    fan = xfade_loop(fan, seg, ns(0.5))
    fan = np.tile(fan, 4)[:L]
    fan *= (1 + 0.35 * np.sin(TAU * 48 * t)) * 0.05
    out += stereo(hum + fan, 0.0) * 1.0
    out[1] = 0.85 * out[1] + 0.15 * np.roll(out[1], 37)
    y = loopf(out, lambda z: lp(z, 7000))
    return normalize(y, -3.0)


def amb_neon():
    L = ns(12.0)  # integer multiple of 1/60 s
    XF = ns(1.0)
    n = L + XF
    t = tt(n)
    base = np.sin(TAU * 60 * t)
    buzz = np.tanh(6 * np.abs(base) - 3)  # rectified mains → 120 Hz harmonics
    buzz = bp(buzz, 110, 5000) * 0.25 + np.sin(TAU * 120 * t) * 0.15
    buzz += bp(white(n, 9400), 2000, 7000) * 0.008  # gas hiss
    rr = R(9401)
    flick = np.ones(n)
    crack = np.zeros(n)
    for _ in range(4):
        c = rr.uniform(0.5, n / SR - 1.5)
        d = rr.uniform(0.1, 0.45)
        k, m = ns(c), ns(d)
        stutter = (rr.random(m // 400 + 1) > 0.5).repeat(400)[:m]
        flick[k:k + m] *= np.where(stutter, 1.0, 0.2)
        cr = bp(white(m, 9402 + k), 1500, 8000) * np.exp(-tt(m) / 0.03) * 0.3 * stutter
        crack[k:k + m] += cr
    y = buzz * lp(flick, 200) + crack
    y = stereo(y, 0.3)
    y = xfade_loop(y, L, XF)
    return normalize(y, -3.0)


def amb_siren():
    dur = 11.0
    n = ns(dur)
    t = tt(n)
    cyc = 3.6
    ph = (t % cyc) / cyc
    f = 650 + 750 * np.sin(np.pi * ph) ** 1.6
    f *= 1 + 0.01 * np.sin(TAU * 0.13 * t)
    p = np.cumsum(f) / SR
    tone = np.tanh(2.5 * np.sin(TAU * p)) + 0.3 * np.sin(TAU * 2 * p)
    env = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 1.4
    y = lp(tone, 1400, 2) * env
    y = hp(y, 350)
    pan = -0.7 + 1.2 * t / dur
    s = np.vstack([y * np.cos((pan + 1) * np.pi / 4), y * np.sin((pan + 1) * np.pi / 4)])
    s = reverb(s, make_ir(3.5, 9500, 0.3, 0.9, predelay=0.06), wet=1.2, dry=0.35)[:, :n]
    s = lp(s, 2500)
    return normalize(fades(s, 0.3, 1.0), -3.0)


# ───────────────────────────── registry ─────────────────────────────

# name: (fn, category, loop, channels)
SOUNDS = {
    "SFX_Click": (lambda: sfx_click(0), "SFX", False),
    "SFX_Click_2": (lambda: sfx_click(1), "SFX", False),
    "SFX_Click_3": (lambda: sfx_click(2), "SFX", False),
    "SFX_Hover": (sfx_hover, "SFX", False),
    "SFX_PanelOpen": (lambda: paper_slide(0.38, 9600, True), "SFX", False),
    "SFX_PanelClose": (lambda: paper_slide(0.3, 9610, False, True), "SFX", False),
    "SFX_Typewriter": (lambda: sfx_typewriter(0), "SFX", False),
    "SFX_Typewriter_2": (lambda: sfx_typewriter(1), "SFX", False),
    "SFX_Typewriter_3": (lambda: sfx_typewriter(2), "SFX", False),
    "SFX_Stamp": (sfx_stamp, "SFX", False),
    "SFX_Cash": (sfx_cash, "SFX", False),
    "SFX_CounterTick": (sfx_counter_tick, "SFX", False),
    "SFX_Clapper": (sfx_clapper, "SFX", False),
    "SFX_DiceRoll": (sfx_diceroll, "SFX", False),
    "SFX_Commit": (sfx_commit, "SFX", False),
    "SFX_Error": (sfx_error, "SFX", False),
    "SFX_GarageDoor": (sfx_garage_door, "SFX", False),
    "SFX_CarDepart": (sfx_car_depart, "SFX", False),
    "SFX_StingWrap": (sting_wrap, "Stinger", False),
    "SFX_StingCut": (sting_cut, "Stinger", False),
    "SFX_StingLevelUp": (sting_levelup, "Stinger", False),
    "SFX_StingRoom": (sting_room, "Stinger", False),
    "SFX_StingPerk": (sting_perk, "Stinger", False),
    "MUS_Title": (mus_title, "Music", True),
    "MUS_Planning": (mus_planning, "Music", True),
    "MUS_Dailies": (mus_dailies, "Music", True),
    "MUS_Win": (mus_win, "Music", False),
    "MUS_GameOver": (mus_gameover, "Music", False),
    "AMB_Rain": (amb_rain, "Ambience", True),
    "AMB_Projector": (amb_projector, "Ambience", True),
    "AMB_Neon": (amb_neon, "Ambience", True),
    "AMB_Siren": (amb_siren, "Ambience", False),
}

SFX_PEAK_DB = {"SFX_Hover": -12, "SFX_CounterTick": -10, "SFX_Click": -6, "SFX_Click_2": -6, "SFX_Click_3": -6,
               "SFX_PanelOpen": -8, "SFX_PanelClose": -8}


def write_wav(path, x):
    x = np.asarray(x)
    if x.ndim == 1:
        x = x[None, :]
    pcm = np.clip(np.round(x.T * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[0])
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def trim_tail(x, thresh_db=-62.0):
    a = np.abs(np.atleast_2d(x)).max(0)
    idx = np.nonzero(a > 10 ** (thresh_db / 20) * a.max())[0]
    end = min(a.shape[0], (idx[-1] if len(idx) else a.shape[0]) + ns(0.02))
    return fades(x[..., :end], 0.0, min(0.05, end / SR / 4))


def main(argv):
    os.makedirs(OUT, exist_ok=True)
    names = argv or list(SOUNDS)
    for name in names:
        fn, cat, loop = SOUNDS[name]
        y = fn()
        if cat == "SFX":
            y = normalize(y, SFX_PEAK_DB.get(name, -3.0))
        if cat in ("SFX", "Stinger"):
            y = trim_tail(y)
        write_wav(os.path.join(OUT, name + ".wav"), y)
        ch = 1 if np.asarray(y).ndim == 1 else 2
        print(f"{name:20s} {cat:9s} ch={ch} {np.asarray(y).shape[-1] / SR:6.2f}s loop={loop}")
    manifest = []
    for name, (_, cat, loop) in SOUNDS.items():
        manifest.append({"file": name + ".wav", "asset": f"/Game/DC/Audio/{name}.{name}", "category": cat,
                         "loop": loop})
    with open(os.path.join(OUT, "manifest.json"), "w") as f:
        json.dump({"sampleRate": SR, "generator": "Tools/audio_gen.py", "sounds": manifest}, f, indent=2)
    write_licenses()


DESC = {
    "SFX_Click": "UI click (switch latch), 3 variants", "SFX_Hover": "UI hover tick",
    "SFX_PanelOpen": "paper slide in", "SFX_PanelClose": "paper slide out + soft thump",
    "SFX_Typewriter": "typewriter key clack, 3 variants", "SFX_Stamp": "rubber stamp thunk",
    "SFX_Cash": "cash register: key, bell, drawer, coins", "SFX_CounterTick": "mechanical counter tick",
    "SFX_Clapper": "clapperboard snap", "SFX_DiceRoll": "film-sprocket ratchet roll",
    "SFX_Commit": "typewriter carriage return + bell", "SFX_Error": "muted-horn double 'nope'",
    "SFX_GarageDoor": "roll-up shutter door", "SFX_CarDepart": "V8 car pulling away",
    "SFX_StingWrap": "stinger: success (brass Ebmaj9 hit)", "SFX_StingCut": "stinger: fail (falling brass cluster)",
    "SFX_StingLevelUp": "stinger: level up (vibes + brass)", "SFX_StingRoom": "stinger: room built (brass swell)",
    "SFX_StingPerk": "stinger: perk (celesta shimmer)",
    "MUS_Title": "title loop: slow noir jazz, Rhodes + muted trumpet + vinyl",
    "MUS_Planning": "planning loop: swing noir jazz, walking bass, brushes/ride, Rhodes, muted trumpet",
    "MUS_Dailies": "dailies loop: tense ostinato, toms, tremolo strings, brass stabs",
    "MUS_Win": "win cue: drum roll into big-band Ebmaj9", "MUS_GameOver": "game-over cue: sad trumpet, film runs off reel",
    "AMB_Rain": "rain on window loop", "AMB_Projector": "projector whirr loop (24 fps claw)",
    "AMB_Neon": "neon sign buzz loop", "AMB_Siren": "distant police siren one-shot",
}


def write_licenses():
    lines = [
        "# Director's Cut: audio sources and licences",
        "",
        "Every audio file in this folder is **procedurally generated by the project** with",
        "`Tools/audio_gen.py` (numpy/scipy synthesis: additive/FM instruments, filtered noise, modal",
        "resonators, synthetic reverb). No samples, recordings, sound packs, or third-party music are used,",
        "and nothing is AI-generated by an external service. The material is original to the project",
        "(author: the Director's Cut team, generated 2026-11-13), so no attribution is required.",
        "",
        "Regenerate with: `python3 Tools/audio_gen.py` (needs `pip install numpy scipy`).",
        "",
        "| File | Category | Loop | Content | Source | Licence |",
        "|---|---|---|---|---|---|",
    ]
    for name, (_, cat, loop) in SOUNDS.items():
        base = name.rsplit("_", 1)[0] if name[-2:] in ("_2", "_3") else name
        d = DESC.get(base, "")
        if base != name:
            d += " (variant)"
        lines.append(f"| {name}.wav | {cat} | {'yes' if loop else 'no'} | {d} | procedurally generated by the project "
                     f"(Tools/audio_gen.py) | project-owned original |")
    with open(os.path.join(OUT, "LICENSES.md"), "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
