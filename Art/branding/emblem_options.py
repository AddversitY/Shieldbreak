"""Alternative emblem designs for Vincent to pick from, drawn with the same steel-and-cyan finish as make_branding.py.

Every design here is original geometry. None uses a ring, a visor or anything taken from another game.

Writes Art/Export/Branding/Options/:
- option_<letter>_<name>.png       each emblem alone, 1024 px, transparent
- option_<letter>_<name>_logo.png  emblem plus wordmark
- emblem_options_sheet.png         all of them side by side on the menu's dark background, lettered

Run: python3 Art/branding/emblem_options.py
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(__file__))
from make_branding import (  # noqa: E402
    CYAN, NIGHT, ROOT, STEEL_BOTTOM, STEEL_TOP, backdrop, crack, emblem, font, shield_outline, vertical_gradient,
    wordmark,
)

OUT = os.path.join(ROOT, "Art", "Export", "Branding", "Options")
EMBER = (255, 150, 60)


def poly(S, pts, fill=255):
    m = Image.new("L", (S, S), 0)
    ImageDraw.Draw(m).polygon([(x * S, y * S) for x, y in pts], fill=fill)
    return m


def shifted(mask, dx, dy, angle=0.0):
    S = mask.width
    return mask.rotate(angle, center=(S / 2, S / 2), translate=(dx * S, dy * S), resample=Image.BICUBIC)


def finish(body, S, accent=CYAN, core=None):
    """Steel gradient body, inner highlight, glowing rim. `core` is an optional mask that glows solid."""
    steel = vertical_gradient((S, S), STEEL_TOP, STEEL_BOTTOM)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    img.paste(steel, (0, 0), body)

    # Soft top highlight inside the body.
    inner = body.filter(ImageFilter.GaussianBlur(S * 0.03 / 2)).point(lambda v: 255 if v > 215 else 0)
    light = vertical_gradient((S, S), (255, 255, 255), (0, 0, 0)).convert("L").point(lambda v: v * 0.22)
    img.paste((190, 205, 220, 255), (0, 0), ImageChops.multiply(inner, light))

    # A rim band along every edge (inside and out), from a blurred copy of the body.
    edges = body.filter(ImageFilter.GaussianBlur(S * 0.005)).point(lambda v: 255 if 25 < v < 230 else 0)
    lit = edges if core is None else ImageChops.lighter(edges, core)
    glow = lit.filter(ImageFilter.GaussianBlur(S * 0.03))
    halo = Image.new("RGBA", (S, S), accent + (0,))
    halo.putalpha(glow.point(lambda v: min(255, int(v * 2.2))))
    out = Image.alpha_composite(halo, img)
    rim_color = tuple(min(255, int(c * 0.4 + 255 * 0.6)) for c in accent)
    rim = Image.new("RGBA", (S, S), rim_color + (0,))
    rim.putalpha(edges)
    out = Image.alpha_composite(out, rim)
    if core is not None:
        hot = Image.new("RGBA", (S, S), rim_color + (0,))
        hot.putalpha(core)
        out = Image.alpha_composite(out, hot)
    return out


def spark(out, S, x, y, radius=0.05, color=(220, 250, 255)):
    m = Image.new("L", (S, S), 0)
    r = S * radius
    ImageDraw.Draw(m).ellipse((x * S - r, y * S - r, x * S + r, y * S + r), fill=255)
    m = m.filter(ImageFilter.GaussianBlur(S * 0.03))
    flare = Image.new("RGBA", (S, S), color + (0,))
    flare.putalpha(m.point(lambda v: int(v * 0.8)))
    return Image.alpha_composite(out, flare)


def hex_shatter(S):
    """A hexagonal energy cell with its upper-right corner blown out into three shards."""
    hexagon = [(0.5 + 0.40 * math.cos(math.radians(a)), 0.5 + 0.40 * math.sin(math.radians(a)))
               for a in range(-90, 270, 60)]
    body = poly(S, hexagon)
    # The blown-out corner: the region beyond a jagged line, split into three shards that fly outwards.
    cut = [(0.52, 0.06), (0.56, 0.30), (0.70, 0.36), (0.95, 0.42), (1.0, 0.0)]
    corner = ImageChops.multiply(body, poly(S, cut))
    body = ImageChops.subtract(body, corner)
    shards = [
        [(0.52, 0.06), (0.56, 0.30), (0.66, 0.12)],
        [(0.56, 0.30), (0.70, 0.36), (0.80, 0.20), (0.66, 0.12)],
        [(0.70, 0.36), (0.85, 0.40), (0.85, 0.30), (0.80, 0.20)],
    ]
    for i, s in enumerate(shards):
        piece = ImageChops.multiply(corner, poly(S, s))
        piece = piece.filter(ImageFilter.GaussianBlur(S * 0.012 / 2)).point(lambda v: 255 if v > 215 else 0)
        body = ImageChops.lighter(body, shifted(piece, 0.03 + 0.025 * i, -0.04 - 0.015 * i, -6 - 5 * i))
    # A glowing hexagonal core, cracked through the middle.
    core_hex = [(0.5 + 0.12 * math.cos(math.radians(a)), 0.52 + 0.12 * math.sin(math.radians(a)))
                for a in range(-90, 270, 60)]
    core = poly(S, core_hex)
    line = Image.new("L", (S, S), 0)
    ImageDraw.Draw(line).line([(0.47 * S, 0.40 * S), (0.53 * S, 0.50 * S), (0.48 * S, 0.56 * S), (0.53 * S, 0.65 * S)],
                              fill=255, width=int(S * 0.02))
    core = ImageChops.subtract(core, line)
    body = ImageChops.subtract(body, core.filter(ImageFilter.GaussianBlur(S * 0.03 / 2)).point(lambda v: 255 if v > 40 else 0))
    return spark(finish(body, S, core=core), S, 0.56, 0.30)


def pierce(S):
    """A spearhead driven down through a pointed shield, splitting it into two halves that lean apart."""
    shield = Image.new("L", (S, S), 0)
    ImageDraw.Draw(shield).polygon(shield_outline(S / 2, S * 0.55, S * 0.66, S * 0.72), fill=255)
    left = ImageChops.multiply(shield, poly(S, [(0, 0), (0.5, 0), (0.5, 1), (0, 1)]))
    right = ImageChops.subtract(shield, left)
    body = ImageChops.lighter(shifted(left, -0.035, 0.01, 4), shifted(right, 0.035, 0.01, -4))
    spear = poly(S, [(0.5, 0.04), (0.585, 0.30), (0.535, 0.30), (0.535, 0.78), (0.5, 0.86), (0.465, 0.78),
                     (0.465, 0.30), (0.415, 0.30)])
    gap = spear.filter(ImageFilter.GaussianBlur(S * 0.035 / 2)).point(lambda v: 255 if v > 40 else 0)
    body = ImageChops.subtract(body, gap)
    return spark(finish(body, S, core=spear), S, 0.5, 0.30, radius=0.06)


def chevrons(S):
    """Two heavy chevrons stacked into a shield shape, the lower one knocked off line: fast, military, readable."""
    upper = poly(S, [(0.14, 0.14), (0.5, 0.30), (0.86, 0.14), (0.86, 0.34), (0.5, 0.52), (0.14, 0.34)])
    lower = poly(S, [(0.14, 0.44), (0.5, 0.62), (0.86, 0.44), (0.86, 0.62), (0.5, 0.90), (0.14, 0.62)])
    lower = shifted(lower, 0.03, 0.015, -3)
    body = ImageChops.lighter(upper, lower)
    # A notch broken out of the lower chevron's right arm.
    body = ImageChops.subtract(body, poly(S, [(0.70, 0.52), (0.80, 0.50), (0.76, 0.60), (0.68, 0.62)]))
    return spark(finish(body, S), S, 0.73, 0.55, radius=0.04)


def monogram(S):
    """An S and a B cut from one diamond plate, split by a diagonal break."""
    plate = poly(S, [(0.5, 0.06), (0.92, 0.5), (0.5, 0.94), (0.08, 0.5)])
    letters = Image.new("L", (S, S), 0)
    f = font(int(S * 0.40))
    d = ImageDraw.Draw(letters)
    for ch, x in (("S", 0.30), ("B", 0.52)):
        box = f.getbbox(ch)
        d.text((x * S - box[0], 0.5 * S - (box[1] + box[3]) / 2), ch, font=f, fill=255)
    body = ImageChops.subtract(plate, letters.filter(ImageFilter.GaussianBlur(S * 0.02 / 2)).point(lambda v: 255 if v > 40 else 0))
    split = poly(S, [(0.0, 0.0), (0.62, 0.0), (0.40, 1.0), (0.0, 1.0)])
    left = ImageChops.multiply(body, split)
    right = ImageChops.subtract(body, split)
    body = ImageChops.lighter(left, shifted(right, 0.02, 0.025, -2))
    letters = ImageChops.lighter(ImageChops.multiply(letters, split), shifted(ImageChops.subtract(letters, split), 0.02, 0.025, -2))
    return finish(body, S, core=letters)


def shard_shield(S):
    """The pointed shield exploded into five facets with glowing seams, like it's mid-break."""
    cx, cy, w, h = S / 2, S / 2, S * 0.62, S * 0.76
    shield = Image.new("L", (S, S), 0)
    ImageDraw.Draw(shield).polygon(shield_outline(cx, cy, w, h), fill=255)
    hub = (0.47, 0.44)
    angles = [-100, -30, 25, 80, 150, 205]  # seam directions from the hub, degrees (0 = right, 90 = down)
    body = Image.new("L", (S, S), 0)
    for i in range(len(angles)):
        a0, a1 = angles[i], angles[(i + 1) % len(angles)] + (360 if i == len(angles) - 1 else 0)
        ray = lambda a: (hub[0] + 2 * math.cos(math.radians(a)), hub[1] + 2 * math.sin(math.radians(a)))
        piece = ImageChops.multiply(shield, poly(S, [hub, ray(a0), ray((a0 + a1) / 2), ray(a1)]))
        piece = piece.filter(ImageFilter.GaussianBlur(S * 0.01 / 2)).point(lambda v: 255 if v > 215 else 0)
        mid = math.radians((a0 + a1) / 2)
        push = 0.022
        body = ImageChops.lighter(body, shifted(piece, math.cos(mid) * push, math.sin(mid) * push, (i - 2.5) * 1.5))
    return spark(finish(body, S), S, hub[0], hub[1], radius=0.07)


