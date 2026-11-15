"""Original tileable wear masks for the character and weapon material (M_SB_Character, Tools/import_art.py).

RGB = colour variation (mean 0.5), roughness variation (mean 0.5), scratches (0 = none, 1 = bright scratch).
No downloaded images: numpy noise and random wrapped line segments only. One tile spans about 24 cm on a gun or
a suit (TileSize in the material), so the scratches read at first-person distance without visible repeats.

    python3 Art/textures/make_wear_texture.py
"""
from pathlib import Path

import numpy as np
from PIL import Image

SIZE = 512
OUT = Path(__file__).resolve().parents[1] / "Export" / "Textures"


def periodic_noise(rng, cells):
    field = rng.random((cells, cells)).astype(np.float32)
    tiled = np.tile(field, (3, 3))
    resized = Image.fromarray(tiled).resize((SIZE * 3, SIZE * 3), Image.Resampling.BICUBIC)
    return np.asarray(resized, dtype=np.float32)[SIZE:2 * SIZE, SIZE:2 * SIZE]


def scratches(rng, count):
    mask = np.zeros((SIZE, SIZE), dtype=np.float32)
    for _ in range(count):
        x, y = rng.random(2) * SIZE
        angle = rng.normal(0.35, 0.5)  # mostly one handling direction, some across it
        length = rng.uniform(8, 60)
        strength = rng.uniform(0.35, 1.0)
        steps = int(length * 2)
        for t in np.linspace(0, 1, steps):
            px = int(x + np.cos(angle) * length * t) % SIZE
            py = int(y + np.sin(angle) * length * t) % SIZE
            fade = strength * (1 - abs(t - 0.5) * 1.6)
            mask[py, px] = max(mask[py, px], fade)
    # Soften by one pixel so they don't alias.
    soft = (mask + np.roll(mask, 1, 0) * 0.5 + np.roll(mask, 1, 1) * 0.5) / 2.0
    return np.clip(soft, 0, 1)


def build():
    rng = np.random.default_rng(20261105)
    broad = periodic_noise(rng, 6)
    mid = periodic_noise(rng, 32)
    grain = periodic_noise(rng, 192)
    colour = np.clip(0.5 + (broad - 0.5) * 0.22 + (mid - 0.5) * 0.10 + (grain - 0.5) * 0.06, 0, 1)
    rough = np.clip(0.5 + (broad - 0.5) * 0.35 + (mid - 0.5) * 0.25 + (grain - 0.5) * 0.15, 0, 1)
    scratch = scratches(rng, 420)
    pixels = np.stack((colour, rough, scratch), axis=-1)
    assert np.isfinite(pixels).all()
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "T_SB_Wear.png"
    Image.fromarray(np.rint(pixels * 255).astype(np.uint8), "RGB").save(path)
    print("%s: %d x %d, mean colour %.3f, mean roughness %.3f, scratch cover %.1f%%"
          % (path.name, SIZE, SIZE, colour.mean(), rough.mean(), (scratch > 0.1).mean() * 100))


if __name__ == "__main__":
    build()
