"""
Riftwalker Pickup Sprite Generator
Generates pickup spritesheet (5 rows x 4 columns).
Each row is a pickup type, each column is an animation frame (bob/glow).
"""
from palette import PICKUP, OUTLINE, TRANSPARENT
from config import OUTPUT, PICKUP_FRAME
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_circle,
    draw_outline_circle, save_sheet, rgba, shade,
)

FW, FH = PICKUP_FRAME  # 16x16
COLS = 4  # Animation frames
ROWS = 5  # Pickup types
CX, CY = FW // 2, FH // 2  # Center (8, 8)


def _draw_health(frame, anim_frame):
    """Red cross/heart, pulsing glow."""
    base = PICKUP['health']
    glow = PICKUP['health_glow']
    t = anim_frame / COLS  # 0.0 - 0.75

    # Glow pulse: expand/contract glow behind cross
    glow_r = 5 + int(t * 2)
    glow_alpha = 180 - int(t * 80)
    draw_circle(frame, CX, CY, glow_r, (*glow[:3], glow_alpha))

    # Cross shape (5px wide, 3px thick arms)
    # Vertical bar
    draw_rect(frame, CX - 1, CY - 3, 3, 7, base)
    # Horizontal bar
    draw_rect(frame, CX - 3, CY - 1, 7, 3, base)

    # Outline pixels on cross
    draw_pixel(frame, CX - 1, CY - 3, OUTLINE)
    draw_pixel(frame, CX + 1, CY - 3, OUTLINE)
    draw_pixel(frame, CX - 1, CY + 3, OUTLINE)
    draw_pixel(frame, CX + 1, CY + 3, OUTLINE)
    draw_pixel(frame, CX - 3, CY - 1, OUTLINE)
    draw_pixel(frame, CX - 3, CY + 1, OUTLINE)
    draw_pixel(frame, CX + 3, CY - 1, OUTLINE)
    draw_pixel(frame, CX + 3, CY + 1, OUTLINE)

    # Highlight on cross center
    highlight = shade(base, 1.3)
    draw_pixel(frame, CX, CY - 1, highlight)
    draw_pixel(frame, CX - 1, CY, highlight)

    # Pulsing shimmer pixel
    shimmer_offsets = [(0, -2), (1, -1), (0, 0), (-1, -1)]
    sx, sy = shimmer_offsets[anim_frame]
    draw_pixel(frame, CX + sx, CY + sy, (255, 255, 255, 200))


def _draw_energy(frame, anim_frame):
    """Blue diamond/crystal shape."""
    base = PICKUP['energy']
    glow = PICKUP['energy_glow']
    t = anim_frame / COLS

    # Glow behind diamond
    glow_alpha = 150 + int(t * 60)
    draw_circle(frame, CX, CY, 5, (*glow[:3], min(255, glow_alpha)))

    # Diamond shape (rotated square): draw pixel by pixel
    dark = shade(base, 0.7)
    light = shade(base, 1.3)

    # Diamond body: 5px tall, widest at center
    diamond_rows = [
        (CX, CX, CY - 3),      # 1px top
        (CX - 1, CX + 1, CY - 2),  # 3px
        (CX - 2, CX + 2, CY - 1),  # 5px
        (CX - 3, CX + 3, CY),      # 7px center
        (CX - 2, CX + 2, CY + 1),  # 5px
        (CX - 1, CX + 1, CY + 2),  # 3px
        (CX, CX, CY + 3),      # 1px bottom
    ]

    for left, right, y in diamond_rows:
        for x in range(left, right + 1):
            if x == left or y == CY + 3:
                draw_pixel(frame, x, y, dark)
            elif x == left + 1 and y < CY:
                draw_pixel(frame, x, y, light)
            else:
                draw_pixel(frame, x, y, base)

    # Outline: top and bottom tips, left and right tips
    draw_pixel(frame, CX, CY - 4, OUTLINE)
    draw_pixel(frame, CX, CY + 4, OUTLINE)

    # Inner sparkle that moves per frame
    sparkle_positions = [
        (CX - 1, CY - 1), (CX + 1, CY - 1),
        (CX + 1, CY + 1), (CX - 1, CY + 1),
    ]
    sp = sparkle_positions[anim_frame]
    draw_pixel(frame, sp[0], sp[1], (255, 255, 255, 220))


def _draw_shard(frame, anim_frame):
    """Purple rift shard crystal."""
    base = PICKUP['shard']
    glow = PICKUP['shard_glow']

    # Glow aura
    glow_alpha = 120 + anim_frame * 30
    draw_circle(frame, CX, CY, 5, (*glow[:3], min(255, glow_alpha)))

    dark = shade(base, 0.6)
    light = shade(base, 1.2)

    # Crystal body: irregular angular shard
    # Main crystal body (tall and narrow)
    draw_rect(frame, CX - 1, CY - 4, 3, 8, base)

    # Wider middle
    draw_rect(frame, CX - 2, CY - 2, 5, 4, base)

    # Shading: left light, right dark
    draw_rect(frame, CX - 2, CY - 2, 1, 4, light)
    draw_rect(frame, CX - 1, CY - 4, 1, 2, light)
    draw_rect(frame, CX + 2, CY - 2, 1, 4, dark)
    draw_rect(frame, CX + 1, CY + 2, 1, 2, dark)

    # Outline on tips
    draw_pixel(frame, CX, CY - 5, OUTLINE)
    draw_pixel(frame, CX, CY + 4, OUTLINE)

    # Rotating shimmer
    shimmer_pos = [
        (CX - 1, CY - 2), (CX + 1, CY - 1),
        (CX + 1, CY + 1), (CX - 1, CY + 1),
    ]
    sp = shimmer_pos[anim_frame]
    draw_pixel(frame, sp[0], sp[1], (255, 255, 255, 200))
    # Secondary shimmer (dimmer, offset)
    sp2 = shimmer_pos[(anim_frame + 2) % 4]
    draw_pixel(frame, sp2[0], sp2[1], (*glow[:3], 200))


