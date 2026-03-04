"""
Riftwalker Sprite Generator Utilities
Drawing primitives for pixel art generation with Pillow.
"""
from PIL import Image, ImageDraw
from palette import OUTLINE, TRANSPARENT
import os


def create_sheet(frame_w, frame_h, cols, rows):
    """Create a transparent RGBA spritesheet image."""
    return Image.new('RGBA', (frame_w * cols, frame_h * rows), TRANSPARENT)


def create_frame(frame_w, frame_h):
    """Create a single transparent RGBA frame."""
    return Image.new('RGBA', (frame_w, frame_h), TRANSPARENT)


def paste_frame(sheet, frame, col, row, frame_w, frame_h):
    """Paste a frame into the spritesheet at grid position."""
    sheet.paste(frame, (col * frame_w, row * frame_h), frame)


def rgba(color, alpha=255):
    """Convert a 3-tuple RGB to 4-tuple RGBA."""
    if len(color) == 4:
        return color
    return (color[0], color[1], color[2], alpha)


def shade(base_color, factor):
    """Darken or lighten a color. factor < 1 = darker, > 1 = lighter."""
    r, g, b = base_color[:3]
    return (
        max(0, min(255, int(r * factor))),
        max(0, min(255, int(g * factor))),
        max(0, min(255, int(b * factor))),
    )


def draw_pixel(img, x, y, color):
    """Draw a single pixel on the image."""
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), rgba(color))


def draw_rect(img, x, y, w, h, color):
    """Draw a filled rectangle."""
    draw = ImageDraw.Draw(img)
    draw.rectangle([x, y, x + w - 1, y + h - 1], fill=rgba(color))


def draw_outline_rect(img, x, y, w, h, fill_color, outline_color=None):
    """Draw a filled rectangle with 1px outline."""
    if outline_color is None:
        outline_color = OUTLINE
    draw = ImageDraw.Draw(img)
    # Outline
    draw.rectangle([x, y, x + w - 1, y + h - 1], fill=rgba(outline_color))
    # Fill (1px inset)
    if w > 2 and h > 2:
        draw.rectangle([x + 1, y + 1, x + w - 2, y + h - 2], fill=rgba(fill_color))


def draw_circle(img, cx, cy, radius, color):
    """Draw a filled circle."""
    draw = ImageDraw.Draw(img)
    draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=rgba(color))


def draw_outline_circle(img, cx, cy, radius, fill_color, outline_color=None):
    """Draw a filled circle with 1px outline."""
    if outline_color is None:
        outline_color = OUTLINE
    draw = ImageDraw.Draw(img)
    draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius],
                 fill=rgba(fill_color), outline=rgba(outline_color))


def draw_line(img, x1, y1, x2, y2, color):
    """Draw a line between two points."""
    draw = ImageDraw.Draw(img)
    draw.line([(x1, y1), (x2, y2)], fill=rgba(color))


def draw_shaded_rect(img, x, y, w, h, dark, mid, light, highlight=None):
    """Draw a rectangle with top-left lighting shading (3-4 levels).
    Light comes from top-left."""
    # Base fill with mid color
    draw_rect(img, x, y, w, h, mid)

    # Top edge highlight
    draw_rect(img, x, y, w, 1, light if highlight is None else highlight)

    # Left edge highlight
    draw_rect(img, x, y + 1, 1, h - 1, light)

    # Bottom edge shadow
    draw_rect(img, x, y + h - 1, w, 1, dark)

    # Right edge shadow
    draw_rect(img, x + w - 1, y, 1, h - 1, dark)

    # Corner highlight (top-left)
    if highlight and w > 3 and h > 3:
        draw_pixel(img, x + 1, y + 1, highlight)


