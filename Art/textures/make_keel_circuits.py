"""Keel's circuit-glass panels: an original procedural circuit-trace pattern (traces with 45-degree bends, pads and
vias), as a glow mask for the alien glass panels set high on the walls.

R channel = glow (traces 1, field 0, soft halo); G = faint field texture for the dark glass.

    python Art/textures/make_keel_circuits.py   -> Art/Export/Textures/T_SB_KeelCircuits.png (1024 x 1024)
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures", "T_SB_KeelCircuits.png")
RNG = np.random.default_rng(1117)
W = H = 1024
DIRS = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]


def trace(d, x, y):
    k = int(RNG.integers(0, 8))
    pts = [(x, y)]
    for _ in range(int(RNG.integers(3, 7))):
        dx, dy = DIRS[k]
        L = RNG.uniform(40, 160)
        x, y = x + dx * L, y + dy * L
        pts.append((x, y))
        k = (k + RNG.choice([-1, 0, 1])) % 8
    w = int(RNG.choice([4, 6, 8]))
    d.line(pts, fill=255, width=w, joint="curve")
    for p in (pts[0], pts[-1]):
        r = w * 1.6
        d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), outline=255, width=max(2, w // 2))


def main():
    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    for _ in range(22):
        trace(d, RNG.uniform(60, W - 60), RNG.uniform(60, H - 60))
    halo = mask.filter(ImageFilter.GaussianBlur(10))
    core = np.asarray(mask, np.float32) / 255.0
    glow = np.clip(core + 0.35 * np.asarray(halo, np.float32) / 255.0, 0, 1)
    field = np.asarray(Image.fromarray((RNG.random((H // 32, W // 32)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255.0
    rgb = np.stack([glow, field * 0.5 + 0.25, np.zeros_like(glow)], -1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray((rgb * 255).astype(np.uint8)).save(OUT, optimize=True)
    print("written", OUT)


main()
