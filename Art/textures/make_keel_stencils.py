"""Keel's landmark stencils: big painted bridge numbers ("01" red, "02" blue) and a hazard chevron band for the trench.

Original artwork: the digits are drawn from rectangles (a stencil-cut block face with bridges in the counters), no
fonts. Alpha is the paint coverage, broken up by wear (scratches, chips and a soft grime falloff), so the decals read
as old paint on the hull rather than stickers. RGB is white for the numbers (tinted in the material) and
orange/black for the chevrons.

    python Art/textures/make_keel_stencils.py      (numpy, Pillow)
Writes Art/Export/Textures/T_SB_KeelStencil01.png, T_SB_KeelStencil02.png, T_SB_KeelChevrons.png.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures")
RNG = np.random.default_rng(1402)

# Segments on a 6 x 10 grid (x0, y0, x1, y1), stencil bridges left as gaps between them.
DIGITS = {
    "0": [(0, 0, 6, 1.6), (0, 8.4, 6, 10), (0, 1.9, 1.6, 8.1), (4.4, 1.9, 6, 8.1)],
    "1": [(2.2, 0, 3.8, 10), (1.0, 0, 2.0, 1.6), (1.0, 8.4, 5.0, 10)],
    "2": [(0, 0, 6, 1.6), (4.4, 1.9, 6, 4.2), (0, 4.4, 6, 5.8), (0, 6.0, 1.6, 8.1), (0, 8.4, 6, 10)],
}


def wear(size, strength=1.0):
    h, w = size
    noise = Image.fromarray((RNG.random((h // 8, w // 8)) * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
    noise = np.asarray(noise.filter(ImageFilter.GaussianBlur(6)), np.float32) / 255.0
    fine = np.asarray(Image.fromarray((RNG.random((h // 2, w // 2)) * 255).astype(np.uint8)).resize((w, h)), np.float32) / 255.0
    scratches = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(scratches)
    for _ in range(int(60 * strength)):
        x, y = RNG.integers(0, w), RNG.integers(0, h)
        length, angle = RNG.uniform(20, 160), RNG.uniform(-0.5, 0.5)
        d.line((x, y, x + length * np.cos(angle), y + length * np.sin(angle)), fill=255, width=int(RNG.integers(1, 3)))
    scratches = np.asarray(scratches, np.float32) / 255.0
    # Mostly solid paint: chips only where the coarse noise is low, a light grain, and fine scratches.
    keep = np.clip((noise - 0.22) * 5.0, 0.0, 1.0) * (0.88 + 0.12 * fine)
    return np.clip(keep * (1.0 - 0.9 * scratches * 0.6), 0.0, 1.0)


def number(text, path, size=(1024, 1024)):
    w, h = size
    mask = Image.new("L", size, 0)
    d = ImageDraw.Draw(mask)
    cell, gap = 75, 40
    total = len(text) * 6 * cell + (len(text) - 1) * gap
    x0, y0 = (w - total) // 2, (h - 10 * cell) // 2
    for i, ch in enumerate(text):
        ox = x0 + i * (6 * cell + gap)
        for a, b, c, e in DIGITS[ch]:
            d.rectangle((ox + a * cell, y0 + b * cell, ox + c * cell - 1, y0 + e * cell - 1), fill=255)
    alpha = np.asarray(mask.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0 * wear((h, w))
    rgba = np.dstack([np.full((h, w), 235, np.uint8)] * 3 + [(alpha * 255).astype(np.uint8)])
    # Stored upside down: the wall decals project it turned half a turn (in-game shots k15/k16).
    Image.fromarray(np.ascontiguousarray(rgba[::-1, ::-1]), "RGBA").save(path, optimize=True)


def chevrons(path, size=(2048, 256)):
    w, h = size
    colour = Image.new("RGB", size, (20, 20, 22))
    d = ImageDraw.Draw(colour)
    stripe = 128
    for x in range(-h, w + h, 2 * stripe):
        d.polygon([(x, h), (x + stripe, h), (x + stripe + h, 0), (x + h, 0)], fill=(214, 118, 18))
    alpha = wear((h, w), 0.6) * 0.95
    border = np.ones((h, w), np.float32)
    border[:10, :] = 0.0
    border[-10:, :] = 0.0
    rgba = np.dstack([np.asarray(colour, np.uint8), (alpha * border * 255).astype(np.uint8)])
    Image.fromarray(rgba, "RGBA").save(path, optimize=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    number("01", os.path.join(OUT, "T_SB_KeelStencil01.png"))
    number("02", os.path.join(OUT, "T_SB_KeelStencil02.png"))
    chevrons(os.path.join(OUT, "T_SB_KeelChevrons.png"))
    print("stencils written")


main()
