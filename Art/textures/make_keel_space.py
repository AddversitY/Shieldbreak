"""Keel's view out of the Breach: an original, procedural space panorama (no ring, no real planet).

A gas giant with soft cloud bands, lit from one side, a thin cold nebula and a star field over near-black blue.
Seen through the Breach's hull window as an emissive panel with parallax, so it reads as far away.

    python Art/textures/make_keel_space.py      (numpy, Pillow)
Writes Art/Export/Textures/T_SB_KeelSpace.png (4096 x 1024).
"""
import os

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures", "T_SB_KeelSpace.png")
W, H = 4096, 1024
RNG = np.random.default_rng(7351)


def smooth_noise(scale, blur):
    small = RNG.random((max(2, H // scale), max(2, W // scale))).astype(np.float32)
    image = Image.fromarray((small * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(image, np.float32) / 255.0


def main():
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    sky = np.zeros((H, W, 3), np.float32)
    sky += np.array([0.002, 0.003, 0.007], np.float32)
    # Nebula: two noise octaves, cold teal into violet, faint.
    cloud = np.clip(smooth_noise(64, 24) * 0.7 + smooth_noise(16, 6) * 0.3 - 0.45, 0, 1) ** 1.6
    hue = smooth_noise(128, 40)[..., None]
    sky += cloud[..., None] * (hue * np.array([0.05, 0.16, 0.20]) + (1 - hue) * np.array([0.14, 0.07, 0.20])) * 0.55
    # Stars: many faint, a few bright with a soft halo.
    stars = np.zeros((H, W), np.float32)
    count = 9000
    sx, sy = RNG.integers(0, W, count), RNG.integers(0, H, count)
    stars[sy, sx] = RNG.random(count) ** 6 * 1.6 + 0.08
    bright = Image.fromarray(np.clip(stars * 255, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.1))
    stars = np.maximum(stars * 0.9, np.asarray(bright, np.float32) / 255.0 * 2.2)
    sky += stars[..., None] * np.array([0.85, 0.9, 1.0])
    # Gas giant: centre right of the panorama, partly below the frame, banded, lit from the left.
    cx, cy, r = W * 0.68, H * 1.05, H * 0.82
    dx, dy = (x - cx) / r, (y - cy) / r
    d2 = dx * dx + dy * dy
    disc = d2 < 1.0
    nz = np.sqrt(np.clip(1.0 - d2, 0, 1))
    light = np.clip(-dx * 0.9 - dy * 0.25 + nz * 0.25, 0, 1) ** 1.6
    lat = dy + 0.012 * np.sin(dx * 7.0 + smooth_noise(48, 14) * 3.0)
    bands = np.clip(0.5 + 0.45 * np.sin(lat * 26.0) + 0.2 * np.sin(lat * 71.0) + (smooth_noise(24, 6) - 0.5) * 0.15, 0, 1)
    planet = (np.array([0.42, 0.30, 0.20])[None, None] * bands[..., None]
              + np.array([0.12, 0.16, 0.20])[None, None] * (1 - bands[..., None]))
    planet = planet * (light[..., None] * 1.6 + 0.015)
    # Atmosphere rim on the lit side.
    rim = np.exp(-((np.sqrt(d2) - 1.0) / 0.012) ** 2) * np.clip(-dx * 0.8 - dy * 0.3 + 0.2, 0, 1)
    sky = np.where(disc[..., None], planet, sky) + rim[..., None] * np.array([0.35, 0.55, 0.85])
    image = np.clip(sky ** (1 / 2.2), 0, 1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray((image * 255).astype(np.uint8)).save(OUT, optimize=True)
    print("space panorama written", OUT)


main()
