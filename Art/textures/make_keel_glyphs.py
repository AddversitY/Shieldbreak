"""Keel's alien glyph panels: an original procedural glyph script (no game's glyphs), as a glow mask.

Clusters of rounded strokes with nodes and short branches, laid out in loose columns like an alien circuit
script. R channel = glow mask (strokes 1, field 0) with a soft halo; G = a faint field texture for the dark panel.

    python Art/textures/make_keel_glyphs.py      -> Art/Export/Textures/T_SB_KeelGlyphs.png (1024 x 2048)
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures", "T_SB_KeelGlyphs.png")
RNG = np.random.default_rng(904)
W, H = 1024, 2048


def glyph(d, cx, cy, s):
    """One glyph: a spine with 2-4 bends, rounded caps, a node ring and a branch or two."""
    pts = [(cx, cy)]
    for _ in range(RNG.integers(2, 5)):
        dx, dy = RNG.choice([(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1)])
        L = s * RNG.uniform(0.5, 1.2)
        pts.append((pts[-1][0] + dx * L, pts[-1][1] + dy * L))
    w = max(3, int(s * 0.14))
    d.line(pts, fill=255, width=w, joint="curve")
    for p in (pts[0], pts[-1]):
        r = w * 0.9
        d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=255)
    node = pts[RNG.integers(0, len(pts))]
    R = w * 2.2
    d.ellipse((node[0] - R, node[1] - R, node[0] + R, node[1] + R), outline=255, width=max(2, w // 2))
    if RNG.random() < 0.6:
        b = pts[1]
        d.line([b, (b[0] + s * 0.5, b[1] - s * 0.35)], fill=255, width=max(2, w // 2))


def main():
    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    # One column of large glyphs: the plaques are 1.5-2.5 m tall in game, so each glyph reads at 30-50 cm.
    y = 220
    while y < H - 380:
        s = RNG.uniform(190, 260)
        glyph(d, W / 2 + RNG.uniform(-120, 60), y, s)
        y += s * RNG.uniform(1.5, 1.9)
    halo = mask.filter(ImageFilter.GaussianBlur(28))
    core = np.asarray(mask, np.float32) / 255.0
    glow = np.clip(core + 0.45 * np.asarray(halo, np.float32) / 255.0, 0, 1)
    field = np.asarray(Image.fromarray((RNG.random((H // 16, W // 16)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255.0
    rgb = np.stack([glow, field * 0.6 + 0.2, np.zeros_like(glow)], -1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray((rgb * 255).astype(np.uint8)).save(OUT, optimize=True)
    print("written", OUT)


main()
