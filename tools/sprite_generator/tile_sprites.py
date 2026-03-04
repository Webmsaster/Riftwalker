"""
Riftwalker Universal Tileset Generator
Generates grayscale tiles for a sci-fi roguelike platformer.
Tiles are grayscale so they can be tinted per theme via SDL_SetTextureColorMod.

Layout (32x32 per frame):
  Row 0: 16 auto-tile solid variants (4-bit neighbor mask: top, right, bottom, left)
  Row 1: OneWay, Spike, Ice, Breakable, Fire x4
  Row 2: Conveyor-Right x4, Conveyor-Left x4
  Row 3: Laser emitter x4 dirs, Teleporter x4 anim
  Row 4: Crumbling x4 stages, GravityWell attract, repel
  Row 5: Decorations x8
"""
import random

from palette import TILE
from config import OUTPUT, TILE_SIZE
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_shaded_rect,
    draw_line, apply_outline, save_sheet,
)

# Tileset dimensions
COLS = 16
ROWS = 6
S = TILE_SIZE  # 32

# Seed for deterministic procedural detail
random.seed(42)


# ---------------------------------------------------------------------------
# Helper: add subtle noise/texture to a region
# ---------------------------------------------------------------------------
def _add_noise(frame, x0, y0, w, h, base_color, strength=10):
    """Add subtle per-pixel noise for visual interest."""
    r, g, b = base_color[:3]
    for py in range(y0, y0 + h):
        for px in range(x0, x0 + w):
            offset = random.randint(-strength, strength)
            nr = max(0, min(255, r + offset))
            ng = max(0, min(255, g + offset))
            nb = max(0, min(255, b + offset))
            draw_pixel(frame, px, py, (nr, ng, nb))


# ---------------------------------------------------------------------------
# Row 0: Auto-tile solid variants (16 variants, 4-bit neighbor mask)
# ---------------------------------------------------------------------------
def _draw_solid_tile(mask):
    """Draw a single auto-tile solid variant.

    mask bits: 0=top, 1=right, 2=bottom, 3=left
    Edge is drawn where there is NO neighbor.
    """
    frame = create_frame(S, S)
    has_top = bool(mask & 1)
    has_right = bool(mask & 2)
    has_bottom = bool(mask & 4)
    has_left = bool(mask & 8)

    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    # Fill entire tile with mid color base
    draw_rect(frame, 0, 0, S, S, mid)

    # Add subtle noise texture to interior
    _add_noise(frame, 2, 2, S - 4, S - 4, mid, strength=6)

    # Draw edges (1px dark border) where there is NO neighbor
    if not has_top:
        draw_rect(frame, 0, 0, S, 1, edge)
    if not has_bottom:
        draw_rect(frame, 0, S - 1, S, 1, edge)
    if not has_left:
        draw_rect(frame, 0, 0, 1, S, edge)
    if not has_right:
        draw_rect(frame, S - 1, 0, 1, S, edge)

    # Inner highlight on top-left edge where no neighbor
    # Top-left lighting: highlight near exposed top and left edges
    if not has_top:
        draw_rect(frame, 1 if not has_left else 0, 1, S - (1 if not has_left else 0) - (1 if not has_right else 0), 1, light)
    if not has_left:
        yt = 2 if not has_top else 0
        draw_rect(frame, 1, yt, 1, S - yt - (1 if not has_bottom else 0), light)

    # Top-left inner highlight pixel when both top and left are exposed
    if not has_top and not has_left:
        draw_pixel(frame, 2, 2, highlight)
        draw_pixel(frame, 3, 2, highlight)
        draw_pixel(frame, 2, 3, highlight)

    # Dark shadow near exposed bottom and right edges
    if not has_bottom:
        draw_rect(frame, 1 if not has_left else 0, S - 2, S - (1 if not has_left else 0) - (1 if not has_right else 0), 1, dark)
    if not has_right:
        yt = 1 if not has_top else 0
        draw_rect(frame, S - 2, yt, 1, S - yt - (1 if not has_bottom else 0), dark)

    # Seamless connection indicators: small detail lines where neighbors exist
    # This creates subtle visual continuity
    if has_top:
        for px in range(4, S - 4, 6):
            draw_pixel(frame, px, 0, dark)
    if has_bottom:
        for px in range(4, S - 4, 6):
            draw_pixel(frame, px, S - 1, dark)
    if has_left:
        for py in range(4, S - 4, 6):
            draw_pixel(frame, 0, py, dark)
    if has_right:
        for py in range(4, S - 4, 6):
            draw_pixel(frame, S - 1, py, dark)

    # Corner pixels: draw edge color at outer corners where two non-neighbor edges meet
    if not has_top and not has_left:
        draw_pixel(frame, 0, 0, edge)
    if not has_top and not has_right:
        draw_pixel(frame, S - 1, 0, edge)
    if not has_bottom and not has_left:
        draw_pixel(frame, 0, S - 1, edge)
    if not has_bottom and not has_right:
        draw_pixel(frame, S - 1, S - 1, edge)

    return frame


