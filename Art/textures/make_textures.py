#!/usr/bin/env python3
"""Procedural, tileable greyscale detail textures for the world materials (M_SB_World, see Tools/import_art.py).

The map palette colour tints them in the material (BaseColor = Tint * Detail * 2), so they average ~0.5 grey and
only carry the pattern: Forerunner-style metal panels, floor tiles, rock and grass. Writes Art/Export/Textures/*.png.

    python3 Art/textures/make_textures.py
"""
import os

import numpy as np
from PIL import Image

N = 512
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export", "Textures")
rng = np.random.default_rng(7)


def noise(scale, power=2.0):
    """Tileable fractal noise in [0, 1]: white noise filtered in the frequency domain (periodic by construction)."""
    f = np.fft.fftfreq(N)
    k = np.sqrt(f[:, None] ** 2 + f[None, :] ** 2)
    k[0, 0] = 1.0
    spectrum = np.fft.fft2(rng.standard_normal((N, N))) / (k * scale) ** power
    spectrum[0, 0] = 0
    out = np.real(np.fft.ifft2(spectrum))
    return (out - out.min()) / (out.max() - out.min())


def coords():
    y, x = np.mgrid[0:N, 0:N]
    return x.astype(float), y.astype(float)


def box_mask(x, y, x0, y0, x1, y1, chamfer=0.0):
    """1 inside the axis-aligned box with 45 degree chamfered corners."""
    inside = (x >= x0) & (x < x1) & (y >= y0) & (y < y1)
    if chamfer > 0:
        inside &= (x - x0) + (y - y0) >= chamfer
        inside &= (x1 - x) + (y - y0) >= chamfer
        inside &= (x - x0) + (y1 - y) >= chamfer
        inside &= (x1 - x) + (y1 - y) >= chamfer
    return inside


def edge_distance(mask):
    """Rough distance (px) to the mask's edge, inside the mask, capped at 12."""
    d = np.zeros(mask.shape)
    cur = mask.copy()
    for i in range(12):
        d += cur
        cur = cur & np.roll(cur, 1, 0) & np.roll(cur, -1, 0) & np.roll(cur, 1, 1) & np.roll(cur, -1, 1)
    return d


def panel():
    """Two by two big chamfered plates with grooves between, an inset plate on two of them and fine seam lines."""
    x, y = coords()
    img = np.full((N, N), 0.50)
    h = N // 2
    for (px, py), inset in (((0, 0), True), ((h, 0), False), ((0, h), False), ((h, h), True)):
        plate = box_mask(x, y, px + 5, py + 5, px + h - 5, py + h - 5, chamfer=34)
        tone = 0.50 + rng.uniform(-0.04, 0.04)
        img = np.where(plate, tone, img)
        bevel = edge_distance(plate)
        img += np.where(plate & (bevel < 4), 0.10, 0.0)  # lit bevel
        if inset:
            inner = box_mask(x, y, px + 60, py + 60, px + h - 60, py + h - 60, chamfer=26)
            ring = edge_distance(inner)
            img = np.where(inner, tone - 0.06, img)
            img -= np.where(inner & (ring < 3), 0.16, 0.0)
        else:
            # two thin horizontal seams across the plate
            for sy in (py + h // 3, py + 2 * h // 3):
                img -= np.where(plate & (abs(y - sy) < 1.5) & (x > px + 40) & (x < px + h - 40), 0.14, 0.0)
    grooves = ~(box_mask(x, y, 0, 0, N, N) & ((x % h) >= 5) & ((x % h) < h - 5) & ((y % h) >= 5) & ((y % h) < h - 5))
    img = np.where(grooves, 0.18, img)
    img += (noise(0.02, 1.2) - 0.5) * 0.06
    return img


def tile():
    """Four by four floor tiles with dark grout and a little stone variation per tile."""
    x, y = coords()
    s = N // 4
    tone = rng.uniform(0.44, 0.56, (4, 4))
    img = tone[(y // s).astype(int), (x // s).astype(int)]
    img = img + (noise(0.015, 1.6) - 0.5) * 0.18
    grout = ((x % s) < 4) | ((y % s) < 4)
    img = np.where(grout, 0.22, img)
    return img


def stone():
    big = noise(0.005, 2.2)
    mid = noise(0.02, 1.8)
    fine = noise(0.08, 1.0)
    img = 0.5 + (big - 0.5) * 0.28 + (mid - 0.5) * 0.32 + (fine - 0.5) * 0.22
    # dark cracks where the mid-frequency noise crosses 0.5
    crack = np.abs(noise(0.03, 1.6) - 0.5) < 0.01
    return np.where(crack, img * 0.75, img)


def grass():
    clumps = noise(0.006, 2.0)
    blades = noise(0.25, 0.6)
    return 0.5 + (clumps - 0.5) * 0.35 + (blades - 0.5) * 0.35


def save(name, img):
    img = np.clip(img, 0, 1)
    img = img * (0.5 / img.mean())  # keep the average at mid-grey so the palette tint sets the brightness
    rgb = (np.clip(img, 0, 1) * 255).astype(np.uint8)
    Image.fromarray(np.stack([rgb] * 3, -1), "RGB").save(os.path.join(OUT, "T_SB_%s.png" % name))
    print("wrote T_SB_%s.png (mean %.2f)" % (name, img.mean()))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, make in (("Panel", panel), ("Tile", tile), ("Stone", stone), ("Grass", grass)):
        save(name, make())
