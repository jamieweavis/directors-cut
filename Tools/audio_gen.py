#!/usr/bin/env python3
"""Director's Cut: procedural audio generator (v2: realistic, modern, understated; no music).

Every sound in Content/RawAudio is synthesised from scratch by this script (no samples, no
recordings, no third-party material), so it is owned by the project. Requires numpy + scipy:

    python3 -m venv /tmp/dcvenv && /tmp/dcvenv/bin/pip install numpy scipy
    /tmp/dcvenv/bin/python Tools/audio_gen.py                       # all
    /tmp/dcvenv/bin/python Tools/audio_gen.py SFX_Click AMB_CityDay  # some

Output: 16-bit PCM WAV, 44.1 kHz (mono UI sfx, stereo events/ambience), loudness-normalised
per sound (see LOUD below), plus Content/RawAudio/manifest.json and LICENSES.md.
Everything is deterministic (fixed seeds). Stale WAVs not in the registry are deleted.
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



# ───────────────────────────── building blocks ─────────────────────────────

def put(buf, sig, t0, gain=1.0):
    """Add a mono (or stereo into stereo) signal into buf at t0 seconds, clipped to buf length."""
    i = ns(t0)
    if i >= buf.shape[-1]:
        return buf
    m = min(sig.shape[-1], buf.shape[-1] - i)
    seg = sig[..., :m] * gain
    f = min(ns(0.004), m // 2)
    if f > 0:  # inserted pieces always end with a short fade so truncated tails never click
        seg = seg.copy()
        seg[..., m - f:] *= np.cos(np.linspace(0, np.pi / 2, f)) ** 2
    buf[..., i:i + m] += seg
    return buf


def sweep_noise(n, f0, f1, seed, width_oct=0.6, curve=1.0):
    """Noise whose spectral centre glides f0 -> f1 (log), built from a bank of fixed bands."""
    w = white(n, seed)
    x = (tt(n) / max(tt(n)[-1], 1e-6)) ** curve
    lc = np.log2(f0) + (np.log2(f1) - np.log2(f0)) * x
    edges = np.geomspace(150, 14000, 15)
    y = np.zeros(n)
    for lo, hi in zip(edges[:-1], edges[1:]):
        c = np.log2(np.sqrt(lo * hi))
        wgt = np.exp(-0.5 * ((lc - c) / width_oct) ** 2)
        y += bp(w, lo, hi, 2) * wgt
    return y


def room(y, wet=0.08, dur=0.35, seed=7100, bright=0.05, dark=0.08, pre=0.004):
    """Small dry room (mono in -> mono out)."""
    ir = make_ir(dur, seed, bright, dark, predelay=pre)
    s = reverb(stereo(y), ir, wet=wet)
    return s.mean(0)


def warehouse(y, wet=0.22, dur=1.4, seed=7150):
    """Concrete warehouse reverb; mono or stereo in -> stereo out."""
    ir = make_ir(dur, seed, 0.12, 0.32, predelay=0.012)
    return reverb(y if y.ndim == 2 else stereo(y), ir, wet=wet)


def outdoors(y, wet=0.9, dur=3.0, seed=7180, dry=0.35):
    """Distant, outdoor slap-back between buildings; stereo out."""
    ir = make_ir(dur, seed, 0.18, 0.7, predelay=0.05)
    return reverb(y if y.ndim == 2 else stereo(y), ir, wet=wet, dry=dry)


def tick(n, f=2600, seed=0, amp=1.0, tau=0.0008, body=0.004):
    """A crisp, dry plastic/glass tick: noise transient + a damped resonance."""
    t = tt(n)
    y = hp(white(n, seed), 3000) * np.exp(-t / tau) * 0.7
    y += modal(n, [f, f * 1.93, f * 3.1], [0.6, 0.25, 0.1], [body, body * 0.6, body * 0.4], seed + 1)
    return y * amp


def thump(n, f=70, tau=0.08, seed=0, drop=0.6, noise=0.6):
    """Soft low impact with a short pitch drop."""
    t = tt(n)
    fr = f * (1 + drop * np.exp(-t / 0.012))
    y = np.sin(TAU * np.cumsum(fr) / SR) * np.exp(-t / tau)
    y += lp(white(n, seed), 500) * np.exp(-t / (tau * 0.25)) * noise
    return y * att(n, 0.0015)


def tone(n, f, tau, partials=((1, 1.0),), a=0.003, seed=None):
    """Soft sine-ish tone with optional (ratio, amp) partials, exp decay."""
    t = tt(n)
    y = np.zeros(n)
    for r, amp in partials:
        if f * r < SR * 0.45:
            y += amp * np.sin(TAU * f * r * t) * np.exp(-t / (tau / max(r, 1) ** 0.5))
    return y * att(n, a)


def engine(rpm, cyl=6, seed=0, load=None, bright=1.0):
    """Combustion engine from an rpm curve: firing pulses, crank sub-harmonic, intake/exhaust noise."""
    n = len(rpm)
    load = np.ones(n) * 0.5 if load is None else load
    fire = np.maximum(rpm, 1) / 60.0 * cyl / 2.0
    ph = np.cumsum(fire) / SR
    rr = R(seed)
    jit = 1 + 0.18 * lp(white(n, seed + 1), 40)
    pulses = np.maximum(0, np.sin(TAU * ph)) ** 5 * jit
    sub = np.sin(TAU * ph / cyl * 2) * 0.25  # per-revolution imbalance
    body = lp(pulses, 700 * bright) * 2.2 + bp(pulses, 700, 2600) * (0.25 + 0.45 * load) * bright
    exhaust = bp(white(n, seed + 2), 70, 900) * (0.15 + 0.5 * load) * (0.6 + 0.4 * pulses)
    whine = 0.02 * np.sin(TAU * np.cumsum(rpm / 60.0 * 9.0) / SR) * load  # belt/alternator
    on = (rpm > 60).astype(float)
    return (body + sub + exhaust + whine) * lp(on, 30) * rr.uniform(0.95, 1.0)


def doppler_pass(sig, dur, t_mid, speed=14.0, closest=18.0, pan_span=0.85):
    """Pass a mono source by the listener: time-varying delay (Doppler), 1/r gain, air LP, pan sweep."""
    n = len(sig)
    t = tt(n)
    x = (t - t_mid) * speed
    r = np.sqrt(x ** 2 + closest ** 2)
    delay = r / 343.0
    src_t = t - (delay - delay.min())
    idx = np.clip(src_t * SR, 0, n - 1)
    i0 = np.floor(idx).astype(int)
    fr = idx - i0
    i1 = np.minimum(i0 + 1, n - 1)
    y = sig[i0] * (1 - fr) + sig[i1] * fr
    g = closest / r
    near = lp(y, 6000)
    far = lp(y, 1200)
    y = near * g + far * (1 - g)
    y *= g
    pan = np.clip(x / (abs(x).max() + 1e-9), -1, 1) * pan_span
    a = (pan + 1) * np.pi / 4
    return np.vstack([y * np.cos(a), y * np.sin(a)])


def radio_band(y, drive=2.0):
    """Handheld radio speaker: 350-3200 Hz band, light saturation."""
    return sat(bp(y, 350, 3200, 3) * 1.2, drive)


# ───────────────────────────── loudness ─────────────────────────────

def k_weight(x):
    y = hp(x, 60, 2)
    return y + 0.58 * hp(y, 2000, 1)


def loudness_db(x):
    """Approximate gated short-term loudness (dB): K-weighted 50 ms block power, gated at -20 dB below the
    loudest block, so short sfx and long beds are judged on their 'active' part."""
    x = np.atleast_2d(x)
    k = np.vstack([k_weight(c) for c in x])
    B = ns(0.05)
    nb = max(1, k.shape[1] // B)
    p = np.array([np.sum(np.mean(k[:, i * B:(i + 1) * B] ** 2, axis=1)) for i in range(nb)]) + 1e-20
    g = p[p > p.max() * 0.01]
    return 10 * np.log10(np.mean(g))


def loud_norm(x, target_db, ceiling_db=-1.0):
    x = np.asarray(x, dtype=float)
    x = x * 10 ** ((target_db - loudness_db(x)) / 20)
    c = 10 ** (ceiling_db / 20)
    p = np.max(np.abs(x))
    if p > 2 * c:
        x *= 2 * c / p
        p = 2 * c
    if p > 0.9 * c:
        x = c * np.tanh(x / c)  # soft-knee limit of the few transient peaks
    return x


# ───────────────────────────── UI (mono, quiet) ─────────────────────────────

def sfx_click(var=0):
    """Soft, crisp switch click: noise transient, tiny resonant body, faint low tock."""
    n = ns(0.07)
    t = tt(n)
    f = [2350, 2600, 2150][var]
    y = tick(n, f, 7200 + var * 7, 1.0, 0.0007, 0.0045)
    y += np.sin(TAU * [540, 580, 500][var] * t) * np.exp(-t / 0.006) * 0.45 * att(n, 0.0004)
    k = ns(0.009 + 0.002 * var)  # second, softer contact (release of the switch)
    y[k:] += tick(n - k, f * 1.2, 7230 + var, 0.25, 0.0005, 0.002)
    return fades(room(y, 0.04, 0.2, 7240), 0.0003, 0.01)


def sfx_hover(var=0):
    """Barely-there high tick."""
    n = ns(0.035)
    y = tick(n, [4200, 4600][var], 7300 + var, 1.0, 0.0004, 0.0022)
    return fades(lp(y, 9000), 0.0002, 0.006)


def sfx_panel(opening=True):
    """Short airy whoosh: band of noise gliding up (open) or down (close); open ends with a faint tick."""
    dur = 0.24 if opening else 0.2
    n = ns(dur + 0.05)
    t = tt(n)
    f0, f1 = (700, 4200) if opening else (3600, 650)
    y = sweep_noise(n, f0, f1, 7400 if opening else 7410, 0.55, 0.8)
    x = np.clip(t / dur, 0, 1)
    env = (np.sin(np.pi * x ** (0.65 if opening else 0.45))) ** 1.6
    y *= env
    if opening:
        y = put(y, tick(ns(0.03), 3000, 7420, 0.25, 0.0005, 0.002), dur * 0.82)
    else:
        y = put(y, thump(ns(0.06), 180, 0.012, 7421, 0.3, 0.3) * 0.15, dur * 0.8)
    return fades(room(y, 0.06, 0.25, 7430), 0.002, 0.02)


def sfx_confirm():
    """Positive two-step blip (rising fifth), glassy and short, over a soft click."""
    n = ns(0.32)
    y = tick(n, 2600, 7500, 0.35, 0.0006, 0.003)
    p = ((1, 1.0), (2.0, 0.12), (3.0, 0.04))
    y = put(y, tone(ns(0.25), 880, 0.05, p), 0.004)
    y = put(y, tone(ns(0.3), 1320, 0.09, p), 0.068)
    return fades(room(y, 0.08, 0.3, 7510), 0.0003, 0.03)


def sfx_error():
    """Low, muted double 'bonk' (slightly falling), soft and short."""
    n = ns(0.36)
    y = np.zeros(n)
    for j, (t0, f) in enumerate([(0.0, 200), (0.11, 178)]):
        m = ns(0.2)
        tm = tt(m)
        b = (np.sin(TAU * f * tm) + 0.35 * np.sin(TAU * 2 * f * tm) + 0.12 * np.sin(TAU * 3 * f * tm))
        b *= np.exp(-tm / 0.045) * att(m, 0.003)
        b += lp(white(m, 7600 + j), 900) * np.exp(-tm / 0.006) * 0.25
        y = put(y, lp(b, 1500), t0)
    return fades(room(y, 0.06, 0.25, 7610), 0.0005, 0.03)


def sfx_slotfill():
    """Magnetic snap: a card clicking into a holder (click + small damped body thunk)."""
    n = ns(0.16)
    y = tick(n, 1900, 7700, 0.9, 0.0006, 0.006)
    y += thump(n, 260, 0.018, 7701, 0.5, 0.4) * 0.5
    y = put(y, tick(ns(0.03), 3400, 7702, 0.2, 0.0004, 0.002), 0.012)
    return fades(room(y, 0.07, 0.25, 7710), 0.0003, 0.02)


# ───────────────────────────── money ─────────────────────────────

def bill_riffle(dur, seed, rate=30.0):
    """Electric bill counter: motor whirr + a fast stream of paper flicks."""
    n = ns(dur + 0.15)
    t = tt(n)
    rr = R(seed)
    y = np.zeros(n)
    tp = 0.03
    i = 0
    while tp < dur:
        m = ns(0.02)
        tm = tt(m)
        flick = bp(white(m, seed + 10 + i), 1800, 8000) * np.exp(-tm / 0.0025)
        flick += bp(white(m, seed + 500 + i), 350, 1400) * np.exp(-tm / 0.005) * 0.5
        y = put(y, flick, tp, rr.uniform(0.55, 1.0))
        ramp = min(1.0, tp / 0.12)
        tp += 1.0 / (rate * (0.5 + 0.5 * ramp)) * rr.uniform(0.85, 1.15)
        i += 1
    env = np.clip(t / 0.05, 0, 1) * np.clip((dur + 0.06 - t) / 0.08, 0, 1)
    motor = (0.12 * np.sin(TAU * 118 * t) + 0.05 * np.sin(TAU * 236 * t)) + bp(white(n, seed + 1), 180, 900) * 0.25
    y += motor * env * 0.5
    y = put(y, thump(ns(0.08), 140, 0.02, seed + 2, 0.4, 0.5) * 0.25, dur + 0.02)
    return y


def sfx_cash():
    """Money in: a short bill-counter riffle (~0.6 s)."""
    y = bill_riffle(0.6, 7800)
    return fades(room(y, 0.07, 0.3, 7810), 0.002, 0.04)


def register_drawer(seed):
    """POS cash drawer: solenoid clack, roller slide, stop thump, coin settle."""
    n = ns(0.9)
    rr = R(seed)
    y = tick(n, 1500, seed, 1.0, 0.0008, 0.01) + thump(n, 120, 0.03, seed + 1, 0.5, 0.6) * 0.5
    m = ns(0.22)
    tm = tt(m)
    slide = (bp(white(m, seed + 2), 250, 1600) + 0.4 * lp(white(m, seed + 3), 150)) * np.sin(np.pi * tm / 0.22) * 0.3
    y = put(y, slide, 0.02)
    y = put(y, thump(ns(0.2), 95, 0.04, seed + 4, 0.4, 0.8) * 0.7, 0.24)
    y = put(y, modal(ns(0.1), [880, 1720], [0.3, 0.2], [0.03, 0.02], seed + 5), 0.24)
    for i in range(6):
        c = modal(ns(0.12), np.array([4300, 6200, 8100]) * rr.uniform(0.9, 1.1), [0.2, 0.12, 0.07],
                  [0.04, 0.025, 0.015], seed + 10 + i)
        y = put(y, c, 0.25 + rr.uniform(0, 0.1), rr.uniform(0.15, 0.4))
    return y


def sfx_payday():
    """Dawn payday: bill counter riffle, register beep, drawer pops out."""
    n = ns(1.9)
    y = np.zeros(n)
    y = put(y, bill_riffle(0.55, 7900, 34), 0.0)
    beep = np.sign(np.sin(TAU * 2730 * tt(ns(0.075)))) * 0.12
    beep = lp(beep, 5000) * att(ns(0.075), 0.002) * np.clip((0.075 - tt(ns(0.075))) / 0.01, 0, 1)
    y = put(y, beep, 0.72)
    y = put(y, register_drawer(7910), 0.86)
    return fades(warehouse(y, 0.12, 0.9, 7920), 0.002, 0.08)


# ───────────────────────────── mission flow ─────────────────────────────

def radio_chirp(seed):
    """Handheld radio key-up: squelch burst, two-tone talk-permit chirp, static tail."""
    n = ns(0.5)
    t = tt(n)
    rr = R(seed)
    y = np.zeros(n)
    static = white(n, seed) * 0.35
    crack = np.zeros(n)
    idx = rr.integers(0, n, 120)
    crack[idx] = rr.uniform(-1, 1, 120)
    static += crack * 0.8
    env = np.where(t < 0.04, att(n, 0.002), 1.0) * np.exp(-np.maximum(0, t - 0.03) / 0.07)
    y += static * env
    k = ns(0.035)
    for j, (f, d) in enumerate([(1180, 0.05), (1560, 0.06)]):
        m = ns(d)
        sq = np.tanh(3 * np.sin(TAU * f * tt(m))) * 0.5 * np.clip((d - tt(m)) / 0.006, 0, 1) * att(m, 0.002)
        y = put(y, sq, k / SR + j * d)
    tail = white(ns(0.14), seed + 1) * 0.3 * np.exp(-tt(ns(0.14)) / 0.03)
    y = put(y, tail, 0.18)
    return radio_band(y)


def sfx_mission_start():
    """Radio chirp ('go'), then a car starter cranks and the engine catches to idle."""
    dur = 2.7
    n = ns(dur)
    t = tt(n)
    out = np.zeros((2, n))
    out = put(out, stereo(radio_chirp(8000), -0.25), 0.0, 0.55)
    # starter: whine + compression-modulated load at ~4.5 Hz for 0.55 s
    t0 = 0.6
    m = ns(0.6)
    tm = tt(m)
    comp = 0.55 + 0.45 * np.sin(TAU * 4.6 * tm) ** 2
    whine = saw_blep(np.full(m, 190.0) * (0.92 + 0.08 * comp))
    starter = (lp(whine, 1200) * 0.35 + bp(white(m, 8010), 300, 2000) * 0.25) * comp
    starter *= att(m, 0.01) * np.clip((0.6 - tm) / 0.05, 0, 1)
    starter = put(starter, tick(ns(0.03), 1400, 8011, 1.2, 0.001, 0.008), 0)  # solenoid
    # engine catches at t0+0.5: flare to ~1600 rpm then settle to idle 800
    te = t - (t0 + 0.5)
    rpm = np.where(te < 0, 0.0, 800 + 1000 * np.clip(te / 0.12, 0, 1) * np.exp(-np.maximum(te - 0.12, 0) / 0.35))
    rpm = lp(rpm, 25)
    load = np.where(te < 0.4, 0.8, 0.3)
    eng = engine(rpm, 6, 8020, lp(load, 5)) * 0.6
    eng *= np.clip((dur - t) / 0.6, 0, 1)
    mono = put(np.zeros(n), starter, t0) + eng
    out += stereo(sat(hp(mono, 35), 1.3), 0.1)
    return fades(warehouse(out, 0.18, 1.2, 8030), 0.001, 0.2)


def sfx_mission_complete():
    """Phone notification on a desk: brief haptic buzz, then a soft two-note glassy ping."""
    n = ns(0.9)
    y = np.zeros(n)
    m = ns(0.13)
    tm = tt(m)
    buzz = (np.sin(TAU * 165 * tm) + 0.4 * np.sign(np.sin(TAU * 165 * tm)) * 0.3) * np.sin(np.pi * tm / 0.13) ** 0.5
    buzz += bp(white(m, 8100), 600, 2500) * 0.15 * np.sin(np.pi * tm / 0.13)  # desk rattle
    y = put(y, lp(buzz, 2500) * 0.18, 0.0)
    p = ((1, 1.0), (2.0, 0.18), (4.2, 0.05))
    y = put(y, tone(ns(0.5), 1318.5, 0.16, p, 0.002), 0.03)
    y = put(y, tone(ns(0.7), 1975.5, 0.22, p, 0.002), 0.13)
    return fades(room(y, 0.12, 0.45, 8110), 0.0005, 0.06)


def sfx_success():
    """Positive, non-musical sting: an airy rising whoosh into a soft low hit with a bright shimmer."""
    dur = 1.5
    n = ns(dur)
    out = np.zeros((2, n))
    rise = ns(0.42)
    w = sweep_noise(rise, 500, 6000, 8200, 0.5, 1.4) * (tt(rise) / 0.42) ** 2.2
    out[0, :rise] += w * 0.6
    out[1, :rise] += sweep_noise(rise, 520, 6200, 8201, 0.5, 1.4) * (tt(rise) / 0.42) ** 2.2 * 0.6
    h = 0.42
    out = put(out, stereo(thump(ns(0.6), 62, 0.16, 8202, 0.7, 0.4) * 0.9), h)
    rr = R(8203)
    m = ns(1.0)
    for k, f in enumerate([2140, 2890, 3570, 4410, 5230, 6170]):
        s = tone(m, f * rr.uniform(0.99, 1.01), rr.uniform(0.25, 0.45), ((1, 1.0),), 0.002)
        out = put(out, stereo(s * 0.12, rr.uniform(-0.6, 0.6)), h + 0.005 * k)
    out = put(out, stereo(hp(white(ns(0.5), 8204), 6000) * np.exp(-tt(ns(0.5)) / 0.12) * 0.12), h)
    out = put(out, stereo(tick(ns(0.03), 3200, 8205, 0.5)), h)
    return fades(warehouse(out, 0.22, 1.6, 8210), 0.002, 0.15)


def sfx_fail():
    """Heavy car/steel door slam: low thud, panel ring and latch click, in the warehouse."""
    n = ns(0.9)
    t = tt(n)
    y = thump(n, 68, 0.11, 8300, 0.8, 0.9)
    y += modal(n, [96, 151, 243, 397, 610], [0.5, 0.4, 0.3, 0.15, 0.08], [0.12, 0.09, 0.06, 0.04, 0.03], 8301)
    y += lp(white(n, 8302), 1400) * np.exp(-t / 0.012) * 0.8
    y = put(y, tick(ns(0.05), 2700, 8303, 0.6, 0.0008, 0.006), 0.014)
    y = sat(y * 0.8, 1.4)
    return fades(warehouse(y, 0.28, 1.4, 8310), 0.0005, 0.2)


def siren_whoop(freqs_t, seed):
    """Electronic siren tone following a frequency curve (array)."""
    p = np.cumsum(freqs_t) / SR
    s = np.tanh(2.2 * np.sin(TAU * p)) + 0.25 * np.sin(TAU * 2 * p)
    return lp(s, 2200)


def sfx_busted():
    """Two distant police 'whoop' yelps bouncing off buildings."""
    dur = 2.6
    n = ns(dur)
    t = tt(n)
    f = np.full(n, 1.0)
    env = np.zeros(n)
    for t0 in (0.05, 0.75):
        x = t - t0
        seg = (x >= 0) & (x < 0.62)
        xs = np.clip(x / 0.62, 0, 1)
        fc = 380 + 1050 * np.sin(np.pi * np.clip(xs / 0.55, 0, 1) / 2) ** 1.3 - 350 * np.clip((xs - 0.55) / 0.45, 0, 1)
        f = np.where(seg, fc, f)
        env = np.where(seg, np.sin(np.pi * np.clip(xs / 0.08, 0, 1) / 2) * np.clip((1 - xs) / 0.25, 0, 1), env)
    f = np.where(f > 1, f, 400)
    y = hp(siren_whoop(f, 8400) * lp(env, 60), 300)
    s = stereo(y, 0.35)
    s = outdoors(s, 0.55, 2.0, 8410, 0.6)[:, :n + ns(1.0)]
    return fades(lp(s, 2600), 0.01, 0.4)


def sfx_levelup():
    """Bright, subtle chime: three quick glassy notes rising, with an air shimmer."""
    n = ns(1.6)
    out = np.zeros((2, n))
    bell = ((1, 1.0), (2.76, 0.18), (5.4, 0.05))
    for k, (f, pan) in enumerate([(1567.98, -0.25), (2093.0, 0.0), (2637.0, 0.25)]):
        out = put(out, stereo(tone(ns(1.2), f, 0.35, bell, 0.002) * 0.45, pan), 0.07 * k)
    air = hp(white(ns(0.8), 8500), 6500) * np.sin(np.pi * tt(ns(0.8)) / 0.8) ** 2 * 0.05
    out = put(out, np.vstack([air, hp(white(ns(0.8), 8501), 6500) * np.sin(np.pi * tt(ns(0.8)) / 0.8) ** 2 * 0.05]), 0.0)
    return fades(warehouse(out, 0.25, 1.4, 8510), 0.001, 0.15)


def drill(dur, seed, top=260.0, dips=()):
    """Cordless drill/driver burst: trigger click, motor + gear whine, load dips, spin-down."""
    n = ns(dur + 0.35)
    t = tt(n)
    up = np.clip(t / 0.12, 0, 1) ** 0.6
    down = np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.09))
    f = top * up * down
    for c, d in dips:
        f *= 1 - d * np.exp(-0.5 * ((t - c) / 0.05) ** 2)
    f = np.maximum(f, 1.0)
    ph = np.cumsum(f) / SR
    motor = saw_blep(np.maximum(f, 1.0)) * 0.25 + np.sin(TAU * ph) * 0.3
    gear = np.sin(TAU * ph * 7.0) * 0.12 + np.sin(TAU * ph * 13.0) * 0.05
    brush = bp(white(n, seed), 2500, 8000) * (f / top) * 0.12
    y = (lp(motor, 2500) + gear + brush) * (f / top) ** 0.8
    y = put(y, tick(ns(0.03), 1600, seed + 1, 0.25, 0.001, 0.006), 0.0)
    return y


def sfx_build_start():
    """Power tools: two cordless-driver bursts (the second sinks a screw: rpm dips under load)."""
    n = ns(1.9)
    y = np.zeros(n)
    y = put(y, drill(0.45, 8600, 270), 0.0)
    y = put(y, drill(0.55, 8610, 255, dips=((0.2, 0.25), (0.42, 0.35))), 0.85)
    y = put(y, thump(ns(0.1), 220, 0.02, 8620, 0.3, 0.4) * 0.2, 1.4)  # bit seats
    return fades(warehouse(stereo(y, -0.1), 0.2, 1.2, 8630), 0.001, 0.15)


def fluorescent_hum(n, seed):
    base = np.sin(TAU * 60 * tt(n))
    buzz = np.tanh(5 * np.abs(base) - 2.5)
    return bp(buzz, 110, 3000) * 0.2 + np.sin(TAU * 120 * tt(n)) * 0.25 + bp(white(n, seed), 2000, 6000) * 0.01


def sfx_room_built():
    """Breaker thrown (heavy clunk), fluorescent starters tick and flicker, tubes hum on."""
    dur = 3.0
    n = ns(dur)
    t = tt(n)
    y = tick(n, 1100, 8700, 1.2, 0.001, 0.02)
    y += modal(n, [180, 420, 1130, 2350], [0.6, 0.4, 0.25, 0.12], [0.05, 0.035, 0.02, 0.012], 8701)
    y += thump(n, 90, 0.05, 8702, 0.4, 0.6) * 0.6
    for k, t0 in enumerate([0.32, 0.48, 0.56]):
        y = put(y, modal(ns(0.04), [3200 + 300 * k, 5100], [0.15, 0.06], [0.012, 0.006], 8710 + k), t0)
    gate = np.zeros(n)
    for a, b in [(0.36, 0.40), (0.50, 0.53), (0.58, dur)]:
        gate[(t >= a) & (t < b)] = 1.0
    gate = lp(gate, 80) * np.clip((dur - t) / 1.0, 0, 1) * (0.7 + 0.3 * np.clip((t - 0.58) / 0.6, 0, 1))
    y += fluorescent_hum(n, 8720) * gate * 0.55
    return fades(warehouse(stereo(y, 0.05), 0.25, 1.4, 8730), 0.0005, 0.3)


def sfx_perk():
    """Metal lighter: lid flicked open (bright clink), flint wheel, soft flame bloom."""
    n = ns(1.0)
    t = tt(n)
    y = modal(n, [2870, 4130, 6280, 8020], [0.5, 0.35, 0.2, 0.1], [0.05, 0.035, 0.02, 0.012], 8800)
    y += hp(white(n, 8801), 3000) * np.exp(-t / 0.0012) * 0.7
    m = ns(0.07)
    wheel = bp(white(m, 8802), 2000, 9000) * np.sin(np.pi * tt(m) / 0.07) * 0.5
    rr = R(8803)
    cr = np.zeros(m)
    idx = rr.integers(0, m, 40)
    cr[idx] = rr.uniform(-1, 1, 40)
    wheel += hp(cr, 3000) * 0.8
    y = put(y, wheel, 0.3)
    fm = ns(0.5)
    flame = lp(white(fm, 8804), 600) * np.sin(np.pi * np.clip(tt(fm) / 0.5, 0, 1)) ** 2 * att(fm, 0.02) * 0.9
    flame += bp(white(fm, 8805), 800, 3000) * np.exp(-tt(fm) / 0.05) * 0.15
    y = put(y, flame, 0.34)
    return fades(room(y, 0.1, 0.4, 8810), 0.0003, 0.08)


def sfx_garage_door():
    """Motorised roll-up steel door: chain-drive motor, slat rattle, rumble, stop clunk at the top."""
    dur = 3.0
    n = ns(dur)
    t = tt(n)
    rr = R(8900)
    y = np.zeros(n)
    tpos, k = 0.15, 0
    while tpos < 2.45:
        f0 = rr.uniform(650, 1200)
        m = ns(0.07)
        sl = modal(m, [f0, f0 * 2.31, f0 * 3.7], [0.3, 0.2, 0.1], [0.025, 0.018, 0.01], 8901 + k)
        sl += bp(white(m, 9000 + k), 1500, 6000) * np.exp(-tt(m) / 0.005) * 0.3
        y = put(y, sl, tpos, rr.uniform(0.3, 0.9))
        tpos += 1 / 18.0 * rr.uniform(0.7, 1.3)
        k += 1
    env = np.clip((t - 0.1) / 0.2, 0, 1) * np.clip((2.5 - t) / 0.12, 0, 1)
    rumble = lp(white(n, 8902), 160) * 2.5 + bp(white(n, 8903), 200, 700) * 0.35
    motor = 0.09 * np.sin(TAU * 120 * t) + 0.04 * np.sin(TAU * 240 * t) + bp(white(n, 8904), 900, 2400) * 0.05
    chain = np.maximum(0, np.sin(TAU * 31 * t)) ** 8 * bp(white(n, 8905), 1200, 5000) * 0.3
    y += (rumble * 0.55 + motor + chain) * env
    y = put(y, tick(ns(0.04), 1300, 8906, 0.6, 0.001, 0.01), 0.05)  # relay
    m = ns(0.5)
    clunk = modal(m, [210, 433, 760, 1290], [0.7, 0.4, 0.25, 0.12], [0.12, 0.08, 0.05, 0.03], 8907)
    clunk += lp(white(m, 8908), 1200) * np.exp(-tt(m) / 0.02)
    y = put(y, clunk * 0.7, 2.5)
    return fades(warehouse(stereo(y * 0.6, 0.2), 0.2, 1.4, 8910), 0.005, 0.15)


def sfx_car_depart():
    """Modern six-cylinder: idle, a blip of revs, pulls away and fades down the street."""
    dur = 4.2
    n = ns(dur)
    t = tt(n)
    rpm = 800 + 1900 * np.exp(-0.5 * ((t - 0.45) / 0.12) ** 2)       # blip
    rpm += 2000 * np.clip((t - 1.0) / 1.0, 0, 1) ** 0.7                 # first gear
    rpm -= 900 * np.clip((t - 2.0) / 0.2, 0, 1)                          # shift
    rpm += 700 * np.clip((t - 2.2) / 1.6, 0, 1)
    rpm = lp(rpm, 20)
    load = lp(np.where(t > 1.0, 0.9, 0.35), 8)
    eng = engine(rpm, 6, 9100, load)
    tyre = bp(white(n, 9101), 300, 2500) * np.clip((t - 1.0) / 1.2, 0, 1) * 0.12
    y = eng + tyre
    dist = np.clip(1 - (t - 1.4) / 2.6, 0.05, 1) ** 1.6
    w = np.clip((t - 1.4) / 2.0, 0, 1)
    y = (y * (1 - w) + lp(y, 450) * w) * dist
    y = sat(hp(y, 30) * 0.7, 1.4)
    pan = np.clip((t - 1.2) / 2.5, 0, 1) * 0.7
    a = (pan + 1) * np.pi / 4
    s = np.vstack([y * np.cos(a), y * np.sin(a)])
    return fades(warehouse(s, 0.12, 1.0, 9110), 0.002, 0.3)


def sfx_car_arrive():
    """Car approaches from down the street, slows, brakes softly, idles, engine off."""
    dur = 4.0
    n = ns(dur)
    t = tt(n)
    rpm = 2300 - 1500 * np.clip((t - 0.4) / 1.8, 0, 1) ** 0.8
    rpm = np.where(t < 3.1, rpm, 800 * np.exp(-(t - 3.1) / 0.15))
    rpm = lp(rpm, 20)
    load = lp(np.where(t < 1.0, 0.6, 0.2), 6)
    eng = engine(rpm, 6, 9200, load)
    tyre = bp(white(n, 9201), 300, 2500) * np.clip(1 - (t - 0.5) / 2.0, 0, 1) * 0.15
    y = eng + tyre
    near = np.clip(t / 2.2, 0.05, 1) ** 1.6
    w = np.clip(1 - t / 1.8, 0, 1)
    y = (y * (1 - w) + lp(y, 450) * w) * near
    m = ns(0.35)
    squeak = np.sin(TAU * 2350 * tt(m) + 2 * np.sin(TAU * 7 * tt(m))) * np.sin(np.pi * tt(m) / 0.35) ** 2 * 0.03
    y = put(y, squeak, 2.15)
    y = put(y, thump(ns(0.25), 70, 0.06, 9202, 0.3, 0.4) * 0.3, 3.15)  # engine shudders off
    y = sat(hp(y, 30) * 0.7, 1.4)
    pan = (1 - np.clip(t / 2.4, 0, 1)) * -0.7
    a = (pan + 1) * np.pi / 4
    s = np.vstack([y * np.cos(a), y * np.sin(a)])
    return fades(warehouse(s, 0.12, 1.0, 9210), 0.3, 0.2)


# ───────────────────────────── ambience beds (stereo loops) ─────────────────────────────

def _swells(n, count, seed, w_lo, w_hi, a_lo=0.4, a_hi=1.0):
    """Sum of gaussian bumps (cars passing in the distance) as an envelope, per channel with pan."""
    rr = R(seed)
    t = tt(n)
    env = np.zeros((2, n))
    T = n / SR
    for _ in range(count):
        c = rr.uniform(0, T)
        w = rr.uniform(w_lo, w_hi)
        a = rr.uniform(a_lo, a_hi)
        pan = rr.uniform(-0.8, 0.8)
        for sh in (-T, 0, T):  # wrap so the loop is seamless
            g = a * np.exp(-0.5 * ((t - c - sh) / w) ** 2)
            env[0] += g * np.cos((pan + 1) * np.pi / 4)
            env[1] += g * np.sin((pan + 1) * np.pi / 4)
    return env


def amb_interior():
    """Warehouse room tone: HVAC air, a fridge/compressor hum, faint high air hiss. 30 s loop."""
    L = ns(30.0)
    XF = ns(2.0)
    n = L + XF
    t = tt(n)
    out = np.zeros((2, n))
    for ch in range(2):
        air = lp(pink(n, 9300 + ch, 0.9), 350) * 0.12 + bp(pink(n, 9310 + ch, 0.5), 250, 1500) * 0.025
        air *= 1 + 0.08 * np.sin(TAU * t / 7.5 + ch)
        hiss = bp(white(n, 9320 + ch), 3000, 9000) * 0.0025
        out[ch] = air + hiss
    # integer cycles in 30 s, so tonal parts loop perfectly
    hum = 0.010 * np.sin(TAU * 60 * t) + 0.006 * np.sin(TAU * 120 * t) + 0.003 * np.sin(TAU * 180 * t)
    hum *= 1 + 0.25 * np.sin(TAU * t / 15.0)
    comp = 0.0025 * np.sin(TAU * 357 * t + 0.6 * np.sin(TAU * 0.2 * t))
    out += stereo(hum + comp, -0.3)
    y = xfade_loop(out, L, XF)
    return loopf(y, lambda z: hp(z, 25))


def bird_phrase(seed):
    """A small songbird: 3-7 quick downward chirps (FM sine), 3-6 kHz."""
    rr = R(seed)
    k = rr.integers(3, 8)
    out = np.zeros(ns(1.2))
    tp = 0.0
    f0 = rr.uniform(3200, 5200)
    for i in range(k):
        d = rr.uniform(0.03, 0.07)
        m = ns(d)
        tm = tt(m)
        f = f0 * rr.uniform(0.92, 1.1) * (1.25 - 0.45 * tm / d) * (1 + 0.04 * np.sin(TAU * 60 * tm))
        c = np.sin(TAU * np.cumsum(f) / SR) * np.sin(np.pi * tm / d) ** 1.5
        out = put(out, c, tp, rr.uniform(0.5, 1.0))
        tp += d + rr.uniform(0.03, 0.09)
    return out


def car_horn(dur, seed):
    n = ns(dur)
    t = tt(n)
    y = np.zeros(n)
    for f in (420, 510):
        y += np.tanh(3 * np.sin(TAU * f * t)) * 0.5
    y = bp(y, 300, 2200) * att(n, 0.01) * np.clip((dur - t) / 0.03, 0, 1)
    return y


def traffic(n, seed, swells, level, dark=900):
    out = np.zeros((2, n))
    env = _swells(n, swells, seed, 2.0, 6.0)
    for ch in range(2):
        wash = lp(pink(n, seed + 10 + ch, 0.9), dark) * 0.6 + bp(pink(n, seed + 20 + ch, 0.4), 400, 2200) * 0.08
        hiss = bp(white(n, seed + 30 + ch), 600, 3000) * 0.04
        rumble = lp(pink(n, seed + 40 + ch, 1.0), 120) * 0.5
        out[ch] = wash * (0.35 + env[ch]) + hiss * env[ch] + rumble * 0.6
    return out * level


def amb_city_day():
    """Distant daytime city: traffic wash with passing swells, a far horn, birds. 45 s loop."""
    L = ns(45.0)
    XF = ns(3.0)
    n = L + XF
    out = traffic(n, 9400, 22, 0.1)
    rr = R(9450)
    birds = np.zeros((2, n))
    for i in range(14):
        ph = bird_phrase(9460 + i)
        birds = put(birds, stereo(ph, rr.uniform(-0.9, 0.9)), rr.uniform(0.5, 44.0), rr.uniform(0.004, 0.012))
    birds = outdoors(lp(birds, 7500), 0.5, 1.5, 9470, 0.8)[:, :n]
    out += birds
    horn = outdoors(stereo(car_horn(0.28, 9480), 0.5), 1.2, 2.5, 9481, 0.25)
    out = put(out, lp(horn, 1800) * 0.03, 27.0)
    horn2 = outdoors(stereo(car_horn(0.12, 9482), -0.6), 1.2, 2.5, 9483, 0.25)
    out = put(out, lp(horn2, 1600) * 0.02, 9.0)
    out = put(out, lp(horn2, 1600) * 0.02, 9.2)
    y = xfade_loop(out, L, XF)
    return loopf(y, lambda z: hp(lp(z, 9000), 25))


def cricket(n, seed, carrier, rate, pan):
    """One cricket: chirps of 3 pulses (~28 ms period) repeated at `rate` per second, slight wander."""
    rr = R(seed)
    t = tt(n)
    gate = np.zeros(n)
    tp = rr.uniform(0, 0.5)
    while tp < n / SR - 0.2:
        for p in range(3):
            a = ns(tp + p * 0.028)
            b = a + ns(0.016)
            gate[a:b] = 1.0
        tp += 1.0 / rate * rr.uniform(0.9, 1.1)
    gate = lp(gate, 300)
    c = np.sin(TAU * carrier * t + 0.3 * np.sin(TAU * 0.07 * t)) * gate
    slow = 0.6 + 0.4 * np.sin(TAU * t / rr.uniform(8, 20) + rr.uniform(0, TAU)) ** 2
    return stereo(c * slow, pan)


def amb_city_night():
    """Night city: thinner, darker traffic, constant far hum, crickets and insects. 45 s loop."""
    L = ns(45.0)
    XF = ns(3.0)
    n = L + XF
    t = tt(n)
    out = traffic(n, 9500, 7, 0.065, 600)
    for ch in range(2):
        out[ch] += lp(pink(n, 9510 + ch, 1.0), 180) * 0.025
    out += stereo(0.0015 * np.sin(TAU * 100 * t) + 0.001 * np.sin(TAU * 150 * t), 0.0)
    cr = np.zeros((2, n))
    for i, (f, r, p, g) in enumerate([(4350, 2.6, -0.6, 1.0), (4620, 2.2, 0.5, 0.7), (4100, 3.0, 0.15, 0.5),
                                      (4800, 2.4, -0.2, 0.35)]):
        cr += cricket(n, 9520 + i, f, r, p) * g
    insects = np.vstack([bp(white(n, 9530 + c), 5000, 9000) for c in range(2)]) * 0.004
    insects *= 1 + 0.3 * np.sin(TAU * t / 6.0)
    out += (outdoors(cr, 0.4, 1.2, 9540, 0.8)[:, :n]) * 0.006 + insects
    y = xfade_loop(out, L, XF)
    return loopf(y, lambda z: hp(lp(z, 9000), 25))


def amb_rain():
    """Rain on the roof and factory windows: dense patter, low roof roar, window taps, gutter drip. 32 s loop."""
    L = ns(32.0)
    XF = ns(3.0)
    n = L + XF
    out = np.zeros((2, n))
    t = tt(n)
    for ch in range(2):
        bed = bp(pink(n, 9600 + ch, 0.35), 350, 9000)
        swell = 1 + 0.15 * np.sin(TAU * t / 11.0 + ch) + 0.08 * np.sin(TAU * t / 4.3)
        out[ch] += bed * 0.05 * swell
        out[ch] += lp(pink(n, 9610 + ch, 0.9), 220) * 0.07  # rain on a big metal roof
        rr = R(9620 + ch)
        cnt = int(n / SR * 900)
        imp = np.zeros(n)
        idx = rr.integers(0, n, cnt)
        imp[idx] = rr.uniform(-1, 1, cnt) * rr.uniform(0.1, 1, cnt) ** 3
        out[ch] += bp(imp, 2500, 10000) * 0.6
        for i in range(int(n / SR * 14)):
            k = rr.integers(0, n - ns(0.05))
            f = rr.uniform(1800, 4200)
            d = modal(ns(0.05), [f, f * 1.7], [1, 0.4], [0.006, 0.003]) * rr.uniform(0.02, 0.07)
            out[ch, k:k + len(d)] += d
    rr = R(9640)
    tpos = 0.3
    while tpos < n / SR - 0.1:
        m = ns(0.06)
        tm = tt(m)
        f = 700 * (1 + 0.8 * tm / 0.06)
        plop = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-tm / 0.012) * 0.05 * rr.uniform(0.6, 1)
        out = put(out, stereo(plop, 0.55), tpos)
        tpos += rr.uniform(1.1, 1.9)
    y = xfade_loop(out, L, XF)
    return loopf(y, lambda z: hp(lp(z, 9000), 25))


# ───────────────────────────── ambience one-shots (stereo, baked distance) ─────────────────────────────

def amb_siren():
    """Distant police siren wail drifting across, between buildings."""
    dur = 11.0
    n = ns(dur)
    t = tt(n)
    cyc = 3.6
    ph = (t % cyc) / cyc
    f = (650 + 750 * np.sin(np.pi * ph) ** 1.6) * (1 + 0.01 * np.sin(TAU * 0.13 * t))
    y = hp(siren_whoop(f, 9700), 350) * np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 1.4
    pan = -0.7 + 1.2 * t / dur
    s = np.vstack([y * np.cos((pan + 1) * np.pi / 4), y * np.sin((pan + 1) * np.pi / 4)])
    s = outdoors(s, 1.2, 3.5, 9701, 0.35)[:, :n]
    return fades(lp(s, 2400), 0.3, 1.0)


def dog_bark(seed, f0=420):
    m = ns(0.22)
    tm = tt(m)
    f = f0 * (1.15 - 0.35 * tm / 0.22)
    pulses = np.maximum(0, np.sin(TAU * np.cumsum(f) / SR)) ** 3
    src = pulses + white(m, seed) * 0.25
    y = bp(src, 500, 800) * 1.0 + bp(src, 1300, 1900) * 0.6 + bp(src, 2500, 3200) * 0.2
    return sat(y * att(m, 0.006) * np.exp(-tm / 0.07) * 2, 2.0)


def amb_dog_bark():
    """A dog a few blocks away: three barks, then one more."""
    n = ns(2.6)
    y = np.zeros(n)
    for k, (t0, f) in enumerate([(0.0, 430), (0.32, 410), (0.62, 440), (1.6, 400)]):
        y = put(y, dog_bark(9800 + k, f), t0)
    s = outdoors(stereo(lp(y, 2600), -0.45), 0.7, 2.0, 9810, 0.5)
    return fades(s, 0.005, 0.6)


def amb_train_horn():
    """Far freight train horn: a three-chime chord, long-short-long, rolling off the city."""
    dur = 6.5
    n = ns(dur)
    t = tt(n)
    gate = np.zeros(n)
    for a, b in [(0.2, 1.9), (2.2, 2.75), (3.05, 4.9)]:
        gate[(t >= a) & (t < b)] = 1.0
    gate = lp(gate, 12)
    y = np.zeros(n)
    for f in (311.1, 370.0, 466.2):
        y += saw_blep(np.full(n, f) * (1 + 0.003 * np.sin(TAU * 0.4 * t))) * 0.33
    y = lp(y, 1400) * gate
    s = outdoors(stereo(y, 0.55), 1.5, 4.0, 9901, 0.25)[:, :n + ns(1.5)]
    return fades(lp(hp(s, 180), 1500), 0.05, 1.2)


def amb_construction():
    """Distant building site: a few hammer strikes on steel, a short jackhammer burst, a reversing beeper."""
    dur = 6.0
    n = ns(dur)
    y = np.zeros(n)
    rr = R(9950)
    for k, t0 in enumerate([0.1, 0.62, 1.15, 1.66]):
        m = ns(0.4)
        h = modal(m, [890, 1430, 2210, 3150], [0.5, 0.35, 0.2, 0.1], [0.12, 0.08, 0.05, 0.03], 9951 + k)
        h += hp(white(m, 9960 + k), 1500) * np.exp(-tt(m) / 0.003)
        y = put(y, h * rr.uniform(0.7, 1.0), t0)
    m = ns(1.4)
    tm = tt(m)
    jh = np.maximum(0, np.sin(TAU * 22 * tm)) ** 6 * (bp(white(m, 9970), 400, 3000) + 0.5 * lp(white(m, 9971), 200))
    jh *= np.clip(tm / 0.05, 0, 1) * np.clip((1.4 - tm) / 0.05, 0, 1)
    y = put(y, jh * 0.8, 2.4)
    for k in range(4):
        b = np.sin(TAU * 1040 * tt(ns(0.45))) * att(ns(0.45), 0.005) * np.clip((0.45 - tt(ns(0.45))) / 0.01, 0, 1)
        y = put(y, b * 0.15, 3.4 + k * 0.9)
    s = outdoors(stereo(lp(y, 2800), 0.4), 1.2, 2.5, 9980, 0.3)[:, :n]
    return fades(s, 0.01, 0.4)


def amb_car_pass():
    """A single car passing in the street outside (Doppler, tyre roar), heard through the walls."""
    dur = 6.0
    n = ns(dur)
    t = tt(n)
    rpm = 2100 + 150 * np.sin(TAU * t / 5.0)
    eng = engine(rpm, 4, 9990, np.full(n, 0.5), 0.8) * 0.5
    tyre = bp(pink(n, 9991, 0.5), 300, 3000) * 0.6
    s = doppler_pass(eng + tyre, dur, 3.0, 13.0, 15.0)
    return fades(lp(s, 3000), 0.5, 0.8)


# ───────────────────────────── registry ─────────────────────────────

# name: (fn, category, loop, loudness target dB, description)
SOUNDS = {
    "SFX_Click": (lambda: sfx_click(0), "UI", False, -30, "soft crisp switch click (3 round-robin variants)"),
    "SFX_Click_2": (lambda: sfx_click(1), "UI", False, -30, ""),
    "SFX_Click_3": (lambda: sfx_click(2), "UI", False, -30, ""),
    "SFX_Hover": (lambda: sfx_hover(0), "UI", False, -38, "barely-there high tick (2 variants)"),
    "SFX_Hover_2": (lambda: sfx_hover(1), "UI", False, -38, ""),
    "SFX_PanelOpen": (lambda: sfx_panel(True), "UI", False, -33, "short airy whoosh, rising, faint tick"),
    "SFX_PanelClose": (lambda: sfx_panel(False), "UI", False, -34, "short airy whoosh, falling"),
    "SFX_Confirm": (sfx_confirm, "UI", False, -28, "soft rising two-step glassy blip"),
    "SFX_Error": (sfx_error, "UI", False, -28, "muted low double bonk"),
    "SFX_SlotFill": (sfx_slotfill, "UI", False, -29, "magnetic snap of a card into a holder"),
    "SFX_Cash": (sfx_cash, "Event", False, -26, "electric bill-counter riffle"),
    "SFX_MissionStart": (sfx_mission_start, "Event", False, -23, "radio chirp, then a car starter cranks and catches"),
    "SFX_MissionComplete": (sfx_mission_complete, "Event", False, -24, "phone buzz on a desk + soft two-note ping"),
    "SFX_Success": (sfx_success, "Event", False, -22, "airy riser into a soft low hit with a bright shimmer"),
    "SFX_Fail": (sfx_fail, "Event", False, -22, "heavy steel door slam in the warehouse"),
    "SFX_Busted": (sfx_busted, "Event", False, -25, "two distant police siren whoops"),
    "SFX_LevelUp": (sfx_levelup, "Event", False, -24, "three quick glassy chime notes + air"),
    "SFX_BuildStart": (sfx_build_start, "Event", False, -25, "two cordless-driver bursts"),
    "SFX_RoomBuilt": (sfx_room_built, "Event", False, -24, "breaker clunk, fluorescent starters tick, tubes hum on"),
    "SFX_Perk": (sfx_perk, "Event", False, -25, "metal lighter: lid clink, flint wheel, flame bloom"),
    "SFX_Payday": (sfx_payday, "Event", False, -23, "bill counter, register beep, cash drawer pops"),
    "SFX_GarageDoor": (sfx_garage_door, "Event", False, -27, "motorised steel roller door"),
    "SFX_CarDepart": (sfx_car_depart, "Event", False, -26, "six-cylinder idle, rev blip, pulls away"),
    "SFX_CarArrive": (sfx_car_arrive, "Event", False, -27, "car approaches, brakes, idles, engine off"),
    "AMB_Interior": (amb_interior, "Ambience", True, -36, "warehouse room tone: HVAC air, fridge hum"),
    "AMB_CityDay": (amb_city_day, "Ambience", True, -32, "distant day city: traffic wash, a far horn, birds"),
    "AMB_CityNight": (amb_city_night, "Ambience", True, -34, "night city: thin traffic, far hum, crickets"),
    "AMB_Rain": (amb_rain, "Ambience", True, -31, "rain on the roof and factory windows"),
    "AMB_Siren": (amb_siren, "AmbienceOneShot", False, -33, "distant police siren wail (night)"),
    "AMB_DogBark": (amb_dog_bark, "AmbienceOneShot", False, -34, "dog barking a few blocks away (night)"),
    "AMB_TrainHorn": (amb_train_horn, "AmbienceOneShot", False, -35, "far freight-train horn (night)"),
    "AMB_Construction": (amb_construction, "AmbienceOneShot", False, -34, "distant building site (day)"),
    "AMB_CarPass": (amb_car_pass, "AmbienceOneShot", False, -32, "car passing in the street outside"),
}


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


def trim_tail(x, thresh_db=-60.0):
    a = np.abs(np.atleast_2d(x)).max(0)
    idx = np.nonzero(a > 10 ** (thresh_db / 20) * a.max())[0]
    end = min(a.shape[0], (idx[-1] if len(idx) else a.shape[0]) + ns(0.02))
    return fades(x[..., :end], 0.0, min(0.05, end / SR / 4))


def main(argv):
    os.makedirs(OUT, exist_ok=True)
    names = argv or list(SOUNDS)
    for name in names:
        fn, cat, loop, target, _ = SOUNDS[name]
        y = np.asarray(fn(), dtype=float)
        if not loop:
            y = trim_tail(hp(y, 20, 2))  # remove any DC / sub-sonic drift
        y = loud_norm(y, target, -1.0)
        write_wav(os.path.join(OUT, name + ".wav"), y)
        ch = 1 if y.ndim == 1 else 2
        pk = 20 * np.log10(np.max(np.abs(y)) + 1e-12)
        print(f"{name:22s} {cat:16s} ch={ch} {y.shape[-1] / SR:6.2f}s loop={str(loop):5s} "
              f"loud={loudness_db(y):6.1f} peak={pk:5.1f}")
    # Remove stale WAVs (e.g. old music / theatre sounds) that are no longer in the registry.
    for f in os.listdir(OUT):
        if f.endswith(".wav") and f[:-4] not in SOUNDS:
            os.remove(os.path.join(OUT, f))
            print("removed stale", f)
    manifest = []
    for name, (_, cat, loop, target, _) in SOUNDS.items():
        manifest.append({"file": name + ".wav", "asset": f"/Game/DC/Audio/{name}.{name}", "category": cat,
                         "loop": loop, "loudness": target})
    with open(os.path.join(OUT, "manifest.json"), "w") as f:
        json.dump({"sampleRate": SR, "generator": "Tools/audio_gen.py", "sounds": manifest}, f, indent=2)
    write_licenses()


def write_licenses():
    lines = [
        "# Director's Cut: audio sources and licences",
        "",
        "Every audio file in this folder is **procedurally generated by the project** with",
        "`Tools/audio_gen.py` (numpy/scipy synthesis: filtered noise, modal resonators, simple engine and",
        "motor models, synthetic room/outdoor reverb). No samples, recordings, sound packs, or third-party",
        "material are used, and nothing is generated by an external AI service. The material is original to",
        "the project (author: the Director's Cut team, generated 2026-11-13), so no attribution is required.",
        "There is no music in the game.",
        "",
        "Regenerate with: `python3 Tools/audio_gen.py` (needs `pip install numpy scipy`), then import with",
        "`Tools/audio_import.py` (headless, see its docstring). The WAVs are source only and are not staged.",
        "",
        "| File | Category | Loop | Loudness (dB, gated K-weighted) | Content | Source | Licence |",
        "|---|---|---|---|---|---|---|",
    ]
    last = ""
    for name, (_, cat, loop, target, desc) in SOUNDS.items():
        d = desc or f"variant of {last}"
        if desc:
            last = name
        lines.append(f"| {name}.wav | {cat} | {'yes' if loop else 'no'} | {target} | {d} | procedurally generated "
                     f"by the project (Tools/audio_gen.py) | project-owned original |")
    with open(os.path.join(OUT, "LICENSES.md"), "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