def _draw_row0_autotile(sheet):
    """Draw all 16 auto-tile solid variants into row 0."""
    for mask in range(16):
        frame = _draw_solid_tile(mask)
        paste_frame(sheet, frame, mask, 0, S, S)


# ---------------------------------------------------------------------------
# Row 1: OneWay, Spike, Ice, Breakable, Fire x4
# ---------------------------------------------------------------------------
def _draw_oneway(sheet):
    """OneWay platform: flat platform with dots on top, open below."""
    frame = create_frame(S, S)
    edge = TILE['edge']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    # Platform top section (upper 8px)
    draw_rect(frame, 0, 0, S, 8, mid)
    draw_rect(frame, 0, 0, S, 1, edge)
    draw_rect(frame, 0, 7, S, 1, edge)
    draw_rect(frame, 0, 1, S, 1, light)

    # Grip dots on top surface
    for dx in range(3, S - 3, 4):
        draw_pixel(frame, dx, 3, highlight)
        draw_pixel(frame, dx, 5, highlight)

    # Side edges
    draw_rect(frame, 0, 0, 1, 8, edge)
    draw_rect(frame, S - 1, 0, 1, 8, edge)

    paste_frame(sheet, frame, 0, 1, S, S)


def _draw_spike(sheet):
    """Spike tile: triangular points."""
    frame = create_frame(S, S)
    edge = TILE['edge']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    dark = TILE['solid_dark']

    # Base block (lower half)
    draw_rect(frame, 0, S // 2, S, S // 2, mid)
    draw_rect(frame, 0, S - 1, S, 1, edge)
    draw_rect(frame, 0, S // 2, 1, S // 2, edge)
    draw_rect(frame, S - 1, S // 2, 1, S // 2, edge)

    # Four spike triangles
    spike_w = 8
    num_spikes = S // spike_w
    for i in range(num_spikes):
        cx = i * spike_w + spike_w // 2
        base_y = S // 2
        tip_y = 2

        # Draw spike triangle (filled)
        for row in range(base_y - tip_y):
            y = tip_y + row
            half_w = (row * spike_w // 2) // (base_y - tip_y)
            for px in range(cx - half_w, cx + half_w + 1):
                if px == cx - half_w:
                    draw_pixel(frame, px, y, edge)
                elif px == cx + half_w:
                    draw_pixel(frame, px, y, dark)
                elif px <= cx:
                    draw_pixel(frame, px, y, light)
                else:
                    draw_pixel(frame, px, y, mid)

        # Tip highlight
        draw_pixel(frame, cx, tip_y, light)

    paste_frame(sheet, frame, 1, 1, S, S)


def _draw_ice(sheet):
    """Ice tile: smoother surface with diagonal shine streaks."""
    frame = create_frame(S, S)
    edge = TILE['edge']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    # Base fill - slightly lighter than normal
    draw_rect(frame, 0, 0, S, S, light)

    # Edges
    draw_rect(frame, 0, 0, S, 1, edge)
    draw_rect(frame, 0, S - 1, S, 1, edge)
    draw_rect(frame, 0, 0, 1, S, edge)
    draw_rect(frame, S - 1, 0, 1, S, edge)

    # Highlight inner top-left
    draw_rect(frame, 1, 1, S - 2, 1, highlight)
    draw_rect(frame, 1, 1, 1, S - 2, highlight)

    # Diagonal shine streaks
    for offset in [6, 14, 22]:
        for i in range(8):
            px = offset + i
            py = 2 + i
            if 1 <= px < S - 1 and 1 <= py < S - 1:
                draw_pixel(frame, px, py, highlight)
            # Second parallel streak
            px2 = offset + i + 1
            py2 = 2 + i
            if 1 <= px2 < S - 1 and 1 <= py2 < S - 1:
                draw_pixel(frame, px2, py2, (180, 180, 188))

    # Smooth gradient (no noise for ice - it's smooth)
    draw_rect(frame, 1, S - 2, S - 2, 1, mid)

    paste_frame(sheet, frame, 2, 1, S, S)


def _draw_breakable(sheet):
    """Breakable tile: cracked surface."""
    frame = create_frame(S, S)
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    crack = TILE['crack']

    # Base solid tile
    draw_rect(frame, 0, 0, S, S, mid)
    _add_noise(frame, 2, 2, S - 4, S - 4, mid, strength=5)

    # Edges
    draw_rect(frame, 0, 0, S, 1, edge)
    draw_rect(frame, 0, S - 1, S, 1, edge)
    draw_rect(frame, 0, 0, 1, S, edge)
    draw_rect(frame, S - 1, 0, 1, S, edge)
    draw_rect(frame, 1, 1, S - 2, 1, light)
    draw_rect(frame, 1, 1, 1, S - 2, light)

    # Crack patterns
    # Main diagonal crack
    for i in range(12):
        cx = 8 + i + random.randint(-1, 1)
        cy = 6 + i + random.randint(-1, 1)
        if 1 <= cx < S - 1 and 1 <= cy < S - 1:
            draw_pixel(frame, cx, cy, crack)
            draw_pixel(frame, cx + 1, cy, dark)

    # Secondary crack
    for i in range(8):
        cx = 18 + i + random.randint(-1, 0)
        cy = 10 - i // 2 + random.randint(0, 1)
        if 1 <= cx < S - 1 and 1 <= cy < S - 1:
            draw_pixel(frame, cx, cy, crack)

    # Small crack fragments
    for _ in range(5):
        fx = random.randint(4, S - 5)
        fy = random.randint(4, S - 5)
        draw_pixel(frame, fx, fy, crack)
        draw_pixel(frame, fx + 1, fy + 1, crack)

    paste_frame(sheet, frame, 3, 1, S, S)


def _draw_fire(sheet):
    """Fire tile: 4 animated flickering flame frames."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    for f_idx in range(4):
        frame = create_frame(S, S)

        # Base block (lower portion)
        draw_rect(frame, 0, S // 2, S, S // 2, dark)
        draw_rect(frame, 0, S - 1, S, 1, edge)
        draw_rect(frame, 0, S // 2, 1, S // 2, edge)
        draw_rect(frame, S - 1, S // 2, 1, S // 2, edge)

        # Flame shapes (varies per frame for animation)
        random.seed(42 + f_idx * 7)
        num_flames = 4
        flame_w = S // num_flames
        for i in range(num_flames):
            cx = i * flame_w + flame_w // 2
            base_y = S // 2
            # Vary flame height per frame
            tip_y = 3 + random.randint(-2, 3)
            height = base_y - tip_y

            for row in range(height):
                y = base_y - row
                # Flame narrows toward tip
                progress = row / max(1, height)
                half_w = int((flame_w // 2) * (1.0 - progress * 0.8))
                wiggle = random.randint(-1, 1) if progress > 0.3 else 0

                for px in range(cx - half_w + wiggle, cx + half_w + 1 + wiggle):
                    if 0 <= px < S and 0 <= y < S:
                        # Brightness gradient: brighter at base, lighter at tip
                        if progress < 0.3:
                            draw_pixel(frame, px, y, highlight)
                        elif progress < 0.6:
                            draw_pixel(frame, px, y, light)
                        elif progress < 0.85:
                            draw_pixel(frame, px, y, mid)
                        else:
                            draw_pixel(frame, px, y, dark)

        paste_frame(sheet, frame, 4 + f_idx, 1, S, S)


def _draw_row1(sheet):
    """Draw all Row 1 tiles."""
    _draw_oneway(sheet)
    _draw_spike(sheet)
    _draw_ice(sheet)
    _draw_breakable(sheet)
    _draw_fire(sheet)


# ---------------------------------------------------------------------------
# Row 2: Conveyor-Right x4, Conveyor-Left x4
# ---------------------------------------------------------------------------
def _draw_conveyor(sheet):
    """Conveyor belts with animated chevrons/arrows."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    for direction in range(2):  # 0 = right, 1 = left
        for f_idx in range(4):
            frame = create_frame(S, S)

            # Belt base
            draw_rect(frame, 0, 0, S, S, mid)

            # Top and bottom mechanical edges
            draw_rect(frame, 0, 0, S, 3, dark)
            draw_rect(frame, 0, S - 3, S, 3, dark)
            draw_rect(frame, 0, 0, S, 1, edge)
            draw_rect(frame, 0, S - 1, S, 1, edge)

            # Roller dots at edges
            for rx in range(4, S - 4, 8):
                draw_pixel(frame, rx, 1, light)
                draw_pixel(frame, rx, S - 2, light)

            # Chevron arrows (animated - shift per frame)
            chevron_spacing = 10
            offset = f_idx * 3 if direction == 0 else -(f_idx * 3)
            for ci in range(-1, 4):
                base_x = ci * chevron_spacing + offset
                # Draw chevron
                for row in range(6):
                    y = S // 2 - 3 + row
                    if direction == 0:
                        # Right-pointing chevron: >
                        px = base_x + (row if row < 3 else 5 - row)
                    else:
                        # Left-pointing chevron: <
                        px = base_x + (2 - row if row < 3 else row - 3)

                    # Wrap horizontally for seamless animation
                    px = px % S
                    if 0 <= y < S:
                        draw_pixel(frame, px, y, highlight)
                        # Chevron body
                        if direction == 0:
                            if px - 1 >= 0:
                                draw_pixel(frame, (px - 1) % S, y, light)
                        else:
                            if px + 1 < S:
                                draw_pixel(frame, (px + 1) % S, y, light)

            col = f_idx if direction == 0 else 4 + f_idx
            paste_frame(sheet, frame, col, 2, S, S)


def _draw_row2(sheet):
    """Draw all Row 2 tiles."""
    _draw_conveyor(sheet)


# ---------------------------------------------------------------------------
# Row 3: Laser emitter x4 dirs, Teleporter x4 anim
# ---------------------------------------------------------------------------
def _draw_laser_emitters(sheet):
    """Laser emitter blocks facing 4 directions: right, left, down, up."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    # Directions: 0=right, 1=left, 2=down, 3=up
    for d_idx in range(4):
        frame = create_frame(S, S)

        # Solid base block
        draw_shaded_rect(frame, 0, 0, S, S, dark, mid, light, highlight)
        draw_rect(frame, 0, 0, S, 1, edge)
        draw_rect(frame, 0, S - 1, S, 1, edge)
        draw_rect(frame, 0, 0, 1, S, edge)
        draw_rect(frame, S - 1, 0, 1, S, edge)

        # Emitter slot (bright slit where laser comes out)
        slot_color = highlight
        slot_inner = light

        if d_idx == 0:  # Right
            draw_rect(frame, S - 4, S // 2 - 3, 3, 6, slot_inner)
            draw_rect(frame, S - 2, S // 2 - 2, 1, 4, slot_color)
            draw_rect(frame, S - 1, S // 2 - 2, 1, 4, highlight)
        elif d_idx == 1:  # Left
            draw_rect(frame, 1, S // 2 - 3, 3, 6, slot_inner)
            draw_rect(frame, 1, S // 2 - 2, 1, 4, slot_color)
            draw_rect(frame, 0, S // 2 - 2, 1, 4, highlight)
        elif d_idx == 2:  # Down
            draw_rect(frame, S // 2 - 3, S - 4, 6, 3, slot_inner)
            draw_rect(frame, S // 2 - 2, S - 2, 4, 1, slot_color)
            draw_rect(frame, S // 2 - 2, S - 1, 4, 1, highlight)
        elif d_idx == 3:  # Up
            draw_rect(frame, S // 2 - 3, 1, 6, 3, slot_inner)
            draw_rect(frame, S // 2 - 2, 1, 4, 1, slot_color)
            draw_rect(frame, S // 2 - 2, 0, 4, 1, highlight)

        # Decorative tech detail on emitter body
        if d_idx in (0, 1):
            # Horizontal tech lines
            for ty in [8, 14, 20]:
                sx = 5 if d_idx == 0 else 8
                draw_rect(frame, sx, ty, 6, 1, dark)
        else:
            # Vertical tech lines
            for tx in [8, 14, 20]:
                sy = 5 if d_idx == 2 else 8
                draw_rect(frame, tx, sy, 1, 6, dark)

        paste_frame(sheet, frame, d_idx, 3, S, S)


def _draw_teleporter(sheet):
    """Teleporter: 4-frame swirling portal animation."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    import math

    for f_idx in range(4):
        frame = create_frame(S, S)
        cx, cy = S // 2, S // 2
        angle_offset = f_idx * (math.pi / 2)  # 90 degree rotation per frame

        # Concentric rings with rotation
        for radius in range(14, 2, -2):
            # Choose color based on ring layer
            if radius > 10:
                ring_color = dark
            elif radius > 6:
                ring_color = mid
            else:
                ring_color = light

            # Draw ring pixels
            num_points = max(16, radius * 4)
            for p in range(num_points):
                angle = (2 * math.pi * p / num_points) + angle_offset * (1 + (14 - radius) * 0.1)
                px = int(cx + radius * math.cos(angle))
                py = int(cy + radius * math.sin(angle))
                if 0 <= px < S and 0 <= py < S:
                    draw_pixel(frame, px, py, ring_color)

        # Spiral arms (rotating per frame)
        for arm in range(3):
            arm_angle = angle_offset + arm * (2 * math.pi / 3)
            for r in range(3, 13):
                angle = arm_angle + r * 0.15
                px = int(cx + r * math.cos(angle))
                py = int(cy + r * math.sin(angle))
                if 0 <= px < S and 0 <= py < S:
                    draw_pixel(frame, px, py, highlight)

        # Bright center
        for dx in range(-1, 2):
            for dy in range(-1, 2):
                draw_pixel(frame, cx + dx, cy + dy, highlight)
        draw_pixel(frame, cx, cy, (200, 200, 210))

        # Outer border ring
        for p in range(64):
            angle = 2 * math.pi * p / 64
            px = int(cx + 14 * math.cos(angle))
            py = int(cy + 14 * math.sin(angle))
            if 0 <= px < S and 0 <= py < S:
                draw_pixel(frame, px, py, edge)

        paste_frame(sheet, frame, 4 + f_idx, 3, S, S)


def _draw_row3(sheet):
    """Draw all Row 3 tiles."""
    _draw_laser_emitters(sheet)
    _draw_teleporter(sheet)


# ---------------------------------------------------------------------------
# Row 4: Crumbling x4 stages, GravityWell attract, repel
# ---------------------------------------------------------------------------
def _draw_crumbling(sheet):
    """Crumbling tile: 4 progressive crack stages."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']
    crack = TILE['crack']

    for stage in range(4):
        frame = create_frame(S, S)
        random.seed(42 + stage * 13)

        # Base solid tile (gets progressively damaged)
        draw_rect(frame, 0, 0, S, S, mid)
        _add_noise(frame, 2, 2, S - 4, S - 4, mid, strength=4)

        # Standard edges
        draw_rect(frame, 0, 0, S, 1, edge)
        draw_rect(frame, 0, S - 1, S, 1, edge)
        draw_rect(frame, 0, 0, 1, S, edge)
        draw_rect(frame, S - 1, 0, 1, S, edge)
        draw_rect(frame, 1, 1, S - 2, 1, light)
        draw_rect(frame, 1, 1, 1, S - 2, light)

        # Crack density increases with stage
        num_cracks = (stage + 1) * 3
        crack_len = 4 + stage * 3

        for _ in range(num_cracks):
            sx = random.randint(3, S - 4)
            sy = random.randint(3, S - 4)
            for i in range(crack_len):
                cx = sx + random.randint(-1, 1)
                cy = sy + i + random.randint(-1, 0)
                if 1 <= cx < S - 1 and 1 <= cy < S - 1:
                    draw_pixel(frame, cx, cy, crack)
                sx = cx

        # Missing chunks in later stages
        if stage >= 2:
            num_chunks = stage - 1
            for _ in range(num_chunks):
                chunk_x = random.randint(2, S - 8)
                chunk_y = random.randint(2, S - 8)
                chunk_s = random.randint(2, 4)
                for dx in range(chunk_s):
                    for dy in range(chunk_s):
                        draw_pixel(frame, chunk_x + dx, chunk_y + dy, dark)

        # Stage 3: pieces falling / large gaps
        if stage == 3:
            for _ in range(3):
                gx = random.randint(1, S - 6)
                gy = random.randint(1, S - 6)
                gw = random.randint(3, 6)
                gh = random.randint(3, 6)
                for dx in range(gw):
                    for dy in range(gh):
                        px = gx + dx
                        py = gy + dy
                        if 1 <= px < S - 1 and 1 <= py < S - 1:
                            # Transparent gaps
                            draw_pixel(frame, px, py, (0, 0, 0, 0))

        paste_frame(sheet, frame, stage, 4, S, S)


def _draw_gravity_well(sheet):
    """GravityWell: attract (col4) and repel (col5) with concentric circles."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']

    import math

    for mode in range(2):  # 0 = attract, 1 = repel
        frame = create_frame(S, S)
        cx, cy = S // 2, S // 2

        # Background base
        draw_rect(frame, 0, 0, S, S, dark)

        # Concentric circles
        for radius in range(2, 15, 2):
            # Alternate ring colors
            if radius % 4 == 0:
                ring_color = mid
            else:
                ring_color = dark if mode == 0 else light

            num_points = max(16, radius * 6)
            for p in range(num_points):
                angle = 2 * math.pi * p / num_points
                px = int(cx + radius * math.cos(angle))
                py = int(cy + radius * math.sin(angle))
                if 0 <= px < S and 0 <= py < S:
                    draw_pixel(frame, px, py, ring_color)

        # Directional indicators
        if mode == 0:  # Attract: arrows pointing inward
            arrows = [(cx - 10, cy, 1, 0), (cx + 10, cy, -1, 0),
                      (cx, cy - 10, 0, 1), (cx, cy + 10, 0, -1)]
            for ax, ay, adx, ady in arrows:
                for i in range(4):
                    draw_pixel(frame, ax + adx * i, ay + ady * i, highlight)
                # Arrow head
                if adx != 0:
                    draw_pixel(frame, ax + adx * 3, ay - 1, light)
                    draw_pixel(frame, ax + adx * 3, ay + 1, light)
                else:
                    draw_pixel(frame, ax - 1, ay + ady * 3, light)
                    draw_pixel(frame, ax + 1, ay + ady * 3, light)
        else:  # Repel: arrows pointing outward
            arrows = [(cx - 3, cy, -1, 0), (cx + 3, cy, 1, 0),
                      (cx, cy - 3, 0, -1), (cx, cy + 3, 0, 1)]
            for ax, ay, adx, ady in arrows:
                for i in range(4):
                    draw_pixel(frame, ax + adx * i, ay + ady * i, highlight)
                # Arrow head
                if adx != 0:
                    draw_pixel(frame, ax + adx * 3, ay - 1, light)
                    draw_pixel(frame, ax + adx * 3, ay + 1, light)
                else:
                    draw_pixel(frame, ax - 1, ay + ady * 3, light)
                    draw_pixel(frame, ax + 1, ay + ady * 3, light)

        # Bright center core
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                if abs(dx) + abs(dy) <= 2:
                    draw_pixel(frame, cx + dx, cy + dy, highlight if mode == 0 else light)
        draw_pixel(frame, cx, cy, (200, 200, 210))

        # Outer edge
        draw_rect(frame, 0, 0, S, 1, edge)
        draw_rect(frame, 0, S - 1, S, 1, edge)
        draw_rect(frame, 0, 0, 1, S, edge)
        draw_rect(frame, S - 1, 0, 1, S, edge)

        paste_frame(sheet, frame, 4 + mode, 4, S, S)


def _draw_row4(sheet):
    """Draw all Row 4 tiles."""
    _draw_crumbling(sheet)
    _draw_gravity_well(sheet)


# ---------------------------------------------------------------------------
# Row 5: Decorations x8
# ---------------------------------------------------------------------------
def _draw_decorations(sheet):
    """8 decoration tiles: cracks, moss, pipes, vents, wires, panels, rivets, grate."""
    edge = TILE['edge']
    dark = TILE['solid_dark']
    mid = TILE['solid_mid']
    light = TILE['solid_light']
    highlight = TILE['solid_highlight']
    crack = TILE['crack']

    # Col 0: Cracks
    frame = create_frame(S, S)
    random.seed(100)
    for _ in range(6):
        sx = random.randint(4, S - 4)
        sy = random.randint(2, S - 8)
        for i in range(random.randint(4, 10)):
            cx = sx + random.randint(-1, 1)
            cy = sy + i
            if 0 <= cx < S and 0 <= cy < S:
                draw_pixel(frame, cx, cy, crack)
                draw_pixel(frame, cx + 1, cy, dark)
            sx = cx
    paste_frame(sheet, frame, 0, 5, S, S)

    # Col 1: Moss
    frame = create_frame(S, S)
    random.seed(101)
    moss_mid = (90, 90, 98)
    moss_light = (110, 110, 118)
    # Moss patches growing from edges
    for _ in range(8):
        mx = random.randint(0, S - 1)
        my = random.choice([0, 1, 2])
        for dy in range(random.randint(3, 8)):
            for dx in range(-1, 2):
                px = mx + dx + random.randint(-1, 1)
                py = my + dy
                if 0 <= px < S and 0 <= py < S:
                    draw_pixel(frame, px, py, moss_mid if random.random() > 0.3 else moss_light)
    paste_frame(sheet, frame, 1, 5, S, S)

    # Col 2: Pipes
    frame = create_frame(S, S)
    # Horizontal pipes
    pipe_color = mid
    pipe_highlight = light
    pipe_shadow = dark
    for py in [6, 18]:
        draw_rect(frame, 0, py, S, 6, pipe_color)
        draw_rect(frame, 0, py, S, 1, pipe_highlight)
        draw_rect(frame, 0, py + 5, S, 1, pipe_shadow)
        draw_rect(frame, 0, py + 2, S, 1, pipe_highlight)
        # Pipe joints
        for jx in [0, 14, 28]:
            draw_rect(frame, jx, py - 1, 4, 8, pipe_shadow)
            draw_rect(frame, jx + 1, py, 2, 6, pipe_color)
    paste_frame(sheet, frame, 2, 5, S, S)

    # Col 3: Vents
    frame = create_frame(S, S)
    draw_rect(frame, 2, 2, S - 4, S - 4, dark)
    draw_rect(frame, 2, 2, S - 4, 1, edge)
    draw_rect(frame, 2, S - 3, S - 4, 1, edge)
    draw_rect(frame, 2, 2, 1, S - 4, edge)
    draw_rect(frame, S - 3, 2, 1, S - 4, edge)
    # Vent slats
    for vy in range(5, S - 5, 3):
        draw_rect(frame, 4, vy, S - 8, 2, mid)
        draw_rect(frame, 4, vy, S - 8, 1, light)
    # Corner screws
    for sx, sy in [(3, 3), (S - 5, 3), (3, S - 5), (S - 5, S - 5)]:
        draw_rect(frame, sx, sy, 2, 2, light)
        draw_pixel(frame, sx, sy, highlight)
    paste_frame(sheet, frame, 3, 5, S, S)

    # Col 4: Wires
    frame = create_frame(S, S)
    random.seed(104)
    wire_colors = [dark, mid, light]
    for i in range(4):
        wire_c = wire_colors[i % len(wire_colors)]
        sx = random.randint(4, S - 4)
        for py in range(S):
            wobble = random.randint(-1, 1) if py % 3 == 0 else 0
            wx = sx + wobble
            sx = max(1, min(S - 2, wx))
            draw_pixel(frame, sx, py, wire_c)
            draw_pixel(frame, sx + 1, py, dark)
    # Wire clips
    for clip_y in [6, 16, 26]:
        draw_rect(frame, 2, clip_y, S - 4, 2, mid)
    paste_frame(sheet, frame, 4, 5, S, S)

    # Col 5: Panels
    frame = create_frame(S, S)
    # Panel border
    draw_rect(frame, 0, 0, S, S, dark)
    # Two panel sections
    draw_shaded_rect(frame, 2, 2, S // 2 - 3, S - 4, dark, mid, light, highlight)
    draw_shaded_rect(frame, S // 2 + 1, 2, S // 2 - 3, S - 4, dark, mid, light, highlight)
    # Panel seam
    draw_rect(frame, S // 2 - 1, 1, 2, S - 2, edge)
    paste_frame(sheet, frame, 5, 5, S, S)

    # Col 6: Rivets
    frame = create_frame(S, S)
    # Base surface
    draw_rect(frame, 0, 0, S, S, mid)
    _add_noise(frame, 1, 1, S - 2, S - 2, mid, strength=4)
    # Rivet grid pattern
    for rx in range(4, S - 2, 7):
        for ry in range(4, S - 2, 7):
            # Rivet: bright pixel with shadow
            draw_pixel(frame, rx, ry, highlight)
            draw_pixel(frame, rx + 1, ry, light)
            draw_pixel(frame, rx, ry + 1, dark)
            draw_pixel(frame, rx + 1, ry + 1, dark)
    paste_frame(sheet, frame, 6, 5, S, S)

    # Col 7: Grate
    frame = create_frame(S, S)
    # Outer frame
    draw_rect(frame, 0, 0, S, S, dark)
    draw_rect(frame, 0, 0, S, 1, edge)
    draw_rect(frame, 0, S - 1, S, 1, edge)
    draw_rect(frame, 0, 0, 1, S, edge)
    draw_rect(frame, S - 1, 0, 1, S, edge)
    # Grid pattern with transparent holes
    for gx in range(2, S - 2, 4):
        for gy in range(2, S - 2, 4):
            # Grid bars
            draw_rect(frame, gx, gy, 4, 1, mid)
            draw_rect(frame, gx, gy, 1, 4, mid)
            # Holes (transparent or very dark)
            draw_rect(frame, gx + 1, gy + 1, 2, 2, edge)
    # Cross bars for structural detail
    draw_rect(frame, S // 2 - 1, 1, 2, S - 2, mid)
    draw_rect(frame, 1, S // 2 - 1, S - 2, 2, mid)
    paste_frame(sheet, frame, 7, 5, S, S)


def _draw_row5(sheet):
    """Draw all Row 5 tiles."""
    _draw_decorations(sheet)


# ---------------------------------------------------------------------------
# Main generation function
# ---------------------------------------------------------------------------
def generate_tiles():
    """Generate the universal tileset spritesheet."""
    print("Generating universal tileset...")

    sheet = create_sheet(S, S, COLS, ROWS)

    _draw_row0_autotile(sheet)
    _draw_row1(sheet)
    _draw_row2(sheet)
    _draw_row3(sheet)
    _draw_row4(sheet)
    _draw_row5(sheet)

    save_sheet(sheet, OUTPUT['tiles'], 'tileset_universal.png')
    print("  Tileset generation complete!")


if __name__ == '__main__':
    generate_tiles()