def _draw_coin(frame, anim_frame):
    """Gold coin with spinning animation (width changes per frame)."""
    base = PICKUP['coin']
    glow = PICKUP['coin_glow']
    dark = shade(base, 0.6)
    light = shade(base, 1.2)

    # Spinning effect: coin width varies per frame
    # Frame 0: full front, 1: 3/4, 2: edge, 3: 3/4 back
    widths = [3, 2, 1, 2]
    coin_w = widths[anim_frame]

    # Subtle glow
    draw_circle(frame, CX, CY, 4, (*glow[:3], 100))

    # Coin body (elliptical)
    x_off = CX - coin_w
    w = coin_w * 2 + 1
    draw_rect(frame, x_off, CY - 3, w, 7, base)
    # Rounded top and bottom
    if coin_w >= 2:
        draw_rect(frame, x_off + 1, CY - 4, w - 2, 1, base)
        draw_rect(frame, x_off + 1, CY + 4, w - 2, 1, base)

    # Shading
    if coin_w >= 2:
        draw_rect(frame, x_off, CY - 3, 1, 7, dark)
        draw_rect(frame, x_off + w - 1, CY - 3, 1, 7, dark)
        # Top highlight
        draw_rect(frame, x_off + 1, CY - 3, max(1, w - 2), 1, light)

    # Edge frame: just a thin line
    if coin_w == 1:
        draw_rect(frame, CX, CY - 4, 1, 9, dark)
        draw_pixel(frame, CX, CY - 3, light)

    # Outline top/bottom
    if coin_w >= 2:
        draw_pixel(frame, CX, CY - 5, OUTLINE)
        draw_pixel(frame, CX, CY + 5, OUTLINE)

    # $ symbol on front face (frame 0 and 3)
    if anim_frame == 0:
        draw_pixel(frame, CX, CY - 1, dark)
        draw_pixel(frame, CX, CY, dark)
        draw_pixel(frame, CX, CY + 1, dark)

    # Shine
    shine_positions = [
        (CX - 1, CY - 2), (CX, CY - 2),
        (CX, CY - 1), (CX - 1, CY - 1),
    ]
    if coin_w >= 2:
        sp = shine_positions[anim_frame]
        draw_pixel(frame, sp[0], sp[1], (255, 255, 255, 220))


def _draw_powerup(frame, anim_frame):
    """Green star/arrow-up powerup."""
    base = PICKUP['powerup']
    glow = PICKUP['powerup_glow']
    dark = shade(base, 0.6)
    light = shade(base, 1.3)

    # Glow aura
    glow_alpha = 130 + anim_frame * 25
    draw_circle(frame, CX, CY, 5, (*glow[:3], min(255, glow_alpha)))

    # Arrow-up / star shape
    # Upward arrow body
    draw_rect(frame, CX - 1, CY - 1, 3, 5, base)  # Shaft

    # Arrow head (triangle pointing up)
    draw_rect(frame, CX - 3, CY - 1, 7, 1, base)
    draw_rect(frame, CX - 2, CY - 2, 5, 1, base)
    draw_rect(frame, CX - 1, CY - 3, 3, 1, base)
    draw_pixel(frame, CX, CY - 4, base)

    # Shading: left bright, right dark
    draw_pixel(frame, CX - 1, CY, light)
    draw_pixel(frame, CX - 1, CY + 1, light)
    draw_pixel(frame, CX - 2, CY - 2, light)
    draw_pixel(frame, CX + 1, CY + 2, dark)
    draw_pixel(frame, CX + 1, CY + 3, dark)
    draw_pixel(frame, CX + 2, CY - 2, dark)

    # Outline on tip
    draw_pixel(frame, CX, CY - 5, OUTLINE)

    # Shimmer animation: small sparkles rotating around
    sparkle_offsets = [
        (CX - 3, CY - 3), (CX + 3, CY - 2),
        (CX + 3, CY + 2), (CX - 3, CY + 2),
    ]
    sp = sparkle_offsets[anim_frame]
    draw_pixel(frame, sp[0], sp[1], (255, 255, 255, 220))
    # Cross sparkle pattern
    draw_pixel(frame, sp[0] + 1, sp[1], (*glow[:3], 150))
    draw_pixel(frame, sp[0] - 1, sp[1], (*glow[:3], 150))


# Map row index to draw function
_DRAW_FUNCS = [
    _draw_health,   # Row 0
    _draw_energy,   # Row 1
    _draw_shard,    # Row 2
    _draw_coin,     # Row 3
    _draw_powerup,  # Row 4
]

_ROW_NAMES = ['health', 'energy', 'rift_shard', 'coin', 'powerup']


def generate_pickups():
    """Generate the pickups spritesheet."""
    print("Generating pickup sprites...")
    sheet = create_sheet(FW, FH, COLS, ROWS)

    for row, draw_func in enumerate(_DRAW_FUNCS):
        for col in range(COLS):
            frame = create_frame(FW, FH)
            draw_func(frame, col)
            paste_frame(sheet, frame, col, row, FW, FH)
        print(f"  Row {row}: {_ROW_NAMES[row]} ({COLS} frames)")

    save_sheet(sheet, OUTPUT['pickups'], 'pickups.png')
    print("Pickup sprites complete!")


if __name__ == '__main__':
    generate_pickups()
