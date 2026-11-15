"""Pack Keel's scanned hull surfaces (CC0 Poly Haven scans) into one masks texture per surface.

Each output is RGB: R = albedo detail (luminance, normalised around 0.5), G = roughness, B = height. The game tints the
detail with the map's palette colour, so the scan's own paint colour (blue plate, rust) never shows; only its wear,
scratches, seams and rivets do. One packed texture per surface keeps the material at three samples (triplanar).

    python Art/textures/make_keel_surfaces.py        (needs numpy and Pillow; downloads ~95 MB once into Saved/PolyHaven)

Sources (all CC0, polyhaven.com): blue_metal_plate (walls), metal_plate (decks), metal_plate_02 (frames and trim).
Writes Art/Export/Textures/T_SB_Keel<Surface>Masks.png.
"""
import os
import urllib.request

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CACHE = os.path.join(ROOT, "Saved", "PolyHaven")
OUT = os.path.join(ROOT, "Art", "Export", "Textures")
SIZE = 2048
BASE = "https://dl.polyhaven.org/file/ph-assets/Textures/png/2k/{a}/{a}_{m}_2k.png"
SURFACES = {"Plate": "blue_metal_plate", "Deck": "metal_plate", "Frame": "metal_plate_02"}


def fetch(asset, kind):
    path = os.path.join(CACHE, f"{asset}_{kind}_2k.png")
    if not os.path.exists(path):
        os.makedirs(CACHE, exist_ok=True)
        request = urllib.request.Request(BASE.format(a=asset, m=kind), headers={"User-Agent": "Shieldbreak-art-build"})
        with urllib.request.urlopen(request, timeout=300) as response, open(path + ".part", "wb") as f:
            f.write(response.read())
        os.replace(path + ".part", path)
    return Image.open(path)


def channel(image):
    a = np.asarray(image.convert("I;16") if image.mode in ("I;16", "I") else image.convert("L"), dtype=np.float32)
    a /= 65535.0 if a.max() > 255 else 255.0
    if a.shape[0] != SIZE:
        a = np.asarray(Image.fromarray((a * 65535).astype(np.uint16)).resize((SIZE, SIZE), Image.LANCZOS), np.float32) / 65535.0
    return a


def main():
    os.makedirs(OUT, exist_ok=True)
    for surface, asset in SURFACES.items():
        diffuse = np.asarray(fetch(asset, "diff").convert("RGB").resize((SIZE, SIZE), Image.LANCZOS), np.float32) / 255.0
        luminance = diffuse @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        # Detail around 0.5: 1 = the scan's average brightness; the material maps R to 0.55..1.45 of the tint.
        detail = np.clip(0.5 + (luminance - luminance.mean()) / (4.0 * luminance.std() + 1e-6), 0.0, 1.0)
        rough = channel(fetch(asset, "rough"))
        height = channel(fetch(asset, "disp"))
        height = (height - height.min()) / max(1e-6, height.max() - height.min())
        packed = np.stack([detail, rough, height], axis=-1)
        path = os.path.join(OUT, f"T_SB_Keel{surface}Masks.png")
        Image.fromarray((packed * 255.0 + 0.5).astype(np.uint8)).save(path, optimize=True)
        print(f"{surface}: {asset} -> {path}  detail mean {detail.mean():.2f}, rough mean {rough.mean():.2f}")


main()