def ember_crack(S):
    """The current cracked shield, but darker steel with an orange-hot crack: a warmer, more aggressive variant."""
    w, h = S * 0.66, S * 0.80
    cx, cy = S / 2, S / 2
    shield = Image.new("L", (S, S), 0)
    ImageDraw.Draw(shield).polygon(shield_outline(cx, cy, w, h), fill=255)
    split = crack(cx, cy, w, h)
    seam = Image.new("L", (S, S), 0)
    ImageDraw.Draw(seam).line(split, fill=255, width=int(S * 0.03))
    body = ImageChops.subtract(shield, seam)
    core = ImageChops.multiply(seam.filter(ImageFilter.MinFilter(3)), shield)
    out = finish(body, S, accent=EMBER, core=core)
    return spark(out, S, split[1][0] / S, split[1][1] / S, color=(255, 220, 160))


OPTIONS = [
    ("A", "cracked", "Cracked shield (current)", lambda S: emblem(S)),
    ("B", "hex", "Hex shatter", hex_shatter),
    ("C", "pierce", "Spearhead", pierce),
    ("D", "chevrons", "Twin chevrons", chevrons),
    ("E", "monogram", "SB monogram", monogram),
    ("F", "shards", "Shard shield", shard_shield),
    ("G", "ember", "Ember crack", ember_crack),
]


