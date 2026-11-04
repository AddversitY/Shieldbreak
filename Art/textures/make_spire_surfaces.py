"""Original tileable alloy microstructure: RGB = colour variation, roughness, height.

No downloaded images. One metre per tile; deliberately no panel seams or painted grids.
Run with Python + numpy + Pillow, then Tools/MakeSpireSurfaces.bat.
"""
from pathlib import Path
import numpy as np
from PIL import Image

SIZE = 1024
OUT = Path(__file__).resolve().parents[1] / 'Export' / 'Textures'


def periodic_noise(rng, scale):
    # Wrap the interpolation border before resizing, then take the central tile. Opposite edges
    # describe adjacent samples of the same field rather than independently generated borders.
    field = rng.random((scale, scale)).astype(np.float32)
    tiled = np.tile(field, (3, 3))
    resized = Image.fromarray(tiled).resize((SIZE * 3, SIZE * 3), Image.Resampling.BICUBIC)
    return np.asarray(resized, dtype=np.float32)[SIZE:2*SIZE, SIZE:2*SIZE]


def build():
    rng = np.random.default_rng(20261103)
    broad = periodic_noise(rng, 8)
    fine = periodic_noise(rng, 128)
    grain = rng.random((SIZE, SIZE), dtype=np.float32)
    # Fine linear tooling marks, with enough cross-grain variation to avoid obvious stripes.
    brush = rng.random((SIZE, 1), dtype=np.float32) * 0.65 + fine * 0.35
    colour = np.clip(0.50 + (broad - .5) * .28 + (fine - .5) * .07, 0, 1)
    roughness = np.clip(0.50 + (broad - .5) * .30 + (brush - .5) * .34, 0, 1)
    height = np.clip(0.50 + (brush - .5) * .26 + (grain - .5) * .12, 0, 1)
    pixels = np.stack((colour, roughness, height), axis=-1)
    assert np.isfinite(pixels).all() and pixels.min() >= 0 and pixels.max() <= 1
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / 'T_SB_SpireSurfaceMasks.png'
    Image.fromarray(np.rint(pixels * 255).astype(np.uint8), 'RGB').save(path)
    print(f'{path.name}: {SIZE} x {SIZE}, deterministic original RGB masks; no external source')


if __name__ == '__main__':
    build()
