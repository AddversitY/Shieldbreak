"""Keel's alien hull surface: an original mottled-metal masks texture (procedural, no scans of any game).

Packed like the other Keel masks (R albedo detail around 0.5, G roughness, B height) so M_SB_KeelHull can use it:
large cloudy blotches (the mottled, oxidised look of an alien alloy), a fine grain, faint flowing etch lines and
shallow wavy panel seams. Tinted mauve/violet in the material instance.

    python Art/textures/make_keel_alien_surfaces.py
Writes Art/Export/Textures/T_SB_KeelAlienMasks.png (2048, tiling).
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Textures", "T_SB_KeelAlienMasks.png")
N = 2048
RNG = np.random.default_rng(2611)


def tiling_noise(cells, octaves=5, persistence=0.55):
    """Fractal value noise that tiles (built in the frequency domain-free way: periodic lattice + bicubic)."""
    total = np.zeros((N, N), np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        c = cells * (2 ** o)
        lattice = RNG.random((c, c)).astype(np.float32)
        tiled = np.tile(lattice, (3, 3))
        img = Image.fromarray((tiled * 255).astype(np.uint8)).resize((N * 3, N * 3), Image.BICUBIC)
        layer = np.asarray(img, np.float32)[N:2 * N, N:2 * N] / 255.0
        total += layer * amp
        norm += amp
        amp *= persistence
    return total / norm


def main():
    blotch = tiling_noise(4, 5)
    blotch = (blotch - blotch.min()) / (blotch.max() - blotch.min())
    grain = tiling_noise(64, 2, 0.5)
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    # Flowing etch lines: iso-lines of a warped noise field, thin and faint.
    warp = tiling_noise(3, 3)
    field = (x * 6.0 + warp * 2.5) % 1.0
    etch = np.exp(-((field - 0.5) / 0.012) ** 2)
    # Wavy panel seams every quarter tile.
    seam_field = (y * 4.0 + 0.05 * np.sin(x * np.pi * 8.0)) % 1.0
    seams = np.exp(-(np.minimum(seam_field, 1.0 - seam_field) / 0.006) ** 2)
    detail = 0.5 + 0.38 * (blotch - 0.5) * 2.0 * 0.6 + 0.08 * (grain - 0.5) - 0.18 * seams + 0.06 * etch
    rough = np.clip(0.35 + 0.35 * blotch + 0.15 * (grain - 0.5), 0, 1)
    height = np.clip(0.55 + 0.25 * (blotch - 0.5) + 0.05 * (grain - 0.5) - 0.45 * seams - 0.1 * etch, 0, 1)
    packed = np.stack([np.clip(detail, 0, 1), rough, height], -1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray((packed * 255 + 0.5).astype(np.uint8)).save(OUT, optimize=True)
    print("written", OUT)


main()
