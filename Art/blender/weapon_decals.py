"""Decal artwork for the textured weapons (bake_weapons.py): original stencilled markings drawn with Pillow.

Each decal is a white-on-transparent PNG; bake_weapons.py projects it onto the gun in mesh space (cm) and tints it.
Text uses the system's open-licence fonts (Inter, DejaVu). Nothing here copies another game's markings: the names,
serials, calibre and icons are Shieldbreak's own.
"""
import os

from PIL import Image, ImageDraw, ImageFont

_WIN = os.path.join(os.environ.get("WINDIR", "C:\\Windows"), "Fonts")
# First existing file wins: Linux (cloud) fonts, then Windows equivalents, so the PC can bake the same markings.
FONT_CHOICES = {
    "display": ["/usr/share/fonts/opentype/inter/InterDisplay-Black.otf", os.path.join(_WIN, "ariblk.ttf"),
                os.path.join(_WIN, "arialbd.ttf")],
    "bold": ["/usr/share/fonts/opentype/inter/Inter-Bold.otf", os.path.join(_WIN, "segoeuib.ttf"),
             os.path.join(_WIN, "arialbd.ttf")],
    "mono": ["/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", os.path.join(_WIN, "consolab.ttf"),
             os.path.join(_WIN, "courbd.ttf")],
}
FONTS = {key: next((p for p in paths if os.path.exists(p)), paths[-1]) for key, paths in FONT_CHOICES.items()}
PX_PER_CM = 96
HERE = os.path.dirname(os.path.abspath(__file__))
EMBLEM = os.path.join(HERE, "..", "Export", "Branding", "emblem_1024.png")


def _canvas(w_cm, h_cm):
    return Image.new("RGBA", (max(8, int(w_cm * PX_PER_CM)), max(8, int(h_cm * PX_PER_CM))), (0, 0, 0, 0))


def _fit_font(draw, text, font_key, w, h, spacing=0):
    size = h
    while size > 6:
        font = ImageFont.truetype(FONTS[font_key], size)
        box = draw.textbbox((0, 0), text, font=font)
        if box[2] - box[0] + spacing * len(text) <= w * 0.98 and box[3] - box[1] <= h * 0.92:
            return font, box
        size -= 2
    font = ImageFont.truetype(FONTS[font_key], 6)
    return font, draw.textbbox((0, 0), text, font=font)


def text(w_cm, h_cm, value, font_key="bold", align="left", spacing=0):
    img = _canvas(w_cm, h_cm)
    d = ImageDraw.Draw(img)
    font, box = _fit_font(d, value, font_key, img.width, img.height, spacing)
    tw = box[2] - box[0] + spacing * len(value)
    x = {"left": 0, "center": (img.width - tw) // 2, "right": img.width - tw}[align] - box[0]
    y = (img.height - (box[3] - box[1])) // 2 - box[1]
    for ch in value:
        d.text((x, y), ch, font=font, fill=(255, 255, 255, 255))
        x += d.textlength(ch, font=font) + spacing
    return img


def lines(w_cm, h_cm, rows, font_key="mono"):
    """Several left-aligned rows of small data-plate text."""
    img = _canvas(w_cm, h_cm)
    row_h = img.height // len(rows)
    for i, row in enumerate(rows):
        part = text(w_cm, h_cm / len(rows), row, font_key)
        img.alpha_composite(part, (0, i * row_h))
    return img


def warning(w_cm, h_cm, label):
    """A hollow warning triangle with an exclamation bar, then a short label to its right."""
    img = _canvas(w_cm, h_cm)
    d = ImageDraw.Draw(img)
    s = img.height
    t = max(3, s // 9)
    d.polygon([(s * 0.5, 0), (s, s - 1), (0, s - 1)], fill=(255, 255, 255, 255))
    d.polygon([(s * 0.5, t * 2.2), (s - t * 1.8, s - 1 - t), (t * 1.8, s - 1 - t)], fill=(0, 0, 0, 0))
    d.rectangle([s * 0.5 - t * 0.5, s * 0.38, s * 0.5 + t * 0.5, s * 0.7], fill=(255, 255, 255, 255))
    d.rectangle([s * 0.5 - t * 0.5, s * 0.76, s * 0.5 + t * 0.5, s * 0.76 + t], fill=(255, 255, 255, 255))
    if label:
        part = text((img.width - s * 1.2) / PX_PER_CM, h_cm * 0.62, label, "bold")
        img.alpha_composite(part, (int(s * 1.2), (img.height - part.height) // 2))
    return img


def chevrons(w_cm, h_cm, count=3):
    """Forward-pointing chevrons (a direction marking)."""
    img = _canvas(w_cm, h_cm)
    d = ImageDraw.Draw(img)
    step = img.width / count
    t = step * 0.45
    for i in range(count):
        x = i * step
        d.polygon([(x, 0), (x + t, 0), (x + step * 0.9, img.height / 2), (x + t, img.height), (x, img.height),
                   (x + step * 0.9 - t, img.height / 2)], fill=(255, 255, 255, 255))
    return img


def ticks(w_cm, h_cm, count=9):
    """A witness/ranging tick ladder: alternating long and short marks."""
    img = _canvas(w_cm, h_cm)
    d = ImageDraw.Draw(img)
    for i in range(count):
        x = int(i * (img.width - 4) / (count - 1))
        d.rectangle([x, 0 if i % 2 == 0 else img.height * 0.45, x + max(2, img.width // 120), img.height], fill=(255, 255, 255, 255))
    return img


def bar(w_cm, h_cm):
    return Image.new("RGBA", (max(8, int(w_cm * PX_PER_CM)), max(8, int(h_cm * PX_PER_CM))), (255, 255, 255, 255))


def emblem(w_cm, h_cm):
    """The Shieldbreak cracked-shield emblem as a one-colour stamp (from the emblem's own alpha and brightness)."""
    src = Image.open(EMBLEM).convert("RGBA")
    a = src.split()[3]
    lum = src.convert("L")
    mask = Image.composite(lum, Image.new("L", src.size, 0), a).point(lambda v: 255 if v > 90 else 0)
    img = _canvas(w_cm, h_cm)
    side = min(img.width, img.height)
    mask = mask.resize((side, side), Image.LANCZOS)
    stamp = Image.new("RGBA", (side, side), (255, 255, 255, 255))
    stamp.putalpha(mask)
    img.alpha_composite(stamp, ((img.width - side) // 2, (img.height - side) // 2))
    return img


def barcode(w_cm, h_cm, seed=7):
    import random
    rnd = random.Random(seed)
    img = _canvas(w_cm, h_cm)
    d = ImageDraw.Draw(img)
    x = 0
    while x < img.width:
        w = rnd.choice((2, 3, 5, 7))
        if rnd.random() > 0.45:
            d.rectangle([x, 0, x + w, img.height], fill=(255, 255, 255, 255))
        x += w + rnd.choice((2, 3, 4))
    return img


def save(img, folder, name):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name + ".png")
    img.save(path)
    return path
