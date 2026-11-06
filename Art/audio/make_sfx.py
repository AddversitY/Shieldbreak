"""Renders Shieldbreak's combat sound banks into Art/Export/Audio/SFX/*.wav (48 kHz, 16-bit).

Every bank is a set of variations (SFX_<Bank>_NN.wav) that the game picks between at random (never the same one twice
in a row), with a small pitch and volume jitter on top. Banks are built from two kinds of material:

- Recorded layers from Epic's free Lyra sample content, which this project already ships under Content/Audio
  (punch, mechanism, tail and distant recordings). Run Art/audio/extract_lyra_audio.py first: it copies the
  source WAVs embedded in those .uasset files to Art/audio/.lyra_src (ignored by git).
- Original synthesized layers (sub thump, ionised snap, energy zaps, armour clank, shield tones, reverbs).

Nothing here is taken from any Halo game. Weapons are layered the way shipped shooters do it:
  <W>_FP      first-person close shot, stereo: transient punch + body + mechanism + sub + Shieldbreak "ion snap"
  <W>_FPTail  first-person tail, stereo: the shot's reflection off the arena (long, decorrelated)
  <W>_TP      third-person close shot, mono (spatialised): punch + body + short room
  <W>_TPTail  third-person tail, mono
  <W>_Dist    distant shot, mono: muffled crack + long rolling tail (crossfaded in by the game past ~30 m)

Usage: python Art/audio/make_sfx.py [--preview]   (--preview also writes a short MP3 per bank to Art/audio/.preview)
"""
import glob
import json
import os
import subprocess
import sys
import wave
import zlib

import numpy as np
from scipy import signal

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, ".lyra_src")
OUT = os.path.normpath(os.path.join(HERE, "..", "Export", "Audio", "SFX"))
PREVIEW = os.path.join(HERE, ".preview")

# ---------------------------------------------------------------------------------------------------------------
# I/O


