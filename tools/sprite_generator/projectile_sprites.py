"""
Riftwalker Projectile Sprite Generator
Generates projectile spritesheet (3 rows x 4 columns).
Each row is a projectile type, each column is an animation frame (rotation/pulse).
"""
from palette import PROJECTILE, OUTLINE, TRANSPARENT
from config import OUTPUT, PROJECTILE_FRAME
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_circle,
    draw_outline_circle, save_sheet, rgba, shade,
)

FW, FH = PROJECTILE_FRAME  # 16x16
COLS = 4  # Animation frames
ROWS = 3  # Projectile types
CX, CY = FW // 2, FH // 2  # Center (8, 8)


def _draw_player_bullet(frame, anim_frame):
    """Blue energy bolt — small elongated projectile."""
    base = PROJECTILE['player_bullet']
    glow = PROJECTILE['player_glow']
    dark = shade(base, 0.6)
    light = shade(base, 1.3)

    # Glow trail behind bolt
    glow_alpha = 100 + anim_frame * 25
    draw_circle(frame, CX, CY, 4, (*glow[:3], min(255, glow_alpha)))

    # Bolt core: small horizontal elongated shape (5x3)
    draw_rect(frame, CX - 2, CY - 1, 5, 3, base)

    # Pointed front (right side, direction of travel)
    draw_pixel(frame, CX + 3, CY, base)
    draw_pixel(frame, CX + 4, CY, light)

    # Tapered back (left side)
    draw_pixel(frame, CX - 3, CY, dark)

    # Shading: top light, bottom dark
    draw_rect(frame, CX - 2, CY - 1, 5, 1, light)
    draw_rect(frame, CX - 2, CY + 1, 5, 1, dark)

    # Bright core center
    draw_pixel(frame, CX, CY, (255, 255, 255, 230))
    draw_pixel(frame, CX + 1, CY, (255, 255, 255, 180))

    # Outline on leading edge
    draw_pixel(frame, CX + 5, CY, OUTLINE)

    # Shimmer/pulse animation: small particles trailing
    trail_offsets = [
        (CX - 4, CY - 1), (CX - 4, CY + 1),
        (CX - 5, CY), (CX - 4, CY),
    ]
    tp = trail_offsets[anim_frame]
    draw_pixel(frame, tp[0], tp[1], (*glow[:3], 160))

    # Energy crackle (tiny pixel that moves)
    crackle_pos = [
        (CX + 2, CY - 2), (CX + 3, CY - 1),
        (CX + 2, CY + 2), (CX + 1, CY - 2),
    ]
    cp = crackle_pos[anim_frame]
    draw_pixel(frame, cp[0], cp[1], (255, 255, 255, 200))


def _draw_enemy_bullet(frame, anim_frame):
    """Red energy ball — round, menacing."""
    base = PROJECTILE['enemy_bullet']
    glow = PROJECTILE['enemy_glow']
    dark = shade(base, 0.5)
    light = shade(base, 1.3)

    # Outer glow (pulsing)
    glow_r = 5 + (anim_frame % 2)
    glow_alpha = 120 + anim_frame * 20
    draw_circle(frame, CX, CY, glow_r, (*glow[:3], min(255, glow_alpha)))

    # Ball body
    draw_outline_circle(frame, CX, CY, 3, base)

    # Inner shading: top-left light
    draw_pixel(frame, CX - 1, CY - 1, light)
    draw_pixel(frame, CX, CY - 1, light)
    draw_pixel(frame, CX - 1, CY, light)

    # Bottom-right shadow
    draw_pixel(frame, CX + 1, CY + 1, dark)
    draw_pixel(frame, CX, CY + 1, dark)

    # Hot core
    draw_pixel(frame, CX, CY, (255, 255, 255, 200))

    # Rotating sparks around the ball
    spark_positions = [
        [(CX - 3, CY - 3), (CX + 3, CY + 3)],
        [(CX + 3, CY - 3), (CX - 3, CY + 3)],
        [(CX + 4, CY), (CX - 4, CY)],
        [(CX, CY - 4), (CX, CY + 4)],
    ]
    for sp in spark_positions[anim_frame]:
        draw_pixel(frame, sp[0], sp[1], (*glow[:3], 200))


def _draw_boss_bullet(frame, anim_frame):
    """Purple large orb — bigger, more dramatic."""
    base = PROJECTILE['boss_bullet']
    glow = PROJECTILE['boss_glow']
    dark = shade(base, 0.5)
    light = shade(base, 1.3)

    # Large glow aura (pulsing)
    glow_alpha = 100 + anim_frame * 30
    draw_circle(frame, CX, CY, 6, (*glow[:3], min(255, glow_alpha)))

    # Outer ring
    draw_outline_circle(frame, CX, CY, 4, base)

    # Inner orb (brighter)
    draw_circle(frame, CX, CY, 2, light)

    # Core white hot center
    draw_pixel(frame, CX, CY, (255, 255, 255, 240))
    draw_pixel(frame, CX - 1, CY, (255, 255, 255, 160))
    draw_pixel(frame, CX, CY - 1, (255, 255, 255, 160))

    # Shading on outer ring
    draw_pixel(frame, CX + 3, CY + 2, dark)
    draw_pixel(frame, CX + 2, CY + 3, dark)
    draw_pixel(frame, CX - 2, CY - 3, light)
    draw_pixel(frame, CX - 3, CY - 2, light)

    # Rotating energy tendrils (4 directions, rotate per frame)
    tendril_sets = [
        [(CX, CY - 6), (CX + 6, CY), (CX, CY + 6), (CX - 6, CY)],
        [(CX + 4, CY - 4), (CX + 4, CY + 4), (CX - 4, CY + 4), (CX - 4, CY - 4)],
        [(CX + 1, CY - 6), (CX + 6, CY + 1), (CX - 1, CY + 6), (CX - 6, CY - 1)],
        [(CX - 4, CY - 5), (CX + 5, CY - 4), (CX + 4, CY + 5), (CX - 5, CY + 4)],
    ]
    for tp in tendril_sets[anim_frame]:
        draw_pixel(frame, tp[0], tp[1], (*glow[:3], 200))

    # Secondary inner ring sparkle
    inner_sparkle = [
        (CX + 1, CY - 2), (CX + 2, CY + 1),
        (CX - 1, CY + 2), (CX - 2, CY - 1),
    ]
    sp = inner_sparkle[anim_frame]
    draw_pixel(frame, sp[0], sp[1], (255, 255, 255, 180))


# Map row index to draw function
_DRAW_FUNCS = [
    _draw_player_bullet,  # Row 0
    _draw_enemy_bullet,   # Row 1
    _draw_boss_bullet,    # Row 2
]

_ROW_NAMES = ['player_bullet', 'enemy_bullet', 'boss_bullet']


def generate_projectiles():
    """Generate the projectiles spritesheet."""
    print("Generating projectile sprites...")
    sheet = create_sheet(FW, FH, COLS, ROWS)

    for row, draw_func in enumerate(_DRAW_FUNCS):
        for col in range(COLS):
            frame = create_frame(FW, FH)
            draw_func(frame, col)
            paste_frame(sheet, frame, col, row, FW, FH)
        print(f"  Row {row}: {_ROW_NAMES[row]} ({COLS} frames)")

    save_sheet(sheet, OUTPUT['projectiles'], 'projectiles.png')
    print("Projectile sprites complete!")


if __name__ == '__main__':
    generate_projectiles()
