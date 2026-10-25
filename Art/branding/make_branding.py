"""Shieldbreak's emblem, game icon, splash screen and store-style key art, drawn from scratch with Pillow.

The emblem is an original design: a pointed sci-fi shield, split by a jagged crack with the right half knocked
slightly out of line, with a glowing cyan rim. The wordmark uses Inter (SIL Open Font License), rendered to pixels.

Writes:
- Build/Windows/Application.ico          the game exe's icon (16-256 px), picked up by Unreal's build
- Content/Splash/Splash.bmp              the startup splash Unreal shows while the game loads
- Content/Splash/Emblem.png              the emblem for the main menu title
- Art/Export/Branding/emblem_1024.png    emblem alone, transparent
- Art/Export/Branding/logo_1600x600.png  emblem + wordmark, transparent
- Art/Export/Branding/capsule_920x430.png and header_460x215.png   store-style banners

Run: python3 Art/branding/make_branding.py
"""
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Art", "Export", "Branding")
FONT_PATHS = [
    "/usr/share/fonts/opentype/inter/InterDisplay-Black.otf",
    "/usr/share/fonts/opentype/inter/Inter-Black.otf",
    "/usr/share/fonts/opentype/inter/InterDisplay-ExtraBold.otf",
    "/usr/share/fonts/opentype/inter/Inter-ExtraBold.otf",
]
CYAN = (90, 210, 255)
STEEL_TOP = (70, 82, 96)
STEEL_BOTTOM = (22, 27, 34)
NIGHT = (8, 11, 16)


def font(size):
    for path in FONT_PATHS:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def shield_outline(cx, cy, w, h):
    """Pointed shield: flat top with chamfered corners, straight sides, a long point at the bottom."""
    pts = [
        (-0.40, -0.50), (0.40, -0.50), (0.50, -0.42), (0.50, 0.08),
        (0.0, 0.50), (-0.50, 0.08), (-0.50, -0.42),
    ]
    return [(cx + x * w, cy + y * h) for x, y in pts]


def crack(cx, cy, w, h):
    """Jagged line from the top edge to the bottom point, slightly off centre."""
    pts = [(0.04, -0.52), (-0.03, -0.30), (0.06, -0.12), (-0.04, 0.06), (0.05, 0.22), (-0.02, 0.38), (0.0, 0.52)]
    return [(cx + x * w, cy + y * h) for x, y in pts]


