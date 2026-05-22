"""Generate placeholder Steam store capsule art for Riftwalker.

Produces every capsule/library size Valve requires, composited from the
player sprite over a rift-themed gradient with the game title. These are
PLACEHOLDERS — replace with commissioned key art before launch, but they
satisfy the Steamworks asset upload requirements and look presentable.

Output: assets/store/*.png
Run:    python tools/make_store_capsules.py
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAYER = os.path.join(ROOT, "assets", "textures", "singles", "player", "player.png")
BOSS = os.path.join(ROOT, "assets", "textures", "singles", "bosses", "void_sovereign.png")
FONT_PATH = os.path.join(ROOT, "assets", "fonts", "default.ttf")
OUT_DIR = os.path.join(ROOT, "assets", "store")

TITLE = "RIFTWALKER"
SUBTITLE = "DIMENSION-SHIFTING ROGUELIKE"

# Steam asset name -> (width, height). Library logo is transparent (no bg).
CAPSULES = {
    "small_capsule_462x174":     (462, 174),
    "header_capsule_460x215":    (460, 215),
    "main_capsule_616x353":      (616, 353),
    "vertical_capsule_374x448":  (374, 448),
    "page_background_1438x810":  (1438, 810),
    "library_capsule_600x900":   (600, 900),
    "library_hero_3840x1240":    (3840, 1240),
}
LIBRARY_LOGO = "library_logo_1280x720"  # transparent, title only


def gradient(w, h):
    """Diagonal deep-void -> rift-purple gradient with a cyan glow blob."""
    img = Image.new("RGB", (w, h))
    px = img.load()
    inner = (20, 14, 40)
    outer = (64, 32, 116)
    for y in range(h):
        for x in range(w):
            t = ((x / w) * 0.5 + (y / h) * 0.5)
            px[x, y] = tuple(int(inner[i] + (outer[i] - inner[i]) * t) for i in range(3))
    img = img.convert("RGBA")
    # cyan rift glow on the left third
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = int(min(w, h) * 0.55)
    cx, cy = int(w * 0.30), int(h * 0.5)
    gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(50, 170, 220, 70))
    glow = glow.filter(ImageFilter.GaussianBlur(min(w, h) * 0.10))
    return Image.alpha_composite(img, glow)


def fit_font(size):
    return ImageFont.truetype(FONT_PATH, size)


def fit_font_to_width(text, max_w, start_size, min_size=8):
    """Largest font (<= start_size) whose rendered `text` width <= max_w."""
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    size = start_size
    while size > min_size:
        f = fit_font(size)
        if probe.textlength(text, font=f) <= max_w:
            return f
        size -= 1
    return fit_font(min_size)


def text_with_outline(draw, xy, text, font, fill, outline=(8, 4, 20), ow=3):
    x, y = xy
    for dx in range(-ow, ow + 1):
        for dy in range(-ow, ow + 1):
            if dx or dy:
                draw.text((x + dx, y + dy), text, font=font, fill=outline)
    draw.text((x, y), text, font=font, fill=fill)


def scaled_sprite(path, target_h):
    s = Image.open(path).convert("RGBA")
    scale = target_h / s.height
    return s.resize((max(1, int(s.width * scale)), target_h), Image.NEAREST)


def make_capsule(name, w, h):
    img = gradient(w, h)
    draw = ImageDraw.Draw(img)
    vertical = h >= w  # vertical/library layouts stack title under art

    # Character art sized to the capsule.
    char_h = int(h * (0.62 if vertical else 0.82))
    player = scaled_sprite(PLAYER, char_h)

    if vertical:
        # art centered upper area, title below
        px = (w - player.width) // 2
        py = int(h * 0.06)
        img.alpha_composite(player, (px, py))
        avail = int(w * 0.90)
        font = fit_font_to_width(TITLE, avail, int(w * 0.16))
        ts = font.size
        tw = draw.textlength(TITLE, font=font)
        ty = int(h * 0.74)
        text_with_outline(draw, ((w - tw) // 2, ty), TITLE, font, (236, 240, 255))
        sf = fit_font_to_width(SUBTITLE, avail, max(10, int(w * 0.06)))
        sw = draw.textlength(SUBTITLE, font=sf)
        text_with_outline(draw, ((w - sw) // 2, ty + int(ts * 1.15)), SUBTITLE, sf,
                          (150, 200, 235), ow=2)
    else:
        # art on the right, title block on the left
        px = int(w * 0.64)
        py = (h - player.height) // 2
        img.alpha_composite(player, (px, py))
        # add a faint second character (boss) far right for big formats
        if w >= 1200:
            boss = scaled_sprite(BOSS, int(h * 0.7))
            img.alpha_composite(boss, (int(w * 0.82), (h - boss.height) // 2))
        tx = int(w * 0.05)
        avail = int(w * 0.64) - tx - int(w * 0.03)   # left block, clear of the art
        font = fit_font_to_width(TITLE, avail, int(h * 0.34))
        ts = font.size
        # vertically center the title+subtitle block on the left
        sf = fit_font_to_width(SUBTITLE, avail, max(11, int(h * 0.11)))
        block_h = ts + int(ts * 0.15) + sf.size
        ty = (h - block_h) // 2
        text_with_outline(draw, (tx, ty), TITLE, font, (236, 240, 255))
        text_with_outline(draw, (tx + 2, ty + int(ts * 1.05)), SUBTITLE, sf,
                          (150, 200, 235), ow=2)

    img.convert("RGB").save(os.path.join(OUT_DIR, name + ".png"))


def make_library_logo(w, h):
    """Transparent title logo for the Steam library."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    font = fit_font(int(h * 0.22))
    tw = draw.textlength(TITLE, font=font)
    text_with_outline(draw, ((w - tw) // 2, int(h * 0.30)), TITLE, font,
                      (240, 244, 255), outline=(20, 60, 90), ow=4)
    sf = fit_font(int(h * 0.07))
    sw = draw.textlength(SUBTITLE, font=sf)
    text_with_outline(draw, ((w - sw) // 2, int(h * 0.58)), SUBTITLE, sf,
                      (150, 200, 235), outline=(20, 60, 90), ow=3)
    img.save(os.path.join(OUT_DIR, LIBRARY_LOGO + ".png"))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, (w, h) in CAPSULES.items():
        make_capsule(name, w, h)
        print(f"  {name}.png  ({w}x{h})")
    make_library_logo(1280, 720)
    print(f"  {LIBRARY_LOGO}.png  (1280x720, transparent)")
    print(f"\nWrote {len(CAPSULES) + 1} placeholder assets to assets/store/")
    print("NOTE: placeholders — replace with commissioned key art before launch.")


if __name__ == "__main__":
    main()
