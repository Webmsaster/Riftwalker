#!/usr/bin/env python3
"""Generate UI texture assets for Riftwalker HUD/menus.

Creates clean, dark-themed panel/bar/button textures using Pillow.
All textures are designed for 9-slice rendering where applicable.
Style: Dark sci-fi/dimensional rift theme (deep blues, purples, subtle glows).
"""

import os
from PIL import Image, ImageDraw, ImageFilter, ImageChops

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "ui")
os.makedirs(OUT_DIR, exist_ok=True)


def rounded_rect_mask(size, radius):
    """Create an anti-aliased rounded rectangle mask at 2x then downscale."""
    scale = 2
    big = Image.new("L", (size[0] * scale, size[1] * scale), 0)
    draw = ImageDraw.Draw(big)
    draw.rounded_rectangle([0, 0, big.width - 1, big.height - 1],
                           radius=radius * scale, fill=255)
    return big.resize(size, Image.LANCZOS)


def make_panel_dark():
    """Dark panel background (9-slice capable, 128x128).
    Used for HUD backing, dialog boxes, tooltips.
    """
    W, H = 128, 128
    R = 12  # corner radius

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    mask = rounded_rect_mask((W, H), R)

    # Base fill: dark blue-purple gradient (top darker, bottom slightly lighter)
    base = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(base)
    for y in range(H):
        t = y / H
        r = int(8 + t * 6)
        g = int(8 + t * 5)
        b = int(18 + t * 8)
        a = int(210 + t * 30)
        draw.line([(0, y), (W - 1, y)], fill=(r, g, b, a))

    # Apply rounded mask
    base.putalpha(ImageChops.multiply(base.split()[3], mask))
    img = Image.alpha_composite(img, base)

    # Inner highlight at top edge (subtle white line)
    highlight = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hdraw = ImageDraw.Draw(highlight)
    for x in range(R + 2, W - R - 2):
        hdraw.point((x, 2), fill=(140, 130, 180, 40))
        hdraw.point((x, 3), fill=(100, 95, 140, 20))
    highlight.putalpha(ImageChops.multiply(highlight.split()[3], mask))
    img = Image.alpha_composite(img, highlight)

    # Border: 1px outer border + 1px inner border
    border = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bdraw = ImageDraw.Draw(border)
    # Outer border
    bdraw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                            outline=(80, 70, 120, 100), width=1)
    # Inner border
    bdraw.rounded_rectangle([1, 1, W - 2, H - 2], radius=R - 1,
                            outline=(50, 45, 75, 60), width=1)
    img = Image.alpha_composite(img, border)

    img.save(os.path.join(OUT_DIR, "panel_dark.png"))
    print("  panel_dark.png (128x128)")


def make_panel_light():
    """Lighter panel variant for tooltips, popups."""
    W, H = 128, 128
    R = 10

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    mask = rounded_rect_mask((W, H), R)

    base = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(base)
    for y in range(H):
        t = y / H
        r = int(20 + t * 8)
        g = int(18 + t * 8)
        b = int(35 + t * 10)
        a = int(230 + t * 20)
        draw.line([(0, y), (W - 1, y)], fill=(r, g, b, a))

    base.putalpha(ImageChops.multiply(base.split()[3], mask))
    img = Image.alpha_composite(img, base)

    # Brighter top highlight
    highlight = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hdraw = ImageDraw.Draw(highlight)
    for x in range(R + 2, W - R - 2):
        hdraw.point((x, 2), fill=(180, 170, 220, 55))
        hdraw.point((x, 3), fill=(120, 115, 160, 30))
    highlight.putalpha(ImageChops.multiply(highlight.split()[3], mask))
    img = Image.alpha_composite(img, highlight)

    border = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bdraw = ImageDraw.Draw(border)
    bdraw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                            outline=(100, 90, 150, 130), width=1)
    bdraw.rounded_rectangle([1, 1, W - 2, H - 2], radius=R - 1,
                            outline=(60, 55, 90, 70), width=1)
    img = Image.alpha_composite(img, border)

    img.save(os.path.join(OUT_DIR, "panel_light.png"))
    print("  panel_light.png (128x128)")