def vertical_gradient(size, top, bottom):
    w, h = size
    grad = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / max(1, h - 1)
        grad.putpixel((0, y), tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return grad.resize((w, h))


def emblem(size):
    """RGBA emblem, supersampled 4x."""
    ss = 4
    S = size * ss
    w, h = S * 0.66, S * 0.80
    cx, cy = S / 2, S / 2
    outline = shield_outline(cx, cy, w, h)
    split = crack(cx, cy, w, h)

    # Masks for the two halves: left of the crack and right of it.
    full = Image.new("L", (S, S), 0)
    ImageDraw.Draw(full).polygon(outline, fill=255)
    left_poly = [(0, 0)] + split + [(0, S)]
    left_mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(left_mask).polygon(left_poly, fill=255)
    left = ImageChops.multiply(full, left_mask)
    right = ImageChops.subtract(full, left_mask)

    # The right half is knocked out of line: shifted and rotated a little.
    shift = (int(S * 0.03), int(S * 0.035))
    right = right.rotate(-2, center=(cx, cy), translate=shift, resample=Image.BICUBIC)
    # Leave a gap along the crack.
    gap = Image.new("L", (S, S), 0)
    ImageDraw.Draw(gap).line(split, fill=255, width=int(S * 0.018))
    left = ImageChops.subtract(left, gap)
    right = ImageChops.subtract(right, gap.rotate(-2, center=(cx, cy), translate=shift))
    body = ImageChops.lighter(left, right)

    steel = vertical_gradient((S, S), STEEL_TOP, STEEL_BOTTOM)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    img.paste(steel, (0, 0), body)

    # Inner bevel: a thinner shield inset, lighter on top.
    inset = shield_outline(cx, cy, w * 0.78, h * 0.78)
    bevel = Image.new("L", (S, S), 0)
    ImageDraw.Draw(bevel).line(inset + [inset[0]], fill=255, width=int(S * 0.012), joint="curve")
    bevel = ImageChops.multiply(bevel, body)
    img.paste((150, 170, 190, 255), (0, 0), bevel.point(lambda v: v * 0.5))

    # Glowing rim around each half and along the crack edges.
    edges = body.filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v > 30 else 0)
    edges = edges.filter(ImageFilter.MaxFilter(max(3, (int(S * 0.012) // 2) * 2 + 1)))
    glow = edges.filter(ImageFilter.GaussianBlur(S * 0.03))
    halo = Image.new("RGBA", (S, S), CYAN + (0,))
    halo.putalpha(glow.point(lambda v: min(255, int(v * 2.2))))
    out = Image.alpha_composite(halo, img)
    rim = Image.new("RGBA", (S, S), (190, 240, 255, 0))
    rim.putalpha(edges)
    out = Image.alpha_composite(out, rim)

    # A bright spark where the crack starts.
    spark = Image.new("L", (S, S), 0)
    sx, sy = split[1]
    r = S * 0.05
    ImageDraw.Draw(spark).ellipse((sx - r, sy - r, sx + r, sy + r), fill=255)
    spark = spark.filter(ImageFilter.GaussianBlur(S * 0.03))
    flare = Image.new("RGBA", (S, S), (220, 250, 255, 0))
    flare.putalpha(spark.point(lambda v: int(v * 0.8)))
    out = Image.alpha_composite(out, flare)
    return out.resize((size, size), Image.LANCZOS)


def wordmark(height, color=(235, 242, 250)):
    f = font(int(height * 0.8))
    text = "SHIELDBREAK"
    tracking = int(height * 0.06)
    widths = [f.getbbox(ch)[2] - f.getbbox(ch)[0] if ch != " " else height // 3 for ch in text]
    total = sum(widths) + tracking * (len(text) - 1)
    img = Image.new("RGBA", (total + height, int(height * 1.3)), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    x = height // 2
    for ch, cw in zip(text, widths):
        draw.text((x - f.getbbox(ch)[0], int(height * 0.1)), ch, font=f, fill=color + (255,))
        x += cw + tracking
    return img.crop(img.getbbox())


def logo(width, height):
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    mark = emblem(int(height * 0.95))
    img.alpha_composite(mark, (0, (height - mark.height) // 2))
    word = wordmark(int(height * 0.32))
    room = width - mark.width - int(height * 0.05)
    if word.width > room:
        word = word.resize((room, int(word.height * room / word.width)), Image.LANCZOS)
    img.alpha_composite(word, (mark.width + int(height * 0.02), (height - word.height) // 2))
    return img


def backdrop(width, height):
    """Night-blue haze with a horizon glow and faint angled panels."""
    bg = vertical_gradient((width, height), (14, 22, 34), NIGHT).convert("RGBA")
    glow = Image.new("L", (width, height), 0)
    ImageDraw.Draw(glow).ellipse((-width * 0.2, height * 0.55, width * 1.2, height * 1.5), fill=110)
    glow = glow.filter(ImageFilter.GaussianBlur(height * 0.15))
    tint = Image.new("RGBA", (width, height), (40, 120, 170, 0))
    tint.putalpha(glow)
    bg = Image.alpha_composite(bg, tint)
    lines = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    d = ImageDraw.Draw(lines)
    for i in range(-6, 14):
        x = i * width / 8
        d.line([(x, height), (x + height * 0.6, 0)], fill=(120, 200, 255, 10), width=max(1, height // 120))
    return Image.alpha_composite(bg, lines)


def banner(width, height):
    img = backdrop(width, height)
    mark = logo(int(width * 0.86), int(height * 0.5))
    img.alpha_composite(mark, ((width - mark.width) // 2, (height - mark.height) // 2))
    return img


def splash(width=800, height=450):
    img = backdrop(width, height)
    mark = logo(int(width * 0.8), int(height * 0.42))
    img.alpha_composite(mark, ((width - mark.width) // 2, int(height * 0.24)))
    d = ImageDraw.Draw(img)
    f = font(int(height * 0.04))
    text = "Made with Unreal Engine"
    tw = d.textlength(text, font=f)
    d.text(((width - tw) / 2, height * 0.88), text, font=f, fill=(150, 165, 180, 255))
    return img.convert("RGB")


def main():
    os.makedirs(OUT, exist_ok=True)
    big = emblem(1024)
    big.save(os.path.join(OUT, "emblem_1024.png"))
    logo(1600, 600).save(os.path.join(OUT, "logo_1600x600.png"))
    banner(920, 430).save(os.path.join(OUT, "capsule_920x430.png"))
    banner(460, 215).save(os.path.join(OUT, "header_460x215.png"))

    ico_dir = os.path.join(ROOT, "Build", "Windows")
    os.makedirs(ico_dir, exist_ok=True)
    # Small sizes get a dark tile behind the emblem so it reads on any taskbar.
    tile = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    ImageDraw.Draw(tile).rounded_rectangle((8, 8, 248, 248), radius=44, fill=NIGHT + (255,))
    tile.alpha_composite(emblem(240), (8, 8))
    tile.save(os.path.join(ico_dir, "Application.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

    splash_dir = os.path.join(ROOT, "Content", "Splash")
    os.makedirs(splash_dir, exist_ok=True)
    splash().save(os.path.join(splash_dir, "Splash.bmp"))
    # The main menu shows the emblem beside the title (loaded from this file at runtime).
    emblem(256).save(os.path.join(splash_dir, "Emblem.png"))
    print("wrote branding to", OUT, "plus Build/Windows/Application.ico and Content/Splash/Splash.bmp")


if __name__ == "__main__":
    main()
