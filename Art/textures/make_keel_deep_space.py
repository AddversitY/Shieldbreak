"""Textures for Keel's deep-space windows (Tools/create_keel_deep_space.py): original and procedural.

    python Art/textures/make_keel_deep_space.py      (numpy, Pillow)

T_SB_KeelNebula.png  1024 x 1024, tileable. RGB = a cold violet-teal nebula (two colour fields over FFT noise), A = a
                     generic grey noise (the moon's surface, debris rim variation).
T_SB_KeelBands.png   1024 x 256. The gas giant's cloud bands: rows are latitude, columns longitude (tileable in X),
                     soft banding with turbulent edges, a pale storm oval.
T_SB_KeelDebris.png  1024 x 1024, tileable. A = silhouettes: a few distant hull fragments and two ship hulls, scattered
                     sparsely; G = a lit rim along their upper-left edges.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures")
RNG = np.random.default_rng(20261126)


def fft_noise(h, w, power, lo=0.0):
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    spec = np.fft.fft2(RNG.standard_normal((h, w))) / f ** power
    spec[f < lo] = 0
    out = np.real(np.fft.ifft2(spec))
    out -= out.min()
    return out / out.max()


def save(arr, name, mode):
    Image.fromarray(np.clip(arr * 255.0 + 0.5, 0, 255).astype(np.uint8), mode).save(os.path.join(OUT, name))
    print("wrote", name)


def nebula():
    n = 1024
    base = fft_noise(n, n, 1.7)
    detail = fft_noise(n, n, 1.1, lo=4.0 / n)
    hue = fft_noise(n, n, 2.0)
    dens = np.clip((base * 0.8 + detail * 0.35 - 0.45) * 2.2, 0.0, 1.0) ** 1.6
    violet = np.array([0.42, 0.16, 0.62])
    teal = np.array([0.10, 0.42, 0.55])
    rose = np.array([0.55, 0.20, 0.32])
    t = hue[..., None]
    colour = violet * (1 - t) + teal * t
    colour = colour * 0.8 + rose * 0.2 * (detail[..., None] > 0.7)
    rgb = colour * dens[..., None]
    grey = fft_noise(n, n, 1.3)
    rgba = np.dstack([rgb, grey])
    save(rgba, "T_SB_KeelNebula.png", "RGBA")


def bands():
    w, h = 1024, 256
    lat = np.linspace(-1, 1, h)[:, None]
    turb = fft_noise(h, w, 1.5)
    turb2 = fft_noise(h, w, 1.0, lo=8.0 / w)
    warped = lat + (turb - 0.5) * 0.10 + (turb2 - 0.5) * 0.03
    stripes = 0.5 + 0.5 * np.sin(warped * 19.0) * np.cos(warped * 7.0)
    fine = 0.5 + 0.5 * np.sin(warped * 61.0 + turb * 3.0)
    cream = np.array([0.86, 0.76, 0.60])
    amber = np.array([0.72, 0.46, 0.26])
    slate = np.array([0.42, 0.40, 0.48])
    s = stripes[..., None]
    colour = cream * s + amber * (1 - s)
    colour = colour * (0.85 + 0.15 * fine[..., None])
    polar = np.clip((np.abs(lat) - 0.62) / 0.38, 0, 1)[..., None]
    colour = colour * (1 - polar) + slate * polar
    # a pale storm oval in the southern belt
    yy, xx = np.mgrid[0:h, 0:w]
    oval = np.exp(-(((xx - 700) / 46.0) ** 2 + ((yy - 160) / 11.0) ** 2))
    colour = colour * (1 - oval[..., None] * 0.6) + np.array([0.95, 0.88, 0.78]) * oval[..., None] * 0.6
    save(colour, "T_SB_KeelBands.png", "RGB")


def debris():
    n = 1024
    mask = Image.new("L", (n, n), 0)
    rim = Image.new("L", (n, n), 0)
    dm, dr = ImageDraw.Draw(mask), ImageDraw.Draw(rim)
    rnd = np.random.default_rng(5)

    def shape(cx, cy, pts, scale, angle):
        ca, sa = math.cos(angle), math.sin(angle)
        out = []
        for x, y in pts:
            x, y = x * scale, y * scale
            out.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
        return out

    # Ship hulls: a long wedge with an engine block, and a ring-and-spine hull. Original silhouettes.
    ship_a = [(-60, 0), (-40, -6), (30, -9), (55, -3), (62, 0), (55, 3), (30, 8), (-40, 6), (-60, 2)]
    ship_a_fins = [(-20, -6), (-8, -16), (4, -8)]
    ship_b = [(-40, -2), (40, -2), (40, 2), (-40, 2)]
    pieces = []
    pieces.append((shape(300, 380, ship_a, 1.6, -0.06), shape(300, 380, ship_a_fins, 1.6, -0.06)))
    pieces.append((shape(760, 700, ship_b, 1.2, 0.12), None))
    for i in range(14):
        cx, cy = rnd.uniform(40, n - 40), rnd.uniform(40, n - 40)
        k = rnd.integers(5, 9)
        pts = [(math.cos(2 * math.pi * j / k) * rnd.uniform(4, 14), math.sin(2 * math.pi * j / k) * rnd.uniform(3, 10)) for j in range(k)]
        pieces.append((shape(cx, cy, pts, rnd.uniform(0.6, 1.6), rnd.uniform(0, 6.28)), None))
    for body, extra in pieces:
        dm.polygon(body, fill=255)
        if extra:
            dm.polygon(extra, fill=255)
    # ship_b: a ring around its spine
    dm.ellipse((760 - 22, 700 - 22, 760 + 22, 700 + 22), outline=255, width=5)
    a = np.asarray(mask, np.float32) / 255.0
    shifted = np.roll(np.roll(a, 2, axis=0), 2, axis=1)
    rim_arr = np.clip(a - shifted, 0, 1)
    rim_img = Image.fromarray((rim_arr * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6))
    a_img = mask.filter(ImageFilter.GaussianBlur(0.7))
    out = np.dstack([np.zeros((n, n)), np.asarray(rim_img, np.float32) / 255.0, np.zeros((n, n)),
                     np.asarray(a_img, np.float32) / 255.0])
    save(out, "T_SB_KeelDebris.png", "RGBA")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    nebula()
    bands()
    debris()