def make_bar_frame():
    """Health/entropy bar frame texture (256x40).
    Provides an ornate border around bars.
    """
    W, H = 256, 40
    R = 6

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # Dark inner fill
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([2, 2, W - 3, H - 3], radius=R - 2,
                           fill=(10, 10, 18, 200))

    # Outer border (metallic)
    draw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                           outline=(90, 80, 130, 180), width=2)
    # Inner border (darker)
    draw.rounded_rectangle([2, 2, W - 3, H - 3], radius=R - 2,
                           outline=(40, 35, 60, 120), width=1)

    # Top specular highlight
    for x in range(R + 2, W - R - 2):
        draw.point((x, 3), fill=(160, 150, 200, 50))

    # Bottom shadow line
    for x in range(R + 2, W - R - 2):
        draw.point((x, H - 4), fill=(0, 0, 0, 40))

    # Corner accents (small bright dots at corners)
    for cx, cy in [(R + 2, 4), (W - R - 3, 4), (R + 2, H - 5), (W - R - 3, H - 5)]:
        draw.point((cx, cy), fill=(140, 130, 180, 80))

    img.save(os.path.join(OUT_DIR, "bar_frame.png"))
    print("  bar_frame.png (256x40)")


def make_minimap_frame():
    """Ornate minimap frame (340x240)."""
    W, H = 340, 240
    R = 8

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Outer frame background
    draw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                           fill=(30, 28, 50, 220))
    # Cut out center (transparent inner area)
    inner_margin = 10
    inner = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    idraw = ImageDraw.Draw(inner)
    idraw.rounded_rectangle([inner_margin, inner_margin,
                             W - inner_margin - 1, H - inner_margin - 1],
                            radius=R - 4, fill=(255, 255, 255, 255))
    inner_mask = inner.split()[3]

    # Apply cutout: where inner is white, make frame transparent
    r, g, b, a = img.split()
    a = ImageChops.subtract(a, inner_mask)
    img = Image.merge("RGBA", (r, g, b, a))

    # Re-draw borders on top
    border = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bdraw = ImageDraw.Draw(border)
    # Outer border
    bdraw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                            outline=(100, 90, 150, 180), width=2)
    # Inner border
    bdraw.rounded_rectangle([inner_margin, inner_margin,
                             W - inner_margin - 1, H - inner_margin - 1],
                            radius=R - 4, outline=(70, 65, 110, 160), width=1)

    # Top center ornament (small diamond)
    cx, cy = W // 2, 3
    for dy, hw in [(-2, 1), (-1, 2), (0, 3), (1, 2), (2, 1)]:
        for dx in range(-hw, hw + 1):
            bdraw.point((cx + dx, cy + dy), fill=(140, 130, 200, 200))

    # Side ornaments (small lines)
    for y in [H // 3, 2 * H // 3]:
        for dx in range(4):
            bdraw.point((2 + dx, y), fill=(100, 90, 160, 120))
            bdraw.point((W - 3 - dx, y), fill=(100, 90, 160, 120))

    img = Image.alpha_composite(img, border)
    img.save(os.path.join(OUT_DIR, "minimap_frame.png"))
    print("  minimap_frame.png (340x240)")


def make_button(name, base_color, highlight_alpha, pressed=False):
    """Generate a button texture (400x72)."""
    W, H = 400, 72
    R = 8

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    mask = rounded_rect_mask((W, H), R)

    # Gradient fill
    base = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(base)

    br, bg, bb = base_color
    for y in range(H):
        t = y / H
        if pressed:
            # Pressed: darker at top, slightly lighter at bottom (inverted)
            factor = 0.6 + t * 0.3
        else:
            # Normal: lighter at top, darker at bottom
            factor = 0.9 - t * 0.3
        r = int(br * factor)
        g = int(bg * factor)
        b = int(bb * factor)
        a = 220
        draw.line([(0, y), (W - 1, y)], fill=(r, g, b, a))

    base.putalpha(ImageChops.multiply(base.split()[3], mask))
    img = Image.alpha_composite(img, base)

    # Top highlight line
    hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hldraw = ImageDraw.Draw(hl)
    for x in range(R + 2, W - R - 2):
        if not pressed:
            hldraw.point((x, 2), fill=(255, 255, 255, highlight_alpha))
            hldraw.point((x, 3), fill=(200, 200, 255, highlight_alpha // 2))
    hl.putalpha(ImageChops.multiply(hl.split()[3], mask))
    img = Image.alpha_composite(img, hl)

    # Border
    border = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bdraw = ImageDraw.Draw(border)
    bdraw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                            outline=(100, 90, 150, 160), width=1)
    if not pressed:
        bdraw.rounded_rectangle([1, 1, W - 2, H - 2], radius=R - 1,
                                outline=(50, 45, 80, 80), width=1)
    img = Image.alpha_composite(img, border)

    img.save(os.path.join(OUT_DIR, f"{name}.png"))
    print(f"  {name}.png ({W}x{H})")


def make_divider():
    """Horizontal divider line (256x4)."""
    W, H = 256, 4
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Centered gradient line that fades at edges
    for x in range(W):
        # Distance from center (0 at center, 1 at edges)
        t = abs(x - W / 2) / (W / 2)
        alpha = int(120 * (1 - t * t))  # quadratic falloff
        draw.point((x, 1), fill=(100, 90, 160, alpha))
        draw.point((x, 2), fill=(60, 55, 100, alpha // 2))

    img.save(os.path.join(OUT_DIR, "divider.png"))
    print("  divider.png (256x4)")


def make_icon_frame():
    """Frame for class icon / item icons (56x56)."""
    W, H = 56, 56
    R = 6

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Dark background
    draw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                           fill=(12, 12, 22, 200))
    # Outer border
    draw.rounded_rectangle([0, 0, W - 1, H - 1], radius=R,
                           outline=(100, 90, 150, 180), width=2)
    # Inner border
    draw.rounded_rectangle([2, 2, W - 3, H - 3], radius=R - 2,
                           outline=(50, 45, 80, 100), width=1)

    # Corner gems (tiny bright dots)
    for cx, cy in [(4, 4), (W - 5, 4), (4, H - 5), (W - 5, H - 5)]:
        draw.point((cx, cy), fill=(160, 140, 220, 180))

    img.save(os.path.join(OUT_DIR, "icon_frame.png"))
    print("  icon_frame.png (56x56)")


def make_vignette_overlay():
    """Subtle corner vignette for panels (256x256, tileable)."""
    W, H = 256, 256
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    for y in range(H):
        for x in range(W):
            # Distance from center, normalized
            dx = (x - W / 2) / (W / 2)
            dy = (y - H / 2) / (H / 2)
            dist = (dx * dx + dy * dy) ** 0.5
            if dist > 0.7:
                alpha = int(min(40, (dist - 0.7) / 0.3 * 40))
                img.putpixel((x, y), (0, 0, 0, alpha))

    img.save(os.path.join(OUT_DIR, "vignette.png"))
    print("  vignette.png (256x256)")


def make_glow_orb():
    """Small radial glow texture (64x64) for indicators, pickups, effects."""
    W, H = 64, 64
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    cx, cy = W // 2, H // 2
    for y in range(H):
        for x in range(W):
            dx = x - cx
            dy = y - cy
            dist = (dx * dx + dy * dy) ** 0.5
            max_dist = W / 2
            if dist < max_dist:
                t = dist / max_dist
                alpha = int(255 * (1 - t * t))  # quadratic falloff
                brightness = int(255 * (1 - t))
                img.putpixel((x, y), (brightness, brightness, brightness, alpha))

    img.save(os.path.join(OUT_DIR, "glow_orb.png"))
    print("  glow_orb.png (64x64)")


def make_noise_overlay():
    """Subtle film grain / noise overlay (128x128, tileable)."""
    import random
    random.seed(42)  # deterministic
    W, H = 128, 128
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    for y in range(H):
        for x in range(W):
            if random.random() < 0.15:  # 15% pixel coverage
                v = random.randint(180, 255)
                a = random.randint(4, 12)
                img.putpixel((x, y), (v, v, v, a))

    img.save(os.path.join(OUT_DIR, "noise.png"))
    print("  noise.png (128x128)")


if __name__ == "__main__":
    print("Generating UI textures for Riftwalker...")
    make_panel_dark()
    make_panel_light()
    make_bar_frame()
    make_minimap_frame()
    make_button("button_normal", (25, 22, 45), 35)
    make_button("button_hover", (40, 35, 70), 55)
    make_button("button_pressed", (20, 18, 38), 20, pressed=True)
    make_divider()
    make_icon_frame()
    make_glow_orb()
    make_noise_overlay()
    print(f"\nDone! {len(os.listdir(OUT_DIR))} textures in {OUT_DIR}")
