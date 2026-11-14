"""Offline replica of ShieldbreakAmbience's Hum, VentFan and ReactorDrone loops: peak and RMS after MakeLoop's
normalisation and the AddLoop volume, in dBFS. Noise sequences differ from FRandomStream but the statistics match."""
import numpy as np

RATE = 24000
rng = np.random.default_rng(1)


def lowpass(x, hz):
    c = 1 - np.exp(-2 * np.pi * max(hz, 1) / RATE)
    y = np.empty_like(x); s = 0.0
    for i, v in enumerate(x):
        s += c * (v - s); y[i] = s
    return y


def make_loop(seconds, peak, raw):
    n = int(round(seconds * RATE)); fade = RATE // 2
    a = np.arange(fade) / fade
    raw = raw.copy(); raw[:fade] = raw[:fade] * a + raw[n:n + fade] * (1 - a)
    raw = raw[:n]
    return raw / np.abs(raw).max() * peak


def hum():
    n = 8 * RATE + RATE // 2; t = np.arange(n) / RATE
    wob = 0.85 + 0.15 * np.sin(2 * np.pi * 0.25 * t)
    tone = np.sin(2 * np.pi * 55 * t) + 0.45 * np.sin(2 * np.pi * 110 * t) + 0.15 * np.sin(2 * np.pi * 165 * t)
    return make_loop(8, 0.6, 0.5 * tone * wob + 1.6 * lowpass(rng.uniform(-1, 1, n), 260))


def fan():
    n = 4 * RATE + RATE // 2; t = np.arange(n) / RATE
    w = rng.uniform(-1, 1, n)
    rush = lowpass(w, 900) - 0.6 * lowpass(w, 150)
    blades = 0.65 + 0.35 * np.sin(2 * np.pi * 3 * t)
    whine = 0.12 * np.sin(2 * np.pi * 180 * t) + 0.05 * np.sin(2 * np.pi * 360 * t)
    return make_loop(4, 0.7, 1.4 * rush * blades + whine)


def drone():
    n = 8 * RATE + RATE // 2; t = np.arange(n) / RATE
    swell = 0.7 + 0.3 * np.sin(2 * np.pi * 0.25 * t)
    tone = (np.sin(2 * np.pi * 41 * t) + 0.6 * np.sin(2 * np.pi * 82 * t) + 0.2 * np.sin(2 * np.pi * 123 * t)
            + 0.08 * np.sin(2 * np.pi * 492 * t))
    return make_loop(8, 0.7, (0.55 * tone + 1.2 * lowpass(rng.uniform(-1, 1, n), 120)) * swell)


def db(v):
    return 20 * np.log10(max(v, 1e-9))


for name, wave, volume in (('hum (everywhere)', hum(), 0.22), ('fan (each, at 1.5 m)', fan(), 0.28),
                           ('reactor drone (at 4 m)', drone(), 0.2)):
    out = wave * volume
    print(f'{name:26s} peak {db(np.abs(out).max()):6.1f} dBFS  rms {db(np.sqrt((out ** 2).mean())):6.1f} dBFS')