def render(fn, size):
    if fn is OPTIONS[0][3]:
        return emblem(size)
    ss = 4
    return fn(size * ss).resize((size, size), Image.LANCZOS)


def lockup(mark, width, height):
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    m = mark.resize((int(height * 0.95),) * 2, Image.LANCZOS)
    img.alpha_composite(m, (0, (height - m.height) // 2))
    word = wordmark(int(height * 0.32))
    room = width - m.width - int(height * 0.05)
    if word.width > room:
        word = word.resize((room, int(word.height * room / word.width)), Image.LANCZOS)
    img.alpha_composite(word, (m.width + int(height * 0.02), (height - word.height) // 2))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    cell_w, cell_h = 560, 640
    cols = 4
    rows = math.ceil(len(OPTIONS) / cols)
    sheet = backdrop(cell_w * cols, cell_h * rows + 120)
    d = ImageDraw.Draw(sheet)
    title_font, label_font, small_font = font(56), font(40), font(26)
    d.text((40, 30), "SHIELDBREAK  -  emblem options", font=title_font, fill=(235, 242, 250, 255))
    for i, (letter, slug, name, fn) in enumerate(OPTIONS):
        mark = render(fn, 1024)
        mark.save(os.path.join(OUT, "option_%s_%s.png" % (letter, slug)))
        lockup(mark, 1600, 600).save(os.path.join(OUT, "option_%s_%s_logo.png" % (letter, slug)))
        x, y = (i % cols) * cell_w, 120 + (i // cols) * cell_h
        big = mark.resize((400, 400), Image.LANCZOS)
        sheet.alpha_composite(big, (x + (cell_w - 400) // 2, y + 20))
        # Small sizes, to judge how it reads as an icon.
        for j, s in enumerate((96, 48, 24)):
            sheet.alpha_composite(mark.resize((s, s), Image.LANCZOS), (x + 40 + j * 120 + (96 - s) // 2, y + 440 + (96 - s) // 2))
        d.text((x + 40, y + 550), "%s  %s" % (letter, name), font=label_font, fill=(235, 242, 250, 255))
        print("option", letter, name)
    d.text((cell_w * 3 + 40, 120 + cell_h + 260), "Each shown big and at\nicon sizes (96, 48, 24 px).", font=small_font,
           fill=(150, 165, 180, 255))
    sheet.convert("RGB").save(os.path.join(OUT, "emblem_options_sheet.png"))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
