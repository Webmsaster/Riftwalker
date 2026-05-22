"""Generate the Riftwalker app icon (.ico) from the player sprite.

Composites the player robot over a dark radial rift-glow background and
exports a multi-resolution .ico (16..256 px) for the Windows executable.
"""
import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAYER = os.path.join(ROOT, "assets", "textures", "singles", "player", "player.png")
OUT_ICO = os.path.join(ROOT, "assets", "icon.ico")
OUT_PNG = os.path.join(ROOT, "assets", "icon.png")

CANVAS = 256  # master size, downscaled for the other icon resolutions


def radial_background(size):
    """Dark center -> rift purple/blue edges radial gradient."""
    bg = Image.new("RGB", (size, size))
    px = bg.load()
    cx = cy = size / 2
    maxd = (cx ** 2 + cy ** 2) ** 0.5
    inner = (24, 16, 46)    # deep void purple
    outer = (60, 30, 110)   # rift purple
    for y in range(size):
        for x in range(size):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 / maxd
            t = d ** 0.8
            px[x, y] = tuple(int(inner[i] + (outer[i] - inner[i]) * t) for i in range(3))
    return bg


def rift_glow(size):
    """Soft cyan glow halo behind the player."""
    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(glow)
    r = int(size * 0.34)
    c = size // 2
    d.ellipse([c - r, c - r, c + r, c + r], fill=(60, 180, 230, 130))
    return glow.filter(ImageFilter.GaussianBlur(size * 0.08))


def main():
    base = radial_background(CANVAS).convert("RGBA")
    base = Image.alpha_composite(base, rift_glow(CANVAS))

    player = Image.open(PLAYER).convert("RGBA")
    # Scale player to ~78% of canvas height, keep aspect ratio.
    target_h = int(CANVAS * 0.78)
    scale = target_h / player.height
    new_w = max(1, int(player.width * scale))
    player = player.resize((new_w, target_h), Image.NEAREST)  # crisp pixel art

    # Drop shadow for depth.
    shadow = Image.new("RGBA", base.size, (0, 0, 0, 0))
    sx = (CANVAS - player.width) // 2 + 6
    sy = (CANVAS - player.height) // 2 + 8
    shadow.paste(player, (sx, sy), player)
    shadow = shadow.filter(ImageFilter.GaussianBlur(5))
    base = Image.alpha_composite(base, shadow)

    px = (CANVAS - player.width) // 2
    py = (CANVAS - player.height) // 2
    base.paste(player, (px, py), player)

    base.save(OUT_PNG)
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    base.save(OUT_ICO, format="ICO", sizes=sizes)
    print(f"Wrote {OUT_ICO} ({len(sizes)} sizes) and {OUT_PNG}")


if __name__ == "__main__":
    main()