def read_wav(path):
    with wave.open(path, "rb") as w:
        sr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        raw = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768.0
    x = raw.reshape(-1, ch)
    if ch == 1:
        x = np.repeat(x, 2, axis=1)
    if sr != SR:
        g = np.gcd(sr, SR)
        x = signal.resample_poly(x, SR // g, sr // g, axis=0).astype(np.float32)
    return x


_cache = {}


def src(name, index):
    """A Lyra source layer, e.g. src("Weapons__Rifle__Weapons_Rifle_Punch-Close", 3). Index wraps around."""
    files = _cache.get(name)
    if files is None:
        files = sorted(glob.glob(os.path.join(SRC, "*" + name + "_[0-9]*.wav")))
        if not files:
            files = sorted(glob.glob(os.path.join(SRC, "*" + name + ".wav")))
        if not files:
            raise SystemExit("missing Lyra source layer %s (run extract_lyra_audio.py)" % name)
        _cache[name] = files
    return read_wav(files[index % len(files)])


def write_wav(path, x):
    x = np.clip(x, -1.0, 1.0)
    pcm = (x * 32767.0).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(pcm.shape[1] if pcm.ndim == 2 else 1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


# ---------------------------------------------------------------------------------------------------------------
# DSP building blocks (all stereo float32 arrays shaped (n, 2))


def silence(seconds):
    return np.zeros((int(seconds * SR), 2), np.float32)


def mix(dst, x, at=0.0, gain=1.0):
    """Adds x into dst at time `at`, growing dst if needed. Returns dst."""
    start = int(at * SR)
    end = start + len(x)
    if end > len(dst):
        dst = np.concatenate([dst, np.zeros((end - len(dst), 2), np.float32)])
    dst[start:end] += x * gain
    return dst


def pitch(x, factor):
    """Resampling pitch shift (also changes length, like a tape/sampler)."""
    if abs(factor - 1.0) < 1e-3:
        return x
    n = max(1, int(round(len(x) / factor)))
    return signal.resample(x, n, axis=0).astype(np.float32)


def sos_filter(x, kind, freq, order=2):
    nyq = SR / 2
    if kind == "bandpass":
        wn = [freq[0] / nyq, min(freq[1] / nyq, 0.99)]
    else:
        wn = min(freq / nyq, 0.99)
    sos = signal.butter(order, wn, btype=kind, output="sos")
    return signal.sosfilt(sos, x, axis=0).astype(np.float32)


def lp(x, f, order=2):
    return sos_filter(x, "lowpass", f, order)


def hp(x, f, order=2):
    return sos_filter(x, "highpass", f, order)


def bp(x, lo, hi, order=2):
    return sos_filter(x, "bandpass", (lo, hi), order)


def peak_eq(x, f, gain_db, q=1.0):
    a = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f / SR
    alpha = np.sin(w0) / (2 * q)
    b = [1 + alpha * a, -2 * np.cos(w0), 1 - alpha * a]
    den = [1 + alpha / a, -2 * np.cos(w0), 1 - alpha / a]
    return signal.lfilter(b, den, x, axis=0).astype(np.float32)


def env(n, attack, decay, hold=0.0):
    t = np.arange(n) / SR
    e = np.where(t < attack, t / max(attack, 1e-6), np.exp(-np.maximum(t - attack - hold, 0) / max(decay, 1e-6)))
    return e.astype(np.float32)


def fade(x, fade_in=0.0005, fade_out=0.02):
    n = len(x)
    a = min(int(fade_in * SR), n)
    b = min(int(fade_out * SR), n)
    g = np.ones(n, np.float32)
    if a > 0:
        g[:a] = np.linspace(0, 1, a)
    if b > 0:
        g[n - b:] *= np.linspace(1, 0, b) ** 2
    return x * g[:, None]


def trim(x, seconds, fade_out=0.05):
    return fade(x[: int(seconds * SR)].copy(), 0.0, fade_out)


def stereo(m, width=0.0, rng=None):
    """Mono (n,) -> stereo; width>0 adds a tiny decorrelating delay on one side."""
    s = np.stack([m, m], axis=1).astype(np.float32)
    if width > 0:
        d = int(width * SR)
        s[d:, 1] = m[:-d] if d > 0 else m
        s[:d, 1] = 0
    return s


def tone(seconds, f0, f1, glide, attack, decay, harmonics=(1.0,), fm=None):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    a = np.clip(t / max(glide, 1e-6), 0, 1)
    hz = f0 * (f1 / f0) ** a
    if fm:
        rate, depth = fm
        hz = hz * (1 + depth * np.sin(2 * np.pi * rate * t))
    ph = 2 * np.pi * np.cumsum(hz) / SR
    w = sum(g * np.sin((i + 1) * ph) for i, g in enumerate(harmonics))
    return (w * env(n, attack, decay)).astype(np.float32)


def noise(seconds, rng, attack, decay, hold=0.0):
    n = int(seconds * SR)
    return (rng.uniform(-1, 1, n) * env(n, attack, decay, hold)).astype(np.float32)


def ring(seconds, partials, decay, rng, attack=0.0005):
    """Inharmonic struck-metal ring: [(Hz, gain), ...] each with its own decay spread."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    out = np.zeros(n, np.float32)
    for hz, g in partials:
        ph = rng.uniform(0, 2 * np.pi)
        d = decay * rng.uniform(0.7, 1.3) * (600.0 / hz) ** 0.35
        out += g * np.sin(2 * np.pi * hz * t + ph) * env(n, attack, d)
    return out


def saturate(x, drive):
    return (np.tanh(x * drive) / np.tanh(drive)).astype(np.float32)


def reverb(seconds, rt60, rng, bright=6000.0, dark=1800.0, predelay=0.012, early=()):
    """Stereo impulse response: early reflections + decorrelated noise tail that darkens as it decays."""
    n = int(seconds * SR)
    t = np.arange(n) / SR
    decay = np.exp(-6.91 * t / rt60).astype(np.float32)
    ir = np.zeros((n, 2), np.float32)
    for c in range(2):
        nz = rng.standard_normal(n).astype(np.float32)
        b = lp(nz, bright)
        d = lp(nz, dark)
        mixw = np.clip(t / (rt60 * 0.5), 0, 1)
        ir[:, c] = (b * (1 - mixw) + d * mixw) * decay
    ir[: int(predelay * SR)] = 0
    for at, g, pan in early:
        i = int(at * SR)
        if i < n:
            ir[i, 0] += g * (1 - pan)
            ir[i, 1] += g * (1 + pan)
    return ir / (np.sqrt(np.sum(ir ** 2) / 2) + 1e-9)


def convolve(x, ir):
    out = np.zeros((len(x) + len(ir) - 1, 2), np.float32)
    for c in range(2):
        out[:, c] = signal.fftconvolve(x[:, c], ir[:, c])
    return out


def normalize_rms(x, rms_db=-16.0, peak_db=-3.0):
    """Loudest 50 ms window to rms_db, capped so the peak stays under peak_db (tails are sustained noise)."""
    n = int(0.05 * SR)
    m = to_mono(x)[:, 0]
    loud = max(np.sqrt(np.mean(m[i:i + n] ** 2)) for i in range(0, max(1, len(m) - n), n // 2)) + 1e-9
    g = min(10 ** (rms_db / 20) / loud, 10 ** (peak_db / 20) / (np.max(np.abs(x)) + 1e-9))
    return (x * g).astype(np.float32)


def normalize(x, peak_db=-1.0):
    m = np.max(np.abs(x)) + 1e-9
    return (x * (10 ** (peak_db / 20) / m)).astype(np.float32)


def to_mono(x):
    return np.mean(x, axis=1, keepdims=True).repeat(2, axis=1)


def strip_silence_head(x, thresh=0.01):
    idx = np.argmax(np.max(np.abs(x), axis=1) > thresh * np.max(np.abs(x)))
    return x[max(0, idx - int(0.0005 * SR)):]


def end_trim(x, thresh_db=-62.0, fade_out=0.08):
    """Cuts trailing near-silence so banks stay small."""
    lvl = np.max(np.abs(x), axis=1)
    above = np.nonzero(lvl > 10 ** (thresh_db / 20) * np.max(lvl))[0]
    end = min(len(x), (above[-1] if len(above) else len(x)) + int(0.02 * SR))
    return fade(x[:end].copy(), 0.0, min(fade_out, end / SR / 3))


# ---------------------------------------------------------------------------------------------------------------
# Shared original layers


def sub_thump(rng, f0=62.0, f1=38.0, decay=0.09, seconds=0.5):
    m = tone(seconds, f0 * rng.uniform(0.96, 1.04), f1, decay * 1.2, 0.0015, decay, harmonics=(1.0, 0.25))
    return stereo(saturate(m, 1.6))


def ion_snap(rng, ping_hz=3100.0, gain_ping=0.35):
    """Shieldbreak's signature: a very short ionised crack with a falling metallic ping. Futuristic, not a laser."""
    crack = bp(noise(0.05, rng, 0.0002, 0.006), 2500, 11000)
    ping = ring(0.12, [(ping_hz * rng.uniform(0.97, 1.03), 1.0), (ping_hz * 1.58, 0.45), (ping_hz * 2.31, 0.22)],
                0.018, rng)
    zip_ = tone(0.06, 2400 * rng.uniform(0.95, 1.05), 700, 0.04, 0.0003, 0.012)
    m = np.zeros(int(0.12 * SR), np.float32)
    m[: len(crack)] += crack
    m += ping * gain_ping
    m[: len(zip_)] += zip_ * 0.18
    return stereo(m, width=0.0004)


def room_ir(rng, rt60=0.45, seconds=0.9):
    return reverb(seconds, rt60, rng, bright=7000, dark=2200, predelay=0.006,
                  early=[(0.011, 0.5, -0.6), (0.019, 0.4, 0.5), (0.031, 0.3, -0.2), (0.047, 0.25, 0.7)])


def arena_ir(rng, rt60=1.6, seconds=2.6):
    return reverb(seconds, rt60, rng, bright=5000, dark=1300, predelay=0.03,
                  early=[(0.045, 0.6, -0.5), (0.071, 0.5, 0.6), (0.112, 0.4, -0.7), (0.165, 0.3, 0.3)])


# ---------------------------------------------------------------------------------------------------------------
# Banks

BANKS = {}


def bank(name, count):
    def register(fn):
        BANKS[name] = (count, fn)
        return fn
    return register


def gun_set(name, count, close_fn, tail_fn, dist_fn, tp_room=0.35):
    """Registers <name>_FP/_FPTail/_TP/_TPTail/_Dist from three per-variant builders."""
    @bank(name + "_FP", count)
    def _fp(i, rng):
        return normalize(end_trim(close_fn(i, rng)), -0.8)

    @bank(name + "_TP", count)
    def _tp(i, rng):
        dry = to_mono(close_fn(i, rng))
        wet = convolve(dry, room_ir(rng, tp_room))
        return normalize(end_trim(to_mono(mix(wet * 0.35, dry, 0.0))), -1.0)

    @bank(name + "_FPTail", 3)
    def _fpt(i, rng):
        return normalize_rms(end_trim(tail_fn(i, rng), -58))

    @bank(name + "_TPTail", 3)
    def _tpt(i, rng):
        return normalize_rms(end_trim(to_mono(tail_fn(i + 7, rng)), -58))

    @bank(name + "_Dist", 4)
    def _dist(i, rng):
        return normalize(end_trim(to_mono(dist_fn(i, rng)), -58), -1.0)


# ---- Battle rifle: tight, mid-heavy, three of these per trigger pull --------------------------------------------


def br_close(i, rng):
    x = silence(0.6)
    punch = pitch(src("Weapons__Rifle__Weapons_Rifle_Punch-Close", i * 3), 0.9 * rng.uniform(0.98, 1.02))
    body = pitch(src("SoundWaves__Weapons__sfx_Weapon_AutoRifle_MainLayer", i), 0.92 * rng.uniform(0.98, 1.02))
    sweet = src("SoundWaves__Weapons__sfx_Weapon_AutoRifle_ShotSweetener", i)
    mech = hp(src("Weapons__Rifle__Weapons_Rifle_Mech", i), 700)
    x = mix(x, punch, 0, 1.0)
    x = mix(x, lp(body, 9000), 0.0, 0.55)
    x = mix(x, sweet, 0, 0.35)
    x = mix(x, mech, 0.004, 0.55)
    x = mix(x, sub_thump(rng, 66, 40, 0.07), 0, 0.55)
    x = mix(x, ion_snap(rng, 3050), 0, 0.22)
    x = peak_eq(x, 180, 3.0, 0.8)
    x = peak_eq(x, 2600, 2.0, 1.2)
    return saturate(normalize(x, 0), 1.8)


def br_tail(i, rng):
    t = src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Close", i)
    t = lp(t, 7000)
    body = pitch(src("Weapons__Rifle__Weapons_Rifle_Punch-Close", i + 5), 0.85)
    wet = convolve(body, arena_ir(rng, 1.3))
    return mix(t, wet, 0.0, 0.25 * np.max(np.abs(t)) / (np.max(np.abs(wet)) + 1e-9))


def br_dist(i, rng):
    x = silence(0.1)
    x = mix(x, src("Weapons__Rifle__Weapons_Rifle_Punch-Distant", i * 2), 0, 1.0)
    x = mix(x, src("Weapons__Rifle__Weapons_Rifle_Punch-Far", i * 3), 0.01, 0.6)
    tail = src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Distant", i)
    x = mix(x, tail, 0, 0.8 * np.max(np.abs(x)) / (np.max(np.abs(tail)) + 1e-9))
    return lp(x, 4200)


gun_set("BR", 8, br_close, br_tail, br_dist)


# ---- Sidearm: snappy and bright, a heavy magnum-style crack ----------------------------------------------------


def pistol_close(i, rng):
    x = silence(0.5)
    x = mix(x, pitch(src("Weapons__Pistol__Weapons_Pistol_Punch-Close", i), 0.93 * rng.uniform(0.98, 1.02)), 0, 1.0)
    x = mix(x, trim(src("SoundWaves__Weapons__sfx_Weapon_Pistol_MainLayer_nl", i), 0.35), 0, 0.6)
    x = mix(x, trim(src("Weapons__Pistol__Weapons_Pistol_Sweetener", i), 0.3), 0, 0.35)
    x = mix(x, hp(src("Weapons__Pistol__Weapons_Pistol_Mech", i), 900), 0.003, 0.7)
    x = mix(x, sub_thump(rng, 78, 46, 0.06), 0, 0.5)
    x = mix(x, ion_snap(rng, 3600, 0.3), 0, 0.2)
    x = peak_eq(x, 3200, 2.5, 1.0)
    return saturate(normalize(x, 0), 1.7)


def pistol_tail(i, rng):
    t = pitch(src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Close", i + 2), 1.08)
    return trim(lp(t, 7500), 1.6, 0.4)


def pistol_dist(i, rng):
    x = silence(0.1)
    x = mix(x, src("Weapons__Pistol__Weapons_Pistol_Punch-Distant", i), 0, 1.0)
    x = mix(x, src("Weapons__Pistol__Weapons_Pistol_Punch-Far", i), 0.012, 0.5)
    tail = pitch(src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Distant", i + 3), 1.1)
    x = mix(x, tail, 0, 0.6 * np.max(np.abs(x)) / (np.max(np.abs(tail)) + 1e-9))
    return lp(x, 4500)


gun_set("Sidearm", 6, pistol_close, pistol_tail, pistol_dist)


# ---- Sniper: huge, low, a supersonic crack and an echo that rolls around the arena -----------------------------


def sniper_close(i, rng):
    x = silence(1.0)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_Punch", i), 0.82 * rng.uniform(0.98, 1.02)), 0, 1.0)
    x = mix(x, pitch(src("Weapons__Rifle2__Weapons_Rifle2_Punch", i * 2), 0.8), 0, 0.8)
    x = mix(x, pitch(src("Weapons__Rifle__Weapons_Rifle_Punch-Close", i * 5), 0.72), 0, 0.6)
    x = mix(x, trim(src("SoundWaves__Weapons__sfx_Weapon_Shotgun_MainLayer_nl", i), 0.7, 0.3), 0, 0.45)
    x = mix(x, sub_thump(rng, 52, 28, 0.2, 0.9), 0, 0.9)
    # Supersonic crack: an N-wave click with a bright band of noise.
    crack = bp(noise(0.02, rng, 0.0001, 0.003), 3000, 14000)
    x = mix(x, stereo(crack, 0.0003), 0, 0.5)
    x = mix(x, ion_snap(rng, 2300, 0.45), 0, 0.3)
    x = peak_eq(x, 90, 4.0, 0.7)
    return saturate(normalize(x, 0), 2.2)


def sniper_tail(i, rng):
    t = src("Weapons__Shotgun__Weapons_Shotgun_Noise-Exterior-Close", i)
    t = lp(pitch(t, 0.92), 5500)
    # Two discrete slapback echoes off far walls.
    body = lp(pitch(src("Weapons__Shotgun__Weapons_Shotgun_Punch", i + 1), 0.8), 2500)
    t = mix(t, body, 0.32 + 0.05 * i, 0.35)
    t = mix(t, body, 0.71 + 0.07 * i, 0.18)
    return t


def sniper_dist(i, rng):
    x = silence(0.1)
    x = mix(x, pitch(src("Weapons__Rifle__Weapons_Rifle_Punch-Far", i * 4), 0.75), 0, 1.0)
    tail = src("Weapons__Shotgun__Weapons_Shotgun_Noise-Exterior-Distant", i)
    x = mix(x, tail, 0, 0.9 * np.max(np.abs(x)) / (np.max(np.abs(tail)) + 1e-9))
    return lp(x, 3500)


gun_set("Sniper", 5, sniper_close, sniper_tail, sniper_dist, tp_room=0.5)


@bank("Sniper_Bolt", 3)
def sniper_bolt(i, rng):
    """Played in first person ~0.3 s after each shot: the bolt cycling."""
    x = pitch(src("Weapons__Rifle__Weapons_Rifle_Bolt", 0), 0.9 + 0.04 * i)
    x = mix(x, hp(src("Weapons__Shotgun__Weapons_Shotgun_Mech", i), 400), 0.02, 0.6)
    return normalize(end_trim(x), -4.0)


# ---- Rocket launcher: tube pop, ignition hiss and a heavy push -------------------------------------------------


def rocket_close(i, rng):
    x = silence(1.2)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_Punch", i + 1), 0.62), 0, 0.8)
    pop = tone(0.3, 190 * rng.uniform(0.95, 1.05), 90, 0.08, 0.001, 0.05, harmonics=(1.0, 0.4, 0.2))
    x = mix(x, stereo(pop), 0, 0.6)
    x = mix(x, sub_thump(rng, 48, 26, 0.18, 0.9), 0, 0.9)
    # Ignition: band-limited noise that sweeps up as the motor lights, panned slightly as it leaves.
    n = int(0.9 * SR)
    hiss = rng.uniform(-1, 1, n).astype(np.float32)
    sweep = np.zeros(n, np.float32)
    lo, hi = 300.0, 3200.0
    for k, seg in enumerate(np.array_split(np.arange(n), 18)):
        f = lo * (hi / lo) ** (k / 17)
        sweep[seg] = bp(hiss, f * 0.6, f * 1.6)[seg]
    sweep *= env(n, 0.02, 0.28, 0.08)
    x = mix(x, stereo(sweep, 0.0006), 0.015, 0.7)
    x = mix(x, trim(src("SoundWaves__Weapons__sfx_Weapon_Shotgun_SecondaryLayer_nl", i), 0.9, 0.3), 0, 0.35)
    return saturate(normalize(x, 0), 1.6)


def rocket_tail(i, rng):
    return lp(pitch(src("Weapons__Shotgun__Weapons_Shotgun_Noise-Exterior-Close", i + 1), 0.8), 4000)


def rocket_dist(i, rng):
    x = lp(pitch(src("Weapons__Shotgun__Weapons_Shotgun_Noise-Exterior-Far", i), 0.85), 2600)
    return mix(x, sub_thump(rng, 44, 28, 0.15, 0.6), 0, 0.4 * np.max(np.abs(x)))


gun_set("Rocket", 4, rocket_close, rocket_tail, rocket_dist, tp_room=0.5)


# ---- Rail Lance: charged energy weapon. A big electric crack, a falling zap and a metallic ring ----------------


def rail_close(i, rng):
    x = silence(1.4)
    x = mix(x, pitch(src("Weapons__Rifle2__Weapons_Rifle2_Punch", i * 3 + 1), 0.85), 0, 0.7)
    crack = bp(noise(0.08, rng, 0.0001, 0.012), 1800, 15000)
    x = mix(x, stereo(crack, 0.0004), 0, 0.9)
    zap = tone(0.4, 3400 * rng.uniform(0.96, 1.04), 140, 0.22, 0.0005, 0.09, harmonics=(1.0, 0.5, 0.3, 0.15),
               fm=(97.0, 0.08))
    x = mix(x, stereo(zap, 0.0007), 0, 0.5)
    x = mix(x, stereo(ring(1.2, [(612, 1.0), (1489, 0.6), (2377, 0.45), (3911, 0.25)], 0.22, rng)), 0.004, 0.35)
    x = mix(x, sub_thump(rng, 58, 30, 0.16, 0.8), 0, 0.85)
    sw = trim(src("Weapons__Rifle2__Weapons_Rifle2_Sweetener", i), 1.2, 0.5)
    x = mix(x, hp(sw, 600), 0.01, 0.3)
    return saturate(normalize(x, 0), 1.9)


def rail_tail(i, rng):
    t = lp(pitch(src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Close", i + 4), 0.82), 6000)
    shimmer = stereo(ring(2.0, [(1224, 0.6), (2978, 0.4), (4754, 0.25)], 0.5, rng))
    wet = convolve(shimmer, arena_ir(rng, 1.8))
    return mix(t, wet, 0.02, 0.2 * np.max(np.abs(t)) / (np.max(np.abs(wet)) + 1e-9))


def rail_dist(i, rng):
    x = silence(0.1)
    x = mix(x, lp(pitch(src("Weapons__Rifle__Weapons_Rifle_Punch-Far", i * 5 + 2), 0.8), 5000), 0, 1.0)
    z = tone(0.3, 2600, 200, 0.2, 0.001, 0.07)
    x = mix(x, stereo(lp(z, 3000)), 0, 0.4)
    tail = pitch(src("Weapons__Rifle__Weapons_Rifle_Noise-Exterior-Distant", i + 2), 0.85)
    x = mix(x, tail, 0, 0.8 * np.max(np.abs(x)) / (np.max(np.abs(tail)) + 1e-9))
    return lp(x, 4000)


gun_set("Rail", 4, rail_close, rail_tail, rail_dist, tp_room=0.5)


@bank("Rail_Charge", 1)
def rail_charge(i, rng):
    """0.8 s to full charge: rising whine, crackle that thickens, then a held 'ready' hum. The game stops it on release."""
    n = int(2.4 * SR)
    t = np.arange(n) / SR
    rise = np.clip(t / 0.8, 0, 1)
    hz = 220 * (1900 / 220) ** (rise ** 0.8)
    ph = 2 * np.pi * np.cumsum(hz) / SR
    whine = np.sin(ph) + 0.35 * np.sin(2 * ph + 0.3) + 0.12 * np.sin(3.01 * ph)
    amp = np.clip(t / 0.05, 0, 1) * (0.55 + 0.45 * rise)
    hum = np.sin(2 * np.pi * 95 * t) * 0.35 * rise + np.sin(2 * np.pi * 190 * t) * 0.2 * rise
    crackle = bp(rng.uniform(-1, 1, n).astype(np.float32), 2000, 9000)
    gate = (rng.uniform(0, 1, n) < (0.002 + 0.02 * rise)).astype(np.float32)
    gate = signal.lfilter([1.0], [1.0, -0.995], gate)
    ready = (t > 0.8).astype(np.float32) * (0.6 + 0.4 * np.sin(2 * np.pi * 7 * t))
    m = whine * amp * (1 - 0.35 * ready) + hum * (1 + 0.5 * ready) + crackle * np.clip(gate, 0, 1) * 0.6
    ding = ring(0.6, [(1900, 1.0), (2850, 0.5)], 0.12, rng)
    m[int(0.8 * SR): int(0.8 * SR) + len(ding)] += ding[: n - int(0.8 * SR)] * 0.35
    return normalize(fade(stereo(m.astype(np.float32), 0.0005), 0.003, 0.25), -2.0)


# ---- Reloads: timed to each weapon's reload length so the click lands as the ammo arrives -----------------------


def foley(i, gain=0.4, seconds=0.35):
    return trim(src("Movement__Movement_Foley-Step", i), seconds, 0.1) * gain


@bank("BR_Reload", 2)
def br_reload(i, rng):  # ReloadTime 2.0
    x = silence(2.0)
    x = mix(x, foley(i), 0.05)
    x = mix(x, src("Weapons__Rifle__Weapons_Rifle_ClipOut", 0), 0.22, 1.0)
    x = mix(x, foley(i + 3, 0.3), 0.75)
    x = mix(x, src("Weapons__Rifle__Weapons_Rifle_ClipIn", 0), 1.12 + 0.03 * i, 1.0)
    x = mix(x, pitch(src("Weapons__Rifle__Weapons_Rifle_Bolt", 0), 1.05), 1.55, 0.9)
    return normalize(end_trim(x), -3.0)


@bank("Sidearm_Reload", 2)
def sidearm_reload(i, rng):  # ReloadTime 1.4
    x = silence(1.4)
    x = mix(x, src("Weapons__Pistol__Weapons_Pistol_ClipOut", 0), 0.1, 1.0)
    x = mix(x, foley(i + 1, 0.3, 0.25), 0.35)
    x = mix(x, src("Weapons__Pistol__Weapons_Pistol_ClipIn", 0), 0.78 + 0.02 * i, 1.0)
    x = mix(x, src("Weapons__Pistol__Weapons_Pistol_Slide", 0), 1.08, 0.9)
    return normalize(end_trim(x), -3.0)


@bank("Sniper_Reload", 2)
def sniper_reload(i, rng):  # ReloadTime 2.6
    x = silence(2.6)
    x = mix(x, foley(i + 2), 0.05)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_ClipOut", 0), 0.9), 0.3, 1.0)
    x = mix(x, foley(i + 5, 0.3), 0.9)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_ClipIn", 0), 0.92), 1.45, 1.0)
    x = mix(x, pitch(src("Weapons__Rifle__Weapons_Rifle_Bolt", 0), 0.85), 2.05, 1.0)
    return normalize(end_trim(x), -3.0)


@bank("Rocket_Reload", 2)
def rocket_reload(i, rng):  # ReloadTime 3.0
    x = silence(3.0)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_HandHit", 0), 0.75), 0.25, 0.9)
    x = mix(x, foley(i + 4, 0.45), 0.7)
    scrape = bp(noise(0.5, rng, 0.05, 0.2, 0.15), 600, 3500)
    x = mix(x, stereo(scrape, 0.0005), 1.25, 0.25)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_ClipIn", 0), 0.7), 1.7, 1.0)
    x = mix(x, pitch(src("Weapons__Shotgun__Weapons_Shotgun_ClipOut", 0), 0.8), 2.45, 0.9)
    return normalize(end_trim(x), -3.0)


@bank("DryFire_Light", 3)
def dry_light(i, rng):
    x = src("SoundWaves__Weapons__sfx_Weapon_Pistol_DryClick_nl", i)
    return normalize(end_trim(x), -6.0)


@bank("DryFire_Rifle", 3)
def dry_rifle(i, rng):
    x = mix(src("Weapons__Rifle__Weapons_Rifle_DryFire", 0) * 1.0, src("SoundWaves__Weapons__sfx_Weapon_AutoRifle_DryClick", i), 0.0, 0.7)
    return normalize(end_trim(pitch(x, 1 + 0.03 * (i - 1))), -6.0)


@bank("DryFire_Heavy", 3)
def dry_heavy(i, rng):
    x = src("SoundWaves__Weapons__sfx_Weapon_Shotgun_DryClick_nl", i)
    return normalize(end_trim(x), -6.0)


@bank("DryFire_Energy", 2)
def dry_energy(i, rng):
    m = tone(0.18, 210, 120, 0.15, 0.002, 0.06, harmonics=(1.0, 0.6, 0.4, 0.3))
    m = m + bp(noise(0.18, rng, 0.001, 0.03), 2000, 6000) * 0.3
    return normalize(fade(stereo(m.astype(np.float32))), -8.0)


@bank("Weapon_Raise", 3)
def weapon_raise(i, rng):
    x = trim(src("Foley_Misc__Lyra_WeaponSwap", i), 0.75, 0.2)
    x = mix(x, hp(src("Weapons__Rifle__Weapons_Rifle_Mech", i + 2), 600), 0.32, 0.5)
    return normalize(end_trim(x), -5.0)


# ---- Grenade --------------------------------------------------------------------------------------------------


@bank("Grenade_Throw", 4)
def grenade_throw(i, rng):
    n = int(0.35 * SR)
    nz = rng.uniform(-1, 1, n).astype(np.float32)
    t = np.arange(n) / SR
    w = np.zeros(n, np.float32)
    for k, seg in enumerate(np.array_split(np.arange(n), 14)):
        f = 500 * (2200 / 500) ** np.sin(np.pi * k / 13)
        w[seg] = bp(nz, f * 0.7, f * 1.4)[seg]
    w *= np.sin(np.pi * np.clip(t / 0.3, 0, 1)) ** 2
    x = stereo(w, 0.0008) * 0.8
    x = mix(x, foley(i + 6, 0.5, 0.3), 0.0)
    pin = ring(0.1, [(4200, 1.0), (6100, 0.5)], 0.015, rng)
    x = mix(x, stereo(pin), 0.0, 0.12)
    return normalize(end_trim(x), -4.0)


@bank("Grenade_Bounce", 6)
def grenade_bounce(i, rng):
    b = strip_silence_head(src("Grenade__grenade_bounce", i))
    b = trim(b, 0.45, 0.2)
    clank = ring(0.25, [(1130 * rng.uniform(0.95, 1.05), 1.0), (2710, 0.6), (4390, 0.3)], 0.04, rng)
    x = mix(b, stereo(clank), 0, 0.25 * np.max(np.abs(b)) / (np.max(np.abs(clank)) + 1e-9))
    return normalize(end_trim(to_mono(x)), -2.0)


def explosion_core(i, rng):
    x = silence(4.0)
    punch = src("Explosions__Explosions_Grenade_Punch-Close", i)
    x = mix(x, punch, 0, 1.0)
    x = mix(x, src("Grenade__grenade_explode_thump", i), 0, 0.7)
    x = mix(x, src("Grenade__grenade_explode_whack", i), 0, 0.55)
    x = mix(x, src("Grenade__grenade_explode_body", i), 0.01, 0.6)
    x = mix(x, src("Grenade__grenade_explode_sizzle", i), 0.15, 0.35)
    tail = src("Explosions__Explosions_Grenade_Noise-Exterior", i)
    x = mix(x, tail, 0.0, 0.55 * np.max(np.abs(punch)) / (np.max(np.abs(tail)) + 1e-9))
    # Original layers: a pressure crack and a long sub boom you feel more than hear.
    crack = bp(noise(0.04, rng, 0.0001, 0.006), 1500, 12000)
    x = mix(x, stereo(crack, 0.0004), 0, 0.6)
    x = mix(x, sub_thump(rng, 46, 22, 0.45, 1.6), 0, 1.0)
    # Debris: a scatter of small metallic ticks falling back to the ground.
    for k in range(10):
        tick = ring(0.08, [(rng.uniform(1800, 5200), 1.0), (rng.uniform(5500, 8000), 0.4)], 0.01, rng)
        x = mix(x, stereo(tick), 0.35 + rng.uniform(0, 1.1), 0.06 * rng.uniform(0.3, 1.0))
    x = peak_eq(x, 70, 4.0, 0.7)
    return saturate(normalize(x, 0), 1.5)


@bank("Grenade_Explosion", 5)
def grenade_explosion(i, rng):
    return normalize(end_trim(to_mono(explosion_core(i, rng)), -58), -0.5)


@bank("Grenade_Explosion_FP", 5)
def grenade_explosion_fp(i, rng):
    """Stereo version for an explosion right next to the listener (the game uses it within ~6 m)."""
    return normalize(end_trim(explosion_core(i, rng), -58), -0.5)


@bank("Grenade_Explosion_Dist", 4)
def grenade_explosion_dist(i, rng):
    x = silence(0.2)
    x = mix(x, src("Explosions__Explosions_Grenade_Punch-Distant", i), 0, 1.0)
    tail = src("Explosions__Explosions_Grenade_Noise-Exterior-Distant", i * 2)
    x = mix(x, tail, 0, 0.9 * np.max(np.abs(x)) / (np.max(np.abs(tail)) + 1e-9))
    x = mix(x, sub_thump(rng, 40, 24, 0.4, 1.4), 0, 0.5 * np.max(np.abs(x)))
    return normalize(end_trim(to_mono(lp(x, 2400)), -58), -1.0)


# ---- Shields: a "crystal lattice" energy field (all original synthesis) -------------------------------------------
# One family of sounds built from the same material, so the player learns it: a low field hum plus glassy lattice
# partials (A minor-ish, slightly inharmonic). Hits ping the lattice, low shield makes it strain and crackle, the break
# shatters it, the recharge rebuilds it note by note.

LATTICE = [(880.0, 1.0), (1318.5, 0.7), (1760.0 * 1.012, 0.45), (2637.0 * 0.994, 0.3), (3520.0 * 1.021, 0.18)]


def lattice(seconds, rng, decay, scale=1.0, attack=0.001):
    return ring(seconds, [(hz * scale * rng.uniform(0.995, 1.005), g) for hz, g in LATTICE], decay, rng, attack)


def field_hum(seconds, hz=110.0, attack=0.02, decay=0.3, hold=0.0):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    w = np.sin(2 * np.pi * hz * t) + 0.5 * np.sin(2 * np.pi * hz * 2.003 * t) + 0.25 * np.sin(2 * np.pi * hz * 3.01 * t)
    w *= 1 + 0.3 * np.sin(2 * np.pi * 31 * t)  # electric beating
    return (w * env(n, attack, decay, hold)).astype(np.float32)


def crackle(seconds, rng, density, lo=2500, hi=10000):
    n = int(seconds * SR)
    z = bp(rng.uniform(-1, 1, n).astype(np.float32), lo, hi)
    gate = signal.lfilter([1.0], [1.0, -0.99], (rng.uniform(0, 1, n) < density).astype(np.float32))
    return z * np.clip(gate, 0, 1)


@bank("Shield_Impact", 6)
def shield_impact(i, rng):
    """A round absorbed by the shield: a hard glassy 'tink' on the lattice with a short electric fizz and a soft thud."""
    scale = rng.uniform(0.9, 1.12)
    x = stereo(bp(noise(0.08, rng, 0.0002, 0.008), 4000, 14000), 0.0003) * 0.7
    x = mix(x, stereo(lattice(0.35, rng, 0.06, scale)), 0, 0.6)
    x = mix(x, stereo(crackle(0.15, rng, 0.05) * env(int(0.15 * SR), 0.002, 0.04)), 0.004, 0.45)
    x = mix(x, stereo(field_hum(0.15, 140 * scale, 0.002, 0.04)), 0, 0.3)
    x = mix(x, sub_thump(rng, 130, 70, 0.025, 0.12), 0, 0.25)
    return normalize(end_trim(to_mono(x)), -3.0)


@bank("Shield_Pop", 3)
def shield_pop(i, rng):
    """The lattice shattering: a bright crack, a cascade of glass shards falling in pitch, an electrical discharge and a
    bass drop. Loud and unmistakable from across the map."""
    x = silence(1.1)
    x = mix(x, stereo(bp(noise(0.06, rng, 0.0001, 0.01), 3000, 16000), 0.0004), 0, 1.0)
    x = mix(x, hp(src("Impacts__Lyra_ImpactGlass", i * 2 + 1), 1800), 0.0, 0.5)
    # Shard cascade: lattice pings stepping down in pitch, scattered in time and across the stereo field.
    for k in range(9):
        at = 0.01 + k * 0.028 + rng.uniform(0, 0.012)
        ping = lattice(0.25, rng, 0.04, 1.5 * (0.88 ** k))
        side = rng.uniform(-0.6, 0.6)
        st = np.stack([ping * (1 - side), ping * (1 + side)], axis=1)
        x = mix(x, st, at, 0.35 * (0.92 ** k))
    discharge = crackle(0.6, rng, 0.06, 1200, 8000) * env(int(0.6 * SR), 0.005, 0.18)
    x = mix(x, stereo(discharge, 0.0006), 0.01, 0.7)
    drop = tone(0.6, 220, 38, 0.35, 0.002, 0.2, harmonics=(1.0, 0.4))
    x = mix(x, stereo(saturate(drop, 1.8)), 0, 0.8)
    return normalize(end_trim(saturate(x, 1.3)), -1.0)


@bank("Shield_Recharge", 1)
def shield_recharge(i, rng):
    """The lattice rebuilding: the field hum swells while glassy notes step up an arpeggio and speed up, a shimmer
    grows over the top, then everything locks with a bright 'crystallise' chord."""
    dur = 2.0
    x = silence(dur)
    n = int(1.45 * SR)
    t = np.arange(n) / SR
    prog = t / 1.45
    hum = field_hum(1.45, 1.0, 0.0, 10.0)  # placeholder shape; real hum below with a pitch rise
    ph = 2 * np.pi * np.cumsum(98 * (1 + 0.6 * prog ** 1.5)) / SR
    hum = (np.sin(ph) + 0.5 * np.sin(2.003 * ph) + 0.3 * np.sin(3.01 * ph)) * (0.15 + 0.85 * prog ** 1.3)
    hum *= 1 + 0.25 * np.sin(2 * np.pi * (8 + 30 * prog) * t)
    x = mix(x, stereo(lp(hum.astype(np.float32), 2500), 0.0005), 0, 0.5)
    notes = [440.0, 523.3, 659.3, 880.0, 1046.5, 1318.5, 1760.0, 2093.0]
    at = 0.0
    for k in range(14):
        hz = notes[k % len(notes)] * (2 if k >= len(notes) else 1)
        ping = ring(0.35, [(hz, 1.0), (hz * 2.01, 0.35), (hz * 3.03, 0.15)], 0.08, rng, attack=0.002)
        side = 0.5 * np.sin(k * 1.7)
        x = mix(x, np.stack([ping * (1 - side), ping * (1 + side)], axis=1), at, 0.18 + 0.12 * k / 13)
        at += 0.16 * (1 - 0.055 * k)
    shimmer = crackle(1.45, rng, 0.004, 5000, 14000) * (prog ** 2)
    x = mix(x, stereo(shimmer, 0.0007), 0, 0.25)
    lock = ring(0.7, [(1318.5, 1.0), (1760.0, 0.8), (2637.0, 0.5), (3520.0, 0.3)], 0.2, rng, attack=0.003)
    x = mix(x, stereo(lock, 0.0004), 1.42, 0.55)
    x = mix(x, stereo(bp(noise(0.05, rng, 0.0005, 0.012), 5000, 14000)), 1.42, 0.3)
    wet = convolve(x, room_ir(rng, 0.6))
    return normalize(end_trim(mix(wet * 0.25, x, 0.0)), -4.0)


@bank("Shield_LowWarn", 2)
def shield_low_warn(i, rng):
    """Shield below ~40 %: danger. A heavy heartbeat (lub-dub) under a tense tritone pulse and lattice crackle, like
    the field is failing. Repeats while low and the game speeds it up as the shield drops."""
    x = silence(0.8)
    for at, hz, g in ((0.0, 58.0, 1.0), (0.17, 52.0, 0.75)):
        beat = tone(0.25, hz * 1.4, hz, 0.04, 0.004, 0.07, harmonics=(1.0, 0.3))
        x = mix(x, stereo(saturate(beat, 2.0)), at, g)
        x = mix(x, stereo(lp(noise(0.06, rng, 0.002, 0.02), 400)), at, 0.25 * g)
    # Tense dissonant pulse: a tritone that swells in and is cut short.
    n = int(0.5 * SR)
    t = np.arange(n) / SR
    pulse = (np.sin(2 * np.pi * 311.1 * t) + 0.8 * np.sin(2 * np.pi * 440.0 * t) + 0.3 * np.sin(2 * np.pi * 622.3 * t))
    pulse *= np.sin(np.pi * np.clip(t / 0.45, 0, 1)) ** 2 * (1 + 0.5 * np.sin(2 * np.pi * 14 * t))
    x = mix(x, stereo(lp(pulse.astype(np.float32), 2200), 0.0006), 0.05, 0.35)
    x = mix(x, stereo(crackle(0.5, rng, 0.02, 2000, 9000) * env(int(0.5 * SR), 0.03, 0.12), 0.0005), 0.02, 0.4)
    return normalize(end_trim(x), -5.0)


@bank("Shield_Low", 1)
def shield_low(i, rng):
    """Shield empty: an urgent, clipped alarm. Three fast descending chirps over a buzz, repeated by the game."""
    x = silence(0.32)
    for k, hz in enumerate((1760.0, 1480.0, 1245.0)):
        chirp = tone(0.07, hz * 1.06, hz, 0.02, 0.003, 0.025, harmonics=(1.0, 0.45, 0.2))
        x = mix(x, stereo(chirp), k * 0.075, 1.0)
    buzz = tone(0.26, 110, 105, 0.2, 0.005, 0.12, harmonics=(1.0, 0.7, 0.5, 0.35, 0.25))
    x = mix(x, stereo(lp(buzz, 1800)), 0, 0.35)
    return normalize(end_trim(x), -5.0)


# ---- Armoured footsteps ---------------------------------------------------------------------------------------


@bank("Footstep", 10)
def footstep(i, rng):
    side = "L" if i % 2 == 0 else "R"
    x = src("Movement__Movement_Footstep-Default_" + side, i // 2)
    x = pitch(x, 0.88 * rng.uniform(0.97, 1.03))
    x = mix(x, sub_thump(rng, 95, 60, 0.03, 0.15), 0, 0.35 * np.max(np.abs(x)))
    clank = ring(0.18, [(rng.uniform(380, 460), 1.0), (rng.uniform(900, 1050), 0.6), (rng.uniform(1600, 1900), 0.35)], 0.03, rng)
    x = mix(x, stereo(clank), 0.004, 0.18 * np.max(np.abs(x)) / (np.max(np.abs(clank)) + 1e-9))
    x = mix(x, foley(i, 0.25 * np.max(np.abs(x)) / 0.4, 0.22), 0.01)
    return normalize(end_trim(to_mono(x)), -3.0)


@bank("Land", 4)
def land(i, rng):
    x = pitch(src("Movement__Movement_Land-Generic", i), 0.85)
    x = mix(x, sub_thump(rng, 80, 42, 0.07, 0.35), 0, 0.6 * np.max(np.abs(x)))
    clank = ring(0.3, [(rng.uniform(300, 360), 1.0), (rng.uniform(760, 850), 0.6), (1450, 0.35), (2620, 0.2)], 0.05, rng)
    x = mix(x, stereo(clank), 0.006, 0.25 * np.max(np.abs(x)) / (np.max(np.abs(clank)) + 1e-9))
    x = mix(x, foley(i + 2, 0.45 * np.max(np.abs(x)) / 0.4, 0.35), 0.015)
    return normalize(end_trim(to_mono(x)), -2.0)


# ---------------------------------------------------------------------------------------------------------------


def main():
    preview = "--preview" in sys.argv
    only = [a for a in sys.argv[1:] if not a.startswith("--")]
    os.makedirs(OUT, exist_ok=True)
    manifest = {}
    for name, (count, fn) in BANKS.items():
        if only and not any(name.startswith(o) for o in only):
            continue
        for old in glob.glob(os.path.join(OUT, "SFX_%s_[0-9][0-9].wav" % name)):
            os.remove(old)
        lengths = []
        for i in range(count):
            rng = np.random.default_rng(zlib.crc32(name.encode()) + i * 7919)
            x = fn(i, rng)
            mono = np.allclose(x[:, 0], x[:, 1])
            path = os.path.join(OUT, "SFX_%s_%02d.wav" % (name, i + 1))
            write_wav(path, x[:, :1] if mono else x)
            lengths.append(round(len(x) / SR, 3))
        manifest[name] = {"count": count, "seconds": lengths, "mono": bool(mono)}
        print("%-24s x%d  %s  %.2f-%.2f s" % (name, count, "mono" if mono else "stereo", min(lengths), max(lengths)))
    if not only:
        with open(os.path.join(OUT, "manifest.json"), "w") as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
    if preview:
        make_preview()


def make_preview():
    """One MP3 per bank (all variations back to back) plus a few in-context sequences, for listening on a phone."""
    os.makedirs(PREVIEW, exist_ok=True)
    seqs = {
        "01_battle_rifle_bursts": [("BR_FP", 0.0), ("BR_FP", 0.055), ("BR_FP", 0.11), ("BR_FPTail", 0.11),
                                   ("BR_FP", 0.53), ("BR_FP", 0.585), ("BR_FP", 0.64), ("BR_FPTail", 0.64),
                                   ("BR_FP", 1.06), ("BR_FP", 1.115), ("BR_FP", 1.17), ("BR_FPTail", 1.17)],
        "02_sidearm": [("Sidearm_FP", k * 0.26) for k in range(5)] + [("Sidearm_FPTail", 1.04)],
        "03_sniper": [("Sniper_FP", 0), ("Sniper_FPTail", 0), ("Sniper_Bolt", 0.33), ("Sniper_FP", 1.0), ("Sniper_FPTail", 1.0), ("Sniper_Bolt", 1.33)],
        "04_rocket": [("Rocket_FP", 0), ("Rocket_FPTail", 0), ("Grenade_Explosion", 1.1)],
        "05_rail_lance": [("Rail_Charge", 0), ("Rail_FP", 0.95), ("Rail_FPTail", 0.95)],
        "06_grenade": [("Grenade_Throw", 0), ("Grenade_Bounce", 0.45), ("Grenade_Bounce", 0.8), ("Grenade_Bounce", 1.0), ("Grenade_Explosion_FP", 1.6)],
        "07_distant_battle": [("BR_Dist", 0), ("BR_Dist", 0.06), ("BR_Dist", 0.12), ("Sniper_Dist", 0.9), ("Grenade_Explosion_Dist", 1.6), ("Sidearm_Dist", 2.6), ("Rail_Dist", 3.2)],
        "08_shields": [("Shield_Impact", 0), ("Shield_Impact", 0.18), ("Shield_Impact", 0.36), ("Shield_LowWarn", 0.7), ("Shield_LowWarn", 1.7), ("Shield_Impact", 2.4), ("Shield_LowWarn", 2.55), ("Shield_Pop", 3.4), ("Shield_Low", 4.4), ("Shield_Low", 4.95), ("Shield_Low", 5.5), ("Shield_Recharge", 6.3)],
        "09_footsteps": [("Footstep", k * 0.33) for k in range(8)] + [("Land", 2.9)],
        "10_reloads": [("BR_Reload", 0), ("Sidearm_Reload", 2.3), ("Sniper_Reload", 4.0), ("DryFire_Rifle", 7.0), ("DryFire_Light", 7.4), ("DryFire_Heavy", 7.8), ("Weapon_Raise", 8.4)],
    }
    used = {}
    for title, events in seqs.items():
        x = silence(0.1)
        for name, at in events:
            k = used.get(name, 0)
            used[name] = k + 1
            files = sorted(glob.glob(os.path.join(OUT, "SFX_%s_[0-9][0-9].wav" % name)))
            y = read_wav(files[k % len(files)])
            gain = 0.55 if "Tail" in name else 1.0
            x = mix(x, y, at + 0.1, gain)
        x = normalize(np.concatenate([x, silence(0.4)]), -1.0)
        wav = os.path.join(PREVIEW, title + ".wav")
        write_wav(wav, x)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-b:a", "192k", wav[:-4] + ".mp3"], check=True)
        os.remove(wav)
    print("previews in", PREVIEW)


if __name__ == "__main__":
    main()