def draw_body_with_shading(img, x, y, w, h, colors, outline_color=None):
    """Draw an entity body with outline and 3-level shading.
    colors should have 'dark', 'mid', 'light' keys.
    Light from top-left."""
    if outline_color is None:
        outline_color = OUTLINE

    # Outline
    draw_rect(img, x, y, w, h, outline_color)

    # Inner body with shading
    if w > 2 and h > 2:
        inner_x, inner_y = x + 1, y + 1
        inner_w, inner_h = w - 2, h - 2

        # Base fill
        draw_rect(img, inner_x, inner_y, inner_w, inner_h, colors['mid'])

        # Top highlight
        draw_rect(img, inner_x, inner_y, inner_w, max(1, inner_h // 4), colors['light'])

        # Left highlight
        draw_rect(img, inner_x, inner_y, max(1, inner_w // 4), inner_h, colors['light'])

        # Bottom shadow
        draw_rect(img, inner_x, inner_y + inner_h - max(1, inner_h // 4),
                  inner_w, max(1, inner_h // 4), colors['dark'])

        # Right shadow
        draw_rect(img, inner_x + inner_w - max(1, inner_w // 4), inner_y,
                  max(1, inner_w // 4), inner_h, colors['dark'])

        # Highlight spot (top-left corner area)
        if 'highlight' in colors and inner_w > 4 and inner_h > 4:
            draw_pixel(img, inner_x + 1, inner_y + 1, colors['highlight'])
            draw_pixel(img, inner_x + 2, inner_y + 1, colors['highlight'])
            draw_pixel(img, inner_x + 1, inner_y + 2, colors['highlight'])


def draw_eye(img, x, y, size, eye_color, pupil_color=None):
    """Draw a pixel art eye."""
    if pupil_color is None:
        pupil_color = OUTLINE
    if size <= 2:
        draw_pixel(img, x, y, eye_color)
        if size == 2:
            draw_pixel(img, x + 1, y, eye_color)
    else:
        # Eye white/glow
        for dx in range(size):
            for dy in range(max(1, size - 1)):
                draw_pixel(img, x + dx, y + dy, eye_color)
        # Pupil
        px = x + size // 2
        py = y + max(0, (size - 1) // 2)
        draw_pixel(img, px, py, pupil_color)


def draw_legs(img, x, y, leg_w, leg_h, color, dark_color, spacing=0, offset=0):
    """Draw simple pixel art legs with walk cycle offset."""
    # Left leg
    lx = x
    ly = y + offset
    draw_rect(img, lx, ly, leg_w, leg_h, color)
    draw_rect(img, lx, ly + leg_h - 1, leg_w, 1, dark_color)

    # Right leg
    rx = x + leg_w + spacing
    ry = y - offset
    draw_rect(img, rx, ry, leg_w, leg_h, color)
    draw_rect(img, rx, ry + leg_h - 1, leg_w, 1, dark_color)


def apply_outline(img, outline_color=None):
    """Add 1px black outline around all non-transparent pixels."""
    if outline_color is None:
        outline_color = OUTLINE

    w, h = img.size
    pixels = img.load()
    outline_img = img.copy()
    out_pixels = outline_img.load()

    for y in range(h):
        for x in range(w):
            if pixels[x, y][3] == 0:  # Transparent pixel
                # Check if any neighbor is non-transparent
                for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and pixels[nx, ny][3] > 0:
                        out_pixels[x, y] = rgba(outline_color)
                        break

    return outline_img


def mirror_frame(frame):
    """Horizontally mirror a frame."""
    return frame.transpose(Image.FLIP_LEFT_RIGHT)


def save_sheet(img, output_dir, filename):
    """Save spritesheet to output directory, creating dirs if needed."""
    os.makedirs(output_dir, exist_ok=True)
    path = os.path.join(output_dir, filename)
    img.save(path, 'PNG')
    print(f"  Saved: {path}")
    return path


def lerp_color(c1, c2, t):
    """Linear interpolation between two colors."""
    return (
        int(c1[0] + (c2[0] - c1[0]) * t),
        int(c1[1] + (c2[1] - c1[1]) * t),
        int(c1[2] + (c2[2] - c1[2]) * t),
    )
