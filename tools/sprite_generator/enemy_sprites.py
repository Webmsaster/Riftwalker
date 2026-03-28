"""
Riftwalker Enemy Spritesheet Generator
Generates 32x32 spritesheets for all 11 enemy types.
Each sheet has 5 animation rows: idle, walk, attack, hurt, dead.
"""
from palette import ENEMY_COLORS, OUTLINE, TRANSPARENT
from config import OUTPUT, ENEMY_FRAME
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_body_with_shading, draw_eye,
    draw_outline_rect, apply_outline, save_sheet,
    draw_circle, draw_outline_circle,
    rgba, shade,
)

# Frame layout per enemy
COLS = 6  # max frames per row (walk has 6)
ROWS = 5  # idle, walk, attack, hurt, dead
FW, FH = ENEMY_FRAME  # 32, 32


# ---------------------------------------------------------------------------
# Helper: build a full sheet from per-row drawing callbacks
# ---------------------------------------------------------------------------

def _build_sheet(name, draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead):
    """Create a spritesheet by calling row-drawing callbacks."""
    sheet = create_sheet(FW, FH, COLS, ROWS)
    c = ENEMY_COLORS[name]

    # Row 0: Idle (4 frames)
    for f in range(4):
        frame = create_frame(FW, FH)
        draw_idle(frame, c, f)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, FW, FH)

    # Row 1: Walk (6 frames)
    for f in range(6):
        frame = create_frame(FW, FH)
        draw_walk(frame, c, f)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, FW, FH)

    # Row 2: Attack (4 frames)
    for f in range(4):
        frame = create_frame(FW, FH)
        draw_attack(frame, c, f)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, FW, FH)

    # Row 3: Hurt (2 frames)
    for f in range(2):
        frame = create_frame(FW, FH)
        draw_hurt(frame, c, f)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, FW, FH)

    # Row 4: Dead (4 frames)
    for f in range(4):
        frame = create_frame(FW, FH)
        draw_dead(frame, c, f)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, FW, FH)

    save_sheet(sheet, OUTPUT['enemies'], f"{name}.png")


# ---------------------------------------------------------------------------
# 1. WALKER - Red robot, boxy body, two legs, single eye
# ---------------------------------------------------------------------------

def generate_walker():
    def draw_idle(img, c, f):
        bob = [0, -1, 0, -1][f]
        # Body (boxy)
        draw_body_with_shading(img, 10, 10 + bob, 12, 12, c)
        # Single eye
        draw_eye(img, 14, 14 + bob, 3, c['eye'])
        # Left leg
        draw_rect(img, 12, 22 + bob, 3, 6, c['dark'])
        draw_rect(img, 12, 22 + bob, 3, 2, c['mid'])
        # Right leg
        draw_rect(img, 17, 22 + bob, 3, 6, c['dark'])
        draw_rect(img, 17, 22 + bob, 3, 2, c['mid'])
        # Feet
        draw_rect(img, 11, 27 + bob, 4, 2, c['dark'])
        draw_rect(img, 17, 27 + bob, 4, 2, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_offsets = [0, 2, 3, 2, 0, -1][f]
        # Body
        draw_body_with_shading(img, 10, 10 + bob, 12, 12, c)
        # Eye
        draw_eye(img, 14, 14 + bob, 3, c['eye'])
        # Left leg (offset forward)
        ly = 22 + bob + leg_offsets
        draw_rect(img, 12, ly, 3, 6, c['dark'])
        draw_rect(img, 12, ly, 3, 2, c['mid'])
        draw_rect(img, 11, ly + 5, 4, 2, c['dark'])
        # Right leg (offset backward)
        ry = 22 + bob - leg_offsets
        draw_rect(img, 17, ry, 3, 6, c['dark'])
        draw_rect(img, 17, ry, 3, 2, c['mid'])
        draw_rect(img, 17, ry + 5, 4, 2, c['dark'])

    def draw_attack(img, c, f):
        # Body leans forward in attack
        lean = [0, 2, 3, 1][f]
        draw_body_with_shading(img, 10 + lean, 10, 12, 12, c)
        # Eye glows bigger during attack
        eye_size = [3, 3, 4, 3][f]
        draw_eye(img, 14 + lean, 14, eye_size, c['eye'])
        # Legs stay planted
        draw_rect(img, 12, 22, 3, 6, c['dark'])
        draw_rect(img, 17, 22, 3, 6, c['dark'])
        draw_rect(img, 11, 27, 4, 2, c['dark'])
        draw_rect(img, 17, 27, 4, 2, c['dark'])
        # Attack arm/punch extension
        ax = 22 + lean + [0, 2, 4, 2][f]
        draw_rect(img, ax, 14, 4, 3, c['light'])

    def draw_hurt(img, c, f):
        # Recoil backward
        recoil = [2, 4][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(img, 10 - recoil, 10, 12, 12,
                               {'dark': c['dark'], 'mid': flash,
                                'light': c['highlight'], 'highlight': c['highlight']})
        draw_eye(img, 14 - recoil, 14, 2, c['eye'])
        draw_rect(img, 12, 22, 3, 6, c['dark'])
        draw_rect(img, 17, 22, 3, 6, c['dark'])

    def draw_dead(img, c, f):
        # Collapse sequence: tilt and flatten
        squash = [0, 2, 4, 6][f]
        alpha_factor = [1.0, 0.8, 0.5, 0.3][f]
        body_h = max(2, 12 - squash)
        body_y = 10 + squash
        col = shade(c['mid'], alpha_factor)
        draw_rect(img, 10, body_y, 12, body_h, col)
        draw_rect(img, 10, body_y, 12, max(1, body_h // 3), shade(c['light'], alpha_factor))
        if f < 3:
            draw_eye(img, 14, body_y + body_h // 2, 2, c['eye'])
        # Debris on later frames
        if f >= 2:
            draw_pixel(img, 8, 26, c['dark'])
            draw_pixel(img, 24, 24, c['dark'])
        if f == 3:
            draw_pixel(img, 6, 28, c['dark'])
            draw_pixel(img, 26, 27, c['mid'])

    _build_sheet('walker', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 2. FLYER - Purple winged creature, small body, bat-like wings
# ---------------------------------------------------------------------------

def generate_flyer():
    def _draw_wing(img, x, y, spread, c, flip=False):
        """Draw a bat-like wing. spread controls openness (0-3)."""
        wing_w = 5 + spread
        wing_h = 3 + spread // 2
        if flip:
            draw_rect(img, x - wing_w, y, wing_w, wing_h, c['mid'])
            draw_rect(img, x - wing_w, y, wing_w, 1, c['light'])
            # Wing tip
            draw_pixel(img, x - wing_w, y + wing_h, c['dark'])
            draw_pixel(img, x - wing_w + 2, y + wing_h, c['dark'])
        else:
            draw_rect(img, x, y, wing_w, wing_h, c['mid'])
            draw_rect(img, x, y, wing_w, 1, c['light'])
            draw_pixel(img, x + wing_w - 1, y + wing_h, c['dark'])
            draw_pixel(img, x + wing_w - 3, y + wing_h, c['dark'])

    def draw_idle(img, c, f):
        bob = [0, -1, -2, -1][f]
        wing_spread = [1, 2, 3, 2][f]
        # Small body
        draw_body_with_shading(img, 13, 14 + bob, 6, 7, c)
        # Eyes
        draw_eye(img, 14, 16 + bob, 2, c['eye'])
        draw_eye(img, 17, 16 + bob, 2, c['eye'])
        # Wings
        _draw_wing(img, 19, 13 + bob, wing_spread, c)
        _draw_wing(img, 13, 13 + bob, wing_spread, c, flip=True)

    def draw_walk(img, c, f):
        # Flyer "walks" by hovering with wing flaps
        bob = [0, -2, -3, -2, 0, 1][f]
        wing_spread = [2, 3, 2, 1, 0, 1][f]
        x_drift = [0, 1, 2, 2, 1, 0][f]
        draw_body_with_shading(img, 13 + x_drift, 14 + bob, 6, 7, c)
        draw_eye(img, 14 + x_drift, 16 + bob, 2, c['eye'])
        draw_eye(img, 17 + x_drift, 16 + bob, 2, c['eye'])
        _draw_wing(img, 19 + x_drift, 13 + bob, wing_spread, c)
        _draw_wing(img, 13 + x_drift, 13 + bob, wing_spread, c, flip=True)

    def draw_attack(img, c, f):
        # Dive attack
        dive = [0, 2, 4, 2][f]
        draw_body_with_shading(img, 13, 14 + dive, 6, 7, c)
        draw_eye(img, 14, 16 + dive, 2, c['eye'])
        draw_eye(img, 17, 16 + dive, 2, c['eye'])
        # Wings swept back
        _draw_wing(img, 19, 12 + dive, 4, c)
        _draw_wing(img, 13, 12 + dive, 4, c, flip=True)
        # Talons extended
        if f >= 1:
            draw_pixel(img, 14, 22 + dive, c['dark'])
            draw_pixel(img, 18, 22 + dive, c['dark'])

    def draw_hurt(img, c, f):
        recoil_y = [0, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight']}
        draw_body_with_shading(img, 13, 14 + recoil_y, 6, 7, cols)
        draw_eye(img, 15, 16 + recoil_y, 1, c['eye'])
        _draw_wing(img, 19, 15 + recoil_y, 0, c)
        _draw_wing(img, 13, 15 + recoil_y, 0, c, flip=True)

    def draw_dead(img, c, f):
        fall_y = [0, 3, 6, 9][f]
        alpha = [1.0, 0.8, 0.5, 0.3][f]
        col = shade(c['mid'], alpha)
        body_y = min(14 + fall_y, 26)
        draw_rect(img, 13, body_y, 6, max(2, 7 - f * 2), col)
        # Folded wings
        if f < 3:
            draw_rect(img, 11, body_y, 2, 3, shade(c['dark'], alpha))
            draw_rect(img, 19, body_y, 2, 3, shade(c['dark'], alpha))
        if f >= 2:
            draw_pixel(img, 10, body_y + 2, c['dark'])
            draw_pixel(img, 22, body_y + 1, c['dark'])

    _build_sheet('flyer', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 3. TURRET - Yellow stationary gun platform, barrel that rotates
# ---------------------------------------------------------------------------

def generate_turret():
    def _draw_base(img, c, y_off=0):
        """Draw the turret base platform."""
        draw_body_with_shading(img, 8, 22 + y_off, 16, 6, c)
        # Platform feet
        draw_rect(img, 7, 27 + y_off, 18, 3, c['dark'])
        draw_rect(img, 7, 27 + y_off, 18, 1, c['mid'])

    def _draw_barrel(img, c, angle_frame, y_off=0):
        """Draw the rotating barrel. angle_frame 0-3 for slight rotation."""
        # Turret head
        draw_body_with_shading(img, 11, 14 + y_off, 10, 8, c)
        # Eye/sensor
        draw_eye(img, 18, 17 + y_off, 2, c['eye'])
        # Barrel
        barrel_y = 16 + y_off + [-1, 0, 1, 0][angle_frame]
        barrel_len = 8
        draw_rect(img, 21, barrel_y, barrel_len, 3, c['dark'])
        draw_rect(img, 21, barrel_y, barrel_len, 1, c['mid'])
        # Barrel tip
        draw_pixel(img, 21 + barrel_len - 1, barrel_y, c['light'])

    def draw_idle(img, c, f):
        _draw_base(img, c)
        _draw_barrel(img, c, [0, 0, 1, 1][f])

    def draw_walk(img, c, f):
        # Turret doesn't walk, but barrel scans left-right
        _draw_base(img, c)
        _draw_barrel(img, c, [0, 1, 2, 3, 2, 1][f])

    def draw_attack(img, c, f):
        _draw_base(img, c)
        _draw_barrel(img, c, 0)
        # Muzzle flash
        if f == 1 or f == 2:
            flash_x = 29
            draw_pixel(img, flash_x, 15, c['highlight'])
            draw_pixel(img, flash_x + 1, 16, c['eye'])
            draw_pixel(img, flash_x, 17, c['highlight'])
            if f == 2:
                draw_pixel(img, flash_x + 1, 15, c['eye'])
                draw_pixel(img, flash_x + 2, 16, c['highlight'])

    def draw_hurt(img, c, f):
        recoil = [1, 2][f]
        flash = c['highlight'] if f == 0 else c['mid']
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        _draw_base(img, cols)
        draw_body_with_shading(img, 11 - recoil, 14, 10, 8, cols)
        draw_eye(img, 18 - recoil, 17, 2, c['eye'])

    def draw_dead(img, c, f):
        # Turret crumbles
        squash = [0, 2, 4, 5][f]
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        # Base stays
        draw_rect(img, 7, 27, 18, 3, shade(c['dark'], alpha))
        # Head collapses
        head_y = 14 + squash
        head_h = max(2, 8 - squash)
        draw_rect(img, 11, head_y, 10, head_h, shade(c['mid'], alpha))
        # Barrel falls
        if f < 3:
            draw_rect(img, 21, head_y + squash, max(2, 8 - f * 2), 2, shade(c['dark'], alpha))
        # Debris
        if f >= 2:
            draw_pixel(img, 8, 25, c['dark'])
            draw_pixel(img, 24, 26, c['mid'])

    _build_sheet('turret', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 4. CHARGER - Orange bull-like creature, wide body, horns
# ---------------------------------------------------------------------------

def generate_charger():
    def _draw_horns(img, c, x, y, spread=0):
        """Draw bull horns."""
        # Left horn
        draw_rect(img, x - 1 - spread, y, 2, 1, c['light'])
        draw_pixel(img, x - 2 - spread, y - 1, c['highlight'])
        # Right horn
        draw_rect(img, x + 13 + spread, y, 2, 1, c['light'])
        draw_pixel(img, x + 14 + spread, y - 1, c['highlight'])

    def draw_idle(img, c, f):
        bob = [0, 0, -1, 0][f]
        bx, by = 8, 12 + bob
        # Wide body
        draw_body_with_shading(img, bx, by, 16, 10, c)
        # Horns
        _draw_horns(img, c, bx, by - 1)
        # Eyes
        draw_eye(img, bx + 11, by + 3, 2, c['eye'])
        draw_eye(img, bx + 13, by + 3, 2, c['eye'])
        # Legs (thick)
        draw_rect(img, bx + 1, by + 10, 4, 6, c['dark'])
        draw_rect(img, bx + 1, by + 10, 4, 2, c['mid'])
        draw_rect(img, bx + 11, by + 10, 4, 6, c['dark'])
        draw_rect(img, bx + 11, by + 10, 4, 2, c['mid'])
        # Hooves
        draw_rect(img, bx, by + 15, 5, 2, c['dark'])
        draw_rect(img, bx + 11, by + 15, 5, 2, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_off = [0, 2, 3, 2, 0, -1][f]
        bx, by = 8, 12 + bob
        draw_body_with_shading(img, bx, by, 16, 10, c)
        _draw_horns(img, c, bx, by - 1)
        draw_eye(img, bx + 11, by + 3, 2, c['eye'])
        draw_eye(img, bx + 13, by + 3, 2, c['eye'])
        # Legs with walk cycle
        draw_rect(img, bx + 1, by + 10 + leg_off, 4, 6, c['dark'])
        draw_rect(img, bx + 1, by + 10 + leg_off, 4, 2, c['mid'])
        draw_rect(img, bx + 11, by + 10 - leg_off, 4, 6, c['dark'])
        draw_rect(img, bx + 11, by + 10 - leg_off, 4, 2, c['mid'])

    def draw_attack(img, c, f):
        # Charge forward - body leans, head down
        lean = [0, 3, 6, 4][f]
        head_dip = [0, 1, 2, 1][f]
        bx, by = 8 + lean, 12 + head_dip
        draw_body_with_shading(img, bx, by, 16, 10, c)
        _draw_horns(img, c, bx, by - 1, spread=[0, 1, 2, 1][f])
        draw_eye(img, bx + 11, by + 3, 2, c['eye'])
        # Legs planted
        draw_rect(img, 9, 22, 4, 6, c['dark'])
        draw_rect(img, 19, 22, 4, 6, c['dark'])
        # Dust trail on charge
        if f >= 2:
            draw_pixel(img, 6, 27, c['highlight'])
            draw_pixel(img, 4, 26, c['light'])

    def draw_hurt(img, c, f):
        recoil = [2, 5][f]
        flash = c['highlight'] if f == 0 else c['mid']
        bx = 8 - recoil
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, bx, 12, 16, 10, cols)
        _draw_horns(img, cols, bx, 11)
        draw_rect(img, bx + 1, 22, 4, 6, c['dark'])
        draw_rect(img, bx + 11, 22, 4, 6, c['dark'])

    def draw_dead(img, c, f):
        squash = [0, 3, 5, 7][f]
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        body_h = max(2, 10 - squash)
        by = 12 + squash
        draw_rect(img, 8, by, 16, body_h, shade(c['mid'], alpha))
        draw_rect(img, 8, by, 16, max(1, body_h // 3), shade(c['light'], alpha))
        # Horns on ground
        if f < 3:
            draw_rect(img, 6, by - 1, 2, 1, shade(c['light'], alpha))
            draw_rect(img, 24, by - 1, 2, 1, shade(c['light'], alpha))
        if f >= 2:
            draw_pixel(img, 5, 28, c['dark'])
            draw_pixel(img, 26, 27, c['dark'])

    _build_sheet('charger', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 5. PHASER - Dark purple ghost-like, tall thin, phasing effect
# ---------------------------------------------------------------------------

def generate_phaser():
    def _phase_alpha(f):
        """Get alpha for phasing effect."""
        return [200, 140, 90, 140][f % 4]

    def draw_idle(img, c, f):
        bob = [0, -1, -2, -1][f]
        alpha = _phase_alpha(f)
        # Tall thin body
        draw_rect(img, 13, 6 + bob, 6, 18, rgba(c['mid'], alpha))
        draw_rect(img, 13, 6 + bob, 6, 4, rgba(c['light'], alpha))
        draw_rect(img, 14, 6 + bob, 4, 2, rgba(c['highlight'], alpha))
        # Wispy bottom
        draw_rect(img, 12, 22 + bob, 2, 4, rgba(c['dark'], alpha // 2))
        draw_rect(img, 18, 23 + bob, 2, 3, rgba(c['dark'], alpha // 2))
        draw_rect(img, 14, 24 + bob, 4, 4, rgba(c['mid'], alpha // 2))
        # Eyes (always visible)
        draw_eye(img, 14, 12 + bob, 2, c['eye'])
        draw_eye(img, 17, 12 + bob, 2, c['eye'])

    def draw_walk(img, c, f):
        bob = [0, -2, -3, -2, 0, 1][f]
        alpha = _phase_alpha(f)
        drift = [0, 1, 2, 1, 0, -1][f]
        bx = 13 + drift
        draw_rect(img, bx, 6 + bob, 6, 18, rgba(c['mid'], alpha))
        draw_rect(img, bx, 6 + bob, 6, 4, rgba(c['light'], alpha))
        # Wispy trail
        draw_rect(img, bx - 1, 22 + bob, 2, 5, rgba(c['dark'], alpha // 3))
        draw_rect(img, bx + 5, 23 + bob, 2, 4, rgba(c['dark'], alpha // 3))
        draw_rect(img, bx + 1, 24 + bob, 4, 5, rgba(c['mid'], alpha // 3))
        # Eyes
        draw_eye(img, bx + 1, 12 + bob, 2, c['eye'])
        draw_eye(img, bx + 4, 12 + bob, 2, c['eye'])

    def draw_attack(img, c, f):
        # Phase through - body stretches and becomes more transparent
        stretch = [0, 3, 6, 3][f]
        alpha = [200, 100, 60, 120][f]
        body_w = 6 + [0, 2, 4, 2][f]
        bx = 13 - [0, 1, 2, 1][f]
        draw_rect(img, bx, 6, body_w, 18 + stretch, rgba(c['mid'], alpha))
        draw_rect(img, bx, 6, body_w, 4, rgba(c['light'], alpha))
        # Eyes glow intensely
        draw_eye(img, bx + 1, 12, 3, c['eye'])
        draw_eye(img, bx + body_w - 3, 12, 3, c['eye'])
        # Phase particles
        if f >= 1:
            draw_pixel(img, bx - 2, 10, rgba(c['highlight'], alpha // 2))
            draw_pixel(img, bx + body_w + 1, 14, rgba(c['highlight'], alpha // 2))

    def draw_hurt(img, c, f):
        flash_alpha = [255, 120][f]
        flash_col = c['highlight'] if f == 0 else c['mid']
        draw_rect(img, 13, 6, 6, 18, rgba(flash_col, flash_alpha))
        draw_eye(img, 14, 12, 2, c['eye'])
        draw_eye(img, 17, 12, 2, c['eye'])

    def draw_dead(img, c, f):
        # Dissolve into nothing
        alpha = [160, 100, 50, 15][f]
        h = [18, 14, 9, 4][f]
        y = 6 + (18 - h)
        draw_rect(img, 13, y, 6, h, rgba(c['mid'], alpha))
        # Scattered particles
        if f >= 1:
            draw_pixel(img, 11, 10, rgba(c['light'], alpha))
            draw_pixel(img, 20, 8, rgba(c['light'], alpha))
        if f >= 2:
            draw_pixel(img, 9, 15, rgba(c['dark'], alpha // 2))
            draw_pixel(img, 22, 12, rgba(c['dark'], alpha // 2))
            draw_pixel(img, 15, 5, rgba(c['highlight'], alpha // 2))

    _build_sheet('phaser', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 6. EXPLODER - Bright orange ball with glowing core, unstable
# ---------------------------------------------------------------------------

def generate_exploder():
    def draw_idle(img, c, f):
        bob = [0, -1, 0, 1][f]
        pulse = [0, 1, 0, -1][f]
        r = 7 + pulse
        cx, cy = 16, 16 + bob
        # Outer shell
        draw_outline_circle(img, cx, cy, r, c['mid'])
        draw_circle(img, cx, cy, r - 1, c['mid'])
        # Shading
        draw_circle(img, cx - 1, cy - 1, r - 3, c['light'])
        draw_circle(img, cx + 1, cy + 1, r - 2, c['dark'])
        # Glowing core
        draw_circle(img, cx, cy, 3 + (1 if f % 2 == 0 else 0), c['core'])
        draw_pixel(img, cx - 1, cy - 1, c['highlight'])

    def draw_walk(img, c, f):
        # Bouncing movement
        bob = [0, -2, -3, -2, 0, 1][f]
        cx, cy = 16, 16 + bob
        draw_outline_circle(img, cx, cy, 7, c['mid'])
        draw_circle(img, cx, cy, 6, c['mid'])
        draw_circle(img, cx - 1, cy - 1, 4, c['light'])
        draw_circle(img, cx + 1, cy + 1, 5, c['dark'])
        draw_circle(img, cx, cy, 3, c['core'])
        # Unstable particles
        px = [0, 2, -1, 3, -2, 1][f]
        py = [0, -1, 2, -2, 1, 3][f]
        draw_pixel(img, cx + 8 + px, cy + py, c['highlight'])

    def draw_attack(img, c, f):
        # Swelling before explosion
        swell = [0, 2, 4, 3][f]
        r = 7 + swell
        cx, cy = 16, 16
        draw_outline_circle(img, cx, cy, r, c['mid'])
        draw_circle(img, cx, cy, r - 1, c['light'] if f >= 2 else c['mid'])
        # Core grows and glows
        core_r = 3 + swell
        draw_circle(img, cx, cy, core_r, c['core'])
        # Warning glow
        if f >= 2:
            draw_pixel(img, cx - r, cy, c['core'])
            draw_pixel(img, cx + r, cy, c['core'])
            draw_pixel(img, cx, cy - r, c['core'])
            draw_pixel(img, cx, cy + r, c['core'])

    def draw_hurt(img, c, f):
        flash = c['core'] if f == 0 else c['highlight']
        cx, cy = 16, 16
        draw_outline_circle(img, cx, cy, 7, flash)
        draw_circle(img, cx, cy, 6, flash)
        draw_circle(img, cx, cy, 3, c['core'])

    def draw_dead(img, c, f):
        # Explosion sequence
        cx, cy = 16, 16
        if f == 0:
            draw_circle(img, cx, cy, 8, c['core'])
            draw_circle(img, cx, cy, 5, c['highlight'])
        elif f == 1:
            draw_circle(img, cx, cy, 10, c['highlight'])
            draw_circle(img, cx, cy, 6, c['core'])
            # Explosion particles
            for dx, dy in [(-6, -4), (5, -5), (-4, 6), (7, 3)]:
                draw_pixel(img, cx + dx, cy + dy, c['core'])
        elif f == 2:
            # Fading explosion
            draw_circle(img, cx, cy, 8, rgba(c['highlight'], 150))
            draw_circle(img, cx, cy, 4, rgba(c['core'], 100))
            for dx, dy in [(-8, -6), (7, -7), (-6, 8), (9, 5), (-3, -9), (4, 8)]:
                draw_pixel(img, cx + dx, cy + dy, rgba(c['light'], 120))
        else:
            # Smoke
            draw_circle(img, cx, cy, 5, rgba(c['dark'], 60))
            draw_pixel(img, cx - 3, cy - 5, rgba(c['mid'], 40))
            draw_pixel(img, cx + 4, cy - 4, rgba(c['mid'], 40))

    _build_sheet('exploder', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 7. SHIELDER - Blue-cyan armored, carries shield on front, wide
# ---------------------------------------------------------------------------

def generate_shielder():
    def _draw_shield(img, c, x, y, h=12):
        """Draw the front shield."""
        draw_rect(img, x, y, 3, h, c['shield'])
        draw_rect(img, x, y, 1, h, shade(c['shield'], 1.2))
        draw_rect(img, x + 2, y, 1, h, shade(c['shield'], 0.7))
        # Shield rivets
        draw_pixel(img, x + 1, y + 2, c['highlight'])
        draw_pixel(img, x + 1, y + h - 3, c['highlight'])

    def draw_idle(img, c, f):
        bob = [0, 0, -1, 0][f]
        # Wide armored body
        draw_body_with_shading(img, 10, 10 + bob, 14, 12, c)
        # Shield on front
        _draw_shield(img, c, 23, 9 + bob)
        # Small eye slit
        draw_rect(img, 20, 14 + bob, 3, 2, c['dark'])
        draw_eye(img, 21, 14 + bob, 2, rgba(c['shield'], 200))
        # Legs
        draw_rect(img, 12, 22 + bob, 4, 6, c['dark'])
        draw_rect(img, 12, 22 + bob, 4, 2, c['mid'])
        draw_rect(img, 18, 22 + bob, 4, 6, c['dark'])
        draw_rect(img, 18, 22 + bob, 4, 2, c['mid'])
        # Feet
        draw_rect(img, 11, 27 + bob, 5, 2, c['dark'])
        draw_rect(img, 18, 27 + bob, 5, 2, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_off = [0, 1, 2, 1, 0, -1][f]
        draw_body_with_shading(img, 10, 10 + bob, 14, 12, c)
        _draw_shield(img, c, 23, 9 + bob)
        draw_rect(img, 20, 14 + bob, 3, 2, c['dark'])
        draw_eye(img, 21, 14 + bob, 2, rgba(c['shield'], 200))
        # Walking legs
        draw_rect(img, 12, 22 + bob + leg_off, 4, 6, c['dark'])
        draw_rect(img, 12, 22 + bob + leg_off, 4, 2, c['mid'])
        draw_rect(img, 18, 22 + bob - leg_off, 4, 6, c['dark'])
        draw_rect(img, 18, 22 + bob - leg_off, 4, 2, c['mid'])

    def draw_attack(img, c, f):
        # Shield bash
        lean = [0, 2, 4, 2][f]
        draw_body_with_shading(img, 10 + lean, 10, 14, 12, c)
        # Shield extends forward
        shield_x = 23 + lean + [0, 1, 3, 1][f]
        _draw_shield(img, c, shield_x, 8, h=14)
        draw_rect(img, 20 + lean, 14, 3, 2, c['dark'])
        # Legs
        draw_rect(img, 12, 22, 4, 6, c['dark'])
        draw_rect(img, 18, 22, 4, 6, c['dark'])
        # Impact effect
        if f == 2:
            draw_pixel(img, shield_x + 3, 12, c['highlight'])
            draw_pixel(img, shield_x + 3, 16, c['highlight'])
            draw_pixel(img, shield_x + 4, 14, c['highlight'])

    def draw_hurt(img, c, f):
        recoil = [2, 4][f]
        flash = c['highlight'] if f == 0 else c['mid']
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, 10 - recoil, 10, 14, 12, cols)
        # Shield cracks
        _draw_shield(img, c, 23 - recoil, 9)
        draw_rect(img, 12, 22, 4, 6, c['dark'])
        draw_rect(img, 18, 22, 4, 6, c['dark'])

    def draw_dead(img, c, f):
        squash = [0, 3, 5, 7][f]
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        body_h = max(2, 12 - squash)
        by = 10 + squash
        draw_rect(img, 10, by, 14, body_h, shade(c['mid'], alpha))
        draw_rect(img, 10, by, 14, max(1, body_h // 3), shade(c['light'], alpha))
        # Shield falls
        if f < 3:
            shield_y = 9 + squash * 2
            draw_rect(img, 24 - f, min(shield_y, 26), 3, max(2, 8 - f * 2),
                      shade(c['shield'], alpha))
        if f >= 2:
            draw_pixel(img, 8, 28, c['dark'])
            draw_pixel(img, 26, 27, shade(c['shield'], 0.5))

    _build_sheet('shielder', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 8. CRAWLER - Green bug/spider, flat body, many legs
# ---------------------------------------------------------------------------

def generate_crawler():
    def _draw_legs_multi(img, c, bx, by, bw, f, offsets=None):
        """Draw 3 pairs of spider-like legs."""
        if offsets is None:
            offsets = [0, 0, 0]
        leg_positions = [
            (bx - 2, by + 2),   # front pair
            (bx + 1, by + 4),   # mid pair
            (bx + 4, by + 4),   # back pair
        ]
        for i, (lx, ly) in enumerate(leg_positions):
            off = offsets[i % len(offsets)]
            # Left leg
            draw_pixel(img, lx - 1, ly + off, c['dark'])
            draw_pixel(img, lx - 2, ly + 1 + off, c['mid'])
            draw_pixel(img, lx - 3, ly + 2, c['dark'])
            # Right leg
            rx = bx + bw + (bx - lx)
            draw_pixel(img, rx + 1, ly - off, c['dark'])
            draw_pixel(img, rx + 2, ly + 1 - off, c['mid'])
            draw_pixel(img, rx + 3, ly + 2, c['dark'])

    def draw_idle(img, c, f):
        bob = [0, 0, -1, 0][f]
        bx, by = 10, 18 + bob
        # Flat body
        draw_body_with_shading(img, bx, by, 12, 6, c)
        # Head
        draw_body_with_shading(img, bx + 8, by - 2, 6, 5, c)
        # Eyes
        draw_eye(img, bx + 11, by - 1, 2, c['eye'])
        draw_eye(img, bx + 13, by - 1, 1, c['eye'])
        # Mandibles
        draw_pixel(img, bx + 14, by + 2, c['dark'])
        draw_pixel(img, bx + 14, by + 3, c['dark'])
        # Legs
        leg_off = [[0, 1, 0], [1, 0, 1], [0, 1, 0], [-1, 0, -1]][f]
        _draw_legs_multi(img, c, bx, by, 12, f, leg_off)

    def draw_walk(img, c, f):
        bob = [0, 0, -1, 0, 0, 1][f]
        bx, by = 10, 18 + bob
        draw_body_with_shading(img, bx, by, 12, 6, c)
        draw_body_with_shading(img, bx + 8, by - 2, 6, 5, c)
        draw_eye(img, bx + 11, by - 1, 2, c['eye'])
        draw_eye(img, bx + 13, by - 1, 1, c['eye'])
        draw_pixel(img, bx + 14, by + 2, c['dark'])
        # Walking leg animation - alternating tripod gait
        leg_off = [
            [0, -1, 1], [1, 0, -1], [-1, 1, 0],
            [0, -1, 1], [1, 0, -1], [-1, 1, 0]
        ][f]
        _draw_legs_multi(img, c, bx, by, 12, f, leg_off)

    def draw_attack(img, c, f):
        # Lunge forward with mandibles
        lunge = [0, 2, 4, 2][f]
        bx, by = 10, 18
        draw_body_with_shading(img, bx, by, 12, 6, c)
        draw_body_with_shading(img, bx + 8 + lunge, by - 2, 6, 5, c)
        draw_eye(img, bx + 11 + lunge, by - 1, 2, c['eye'])
        # Mandibles open
        mand_spread = [0, 1, 2, 1][f]
        draw_pixel(img, bx + 14 + lunge, by + 1 - mand_spread, c['dark'])
        draw_pixel(img, bx + 15 + lunge, by + 1 - mand_spread, c['dark'])
        draw_pixel(img, bx + 14 + lunge, by + 3 + mand_spread, c['dark'])
        draw_pixel(img, bx + 15 + lunge, by + 3 + mand_spread, c['dark'])
        _draw_legs_multi(img, c, bx, by, 12, f)

    def draw_hurt(img, c, f):
        recoil = [1, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        bx = 10 - recoil
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, bx, 18, 12, 6, cols)
        draw_body_with_shading(img, bx + 8, 16, 6, 5, cols)
        draw_eye(img, bx + 11, 17, 2, c['eye'])
        _draw_legs_multi(img, c, bx, 18, 12, f)

    def draw_dead(img, c, f):
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        # Flips over
        by = 18 + [0, 1, 2, 2][f]
        bx = 10
        draw_rect(img, bx, by, 12, max(2, 6 - f), shade(c['mid'], alpha))
        if f < 3:
            draw_rect(img, bx + 8, by - 1, 6, max(2, 5 - f), shade(c['mid'], alpha))
        # Legs curl up
        if f < 2:
            for i in range(3):
                lx = bx - 2 + i * 3
                draw_pixel(img, lx, by - 1, shade(c['dark'], alpha))
                draw_pixel(img, lx + 12, by - 1, shade(c['dark'], alpha))
        if f >= 2:
            draw_pixel(img, 8, 26, c['dark'])
            draw_pixel(img, 24, 25, c['dark'])

    _build_sheet('crawler', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 9. SUMMONER - Magenta robed figure, floating orb, tall
# ---------------------------------------------------------------------------

def generate_summoner():
    def _draw_orb(img, c, x, y, pulse=0):
        """Draw the floating orb."""
        r = 2 + pulse
        draw_outline_circle(img, x, y, r, c['orb'])
        draw_pixel(img, x - 1, y - 1, c['highlight'])

    def draw_idle(img, c, f):
        bob = [0, -1, -1, 0][f]
        orb_bob = [-1, -2, -1, 0][f]
        orb_pulse = [0, 0, 1, 0][f]
        # Hooded head
        draw_body_with_shading(img, 12, 6 + bob, 8, 7, c)
        # Robe body (wider at bottom)
        draw_body_with_shading(img, 11, 13 + bob, 10, 10, c)
        draw_rect(img, 10, 20 + bob, 12, 5, c['dark'])
        draw_rect(img, 10, 20 + bob, 12, 1, c['mid'])
        # Robe bottom edge
        draw_rect(img, 9, 24 + bob, 14, 3, c['dark'])
        # Eyes under hood
        draw_eye(img, 14, 9 + bob, 2, c['orb'])
        draw_eye(img, 17, 9 + bob, 2, c['orb'])
        # Floating orb
        _draw_orb(img, c, 24, 8 + orb_bob, orb_pulse)
        # Raised arm
        draw_rect(img, 20, 14 + bob, 3, 2, c['mid'])
        draw_rect(img, 22, 12 + bob, 2, 3, c['mid'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        orb_bob = [-1, -2, -3, -2, -1, 0][f]
        draw_body_with_shading(img, 12, 6 + bob, 8, 7, c)
        draw_body_with_shading(img, 11, 13 + bob, 10, 10, c)
        draw_rect(img, 10, 20 + bob, 12, 5, c['dark'])
        draw_rect(img, 9, 24 + bob, 14, 3, c['dark'])
        draw_eye(img, 14, 9 + bob, 2, c['orb'])
        draw_eye(img, 17, 9 + bob, 2, c['orb'])
        _draw_orb(img, c, 24, 8 + orb_bob)
        draw_rect(img, 20, 14 + bob, 3, 2, c['mid'])
        # Robe sway
        sway = [0, 1, 0, -1, 0, 1][f]
        draw_rect(img, 9 + sway, 25 + bob, 14, 2, c['dark'])

    def draw_attack(img, c, f):
        # Arms raised, orb glows and fires
        arm_raise = [0, -2, -4, -2][f]
        orb_pulse = [0, 1, 2, 1][f]
        draw_body_with_shading(img, 12, 6, 8, 7, c)
        draw_body_with_shading(img, 11, 13, 10, 10, c)
        draw_rect(img, 10, 20, 12, 5, c['dark'])
        draw_rect(img, 9, 24, 14, 3, c['dark'])
        draw_eye(img, 14, 9, 2, c['orb'])
        draw_eye(img, 17, 9, 2, c['orb'])
        # Both arms raised
        draw_rect(img, 8, 12 + arm_raise, 3, 4, c['mid'])
        draw_rect(img, 21, 12 + arm_raise, 3, 4, c['mid'])
        # Orb centered above, glowing
        _draw_orb(img, c, 16, 3 + arm_raise, orb_pulse)
        # Energy particles
        if f >= 2:
            draw_pixel(img, 12, 2, c['orb'])
            draw_pixel(img, 20, 2, c['orb'])
            draw_pixel(img, 16, 0, c['highlight'])

    def draw_hurt(img, c, f):
        recoil = [1, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, 12 - recoil, 6, 8, 7, cols)
        draw_body_with_shading(img, 11 - recoil, 13, 10, 10, cols)
        draw_rect(img, 10 - recoil, 20, 12, 7, c['dark'])
        # Orb destabilized
        _draw_orb(img, c, 24 + recoil, 10, 0)

    def draw_dead(img, c, f):
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        collapse = [0, 3, 6, 8][f]
        # Robe collapses
        by = 6 + collapse
        h = max(2, 20 - collapse * 2)
        draw_rect(img, 11, by, 10, h, shade(c['mid'], alpha))
        draw_rect(img, 10, by + h, 12, max(1, 4 - f), shade(c['dark'], alpha))
        # Orb fades
        if f < 3:
            draw_circle(img, 16, by - 2, max(1, 3 - f), rgba(c['orb'], int(200 * alpha)))
        # Particles
        if f >= 2:
            draw_pixel(img, 8, 20, rgba(c['orb'], int(100 * alpha)))
            draw_pixel(img, 24, 18, rgba(c['orb'], int(80 * alpha)))

    _build_sheet('summoner', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 10. SNIPER - Gold mechanical, long scope/rifle, thin
# ---------------------------------------------------------------------------

def generate_sniper():
    def _draw_rifle(img, c, bx, by, barrel_ext=0):
        """Draw the sniper rifle."""
        # Rifle body
        draw_rect(img, bx + 10, by + 4, 8 + barrel_ext, 2, c['dark'])
        draw_rect(img, bx + 10, by + 4, 8 + barrel_ext, 1, c['mid'])
        # Scope
        draw_rect(img, bx + 14, by + 2, 3, 2, c['scope'])
        draw_pixel(img, bx + 15, by + 2, c['highlight'])
        # Barrel tip
        draw_pixel(img, bx + 17 + barrel_ext, by + 4, c['light'])

    def draw_idle(img, c, f):
        bob = [0, 0, -1, 0][f]
        bx, by = 6, 8 + bob
        # Thin mechanical body
        draw_body_with_shading(img, bx + 2, by, 6, 5, c)  # head
        draw_body_with_shading(img, bx + 3, by + 5, 4, 10, c)  # torso
        # Scope eye
        draw_eye(img, bx + 6, by + 2, 2, c['scope'])
        # Rifle
        _draw_rifle(img, c, bx, by)
        # Thin legs
        draw_rect(img, bx + 3, by + 15, 2, 7, c['dark'])
        draw_rect(img, bx + 3, by + 15, 2, 2, c['mid'])
        draw_rect(img, bx + 6, by + 15, 2, 7, c['dark'])
        draw_rect(img, bx + 6, by + 15, 2, 2, c['mid'])
        # Feet
        draw_rect(img, bx + 2, by + 21, 3, 2, c['dark'])
        draw_rect(img, bx + 6, by + 21, 3, 2, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_off = [0, 2, 3, 2, 0, -1][f]
        bx, by = 6, 8 + bob
        draw_body_with_shading(img, bx + 2, by, 6, 5, c)
        draw_body_with_shading(img, bx + 3, by + 5, 4, 10, c)
        draw_eye(img, bx + 6, by + 2, 2, c['scope'])
        _draw_rifle(img, c, bx, by)
        # Walking legs
        draw_rect(img, bx + 3, by + 15 + leg_off, 2, 7, c['dark'])
        draw_rect(img, bx + 3, by + 15 + leg_off, 2, 2, c['mid'])
        draw_rect(img, bx + 6, by + 15 - leg_off, 2, 7, c['dark'])
        draw_rect(img, bx + 6, by + 15 - leg_off, 2, 2, c['mid'])

    def draw_attack(img, c, f):
        bx, by = 6, 8
        # Aiming stance - crouch slightly
        draw_body_with_shading(img, bx + 2, by + 1, 6, 5, c)
        draw_body_with_shading(img, bx + 3, by + 6, 4, 9, c)
        draw_eye(img, bx + 6, by + 3, 2, c['scope'])
        # Rifle extends for shot
        barrel_ext = [0, 2, 2, 0][f]
        _draw_rifle(img, c, bx, by + 1, barrel_ext)
        # Muzzle flash
        if f == 2:
            fx = bx + 19
            draw_pixel(img, fx, by + 3, c['highlight'])
            draw_pixel(img, fx + 1, by + 4, c['scope'])
            draw_pixel(img, fx, by + 5, c['highlight'])
            draw_pixel(img, fx + 2, by + 4, c['highlight'])
        # Scope glint
        if f == 1:
            draw_pixel(img, bx + 15, by + 3, (255, 255, 255))
        # Legs in crouch
        draw_rect(img, bx + 2, by + 15, 3, 7, c['dark'])
        draw_rect(img, bx + 6, by + 15, 3, 7, c['dark'])

    def draw_hurt(img, c, f):
        recoil = [2, 4][f]
        flash = c['highlight'] if f == 0 else c['mid']
        bx = 6 - recoil
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, bx + 2, 8, 6, 5, cols)
        draw_body_with_shading(img, bx + 3, 13, 4, 10, cols)
        draw_rect(img, bx + 3, 23, 2, 7, c['dark'])
        draw_rect(img, bx + 6, 23, 2, 7, c['dark'])

    def draw_dead(img, c, f):
        alpha = [1.0, 0.7, 0.4, 0.2][f]
        collapse = [0, 3, 6, 8][f]
        bx = 6
        # Body collapses
        by = 8 + collapse
        h = max(2, 15 - collapse)
        draw_rect(img, bx + 2, by, 6, h, shade(c['mid'], alpha))
        # Rifle falls
        if f < 3:
            ry = by + 2 + collapse
            draw_rect(img, bx + 8, min(ry, 28), max(2, 10 - f * 3), 2,
                      shade(c['dark'], alpha))
        if f >= 2:
            draw_pixel(img, 4, 28, c['dark'])
            draw_pixel(img, 20, 27, shade(c['scope'], 0.5))

    _build_sheet('sniper', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 11. MINION - Light purple, small version of walker (smaller character)
# ---------------------------------------------------------------------------

def generate_minion():
    def draw_idle(img, c, f):
        bob = [0, -1, 0, -1][f]
        # Small boxy body (centered smaller in 32x32)
        bx, by = 12, 16 + bob
        draw_body_with_shading(img, bx, by, 8, 8, c)
        # Single eye
        draw_eye(img, bx + 3, by + 2, 2, c['eye'])
        # Small legs
        draw_rect(img, bx + 1, by + 8, 2, 4, c['dark'])
        draw_rect(img, bx + 1, by + 8, 2, 1, c['mid'])
        draw_rect(img, bx + 5, by + 8, 2, 4, c['dark'])
        draw_rect(img, bx + 5, by + 8, 2, 1, c['mid'])
        # Tiny feet
        draw_rect(img, bx, by + 11, 3, 1, c['dark'])
        draw_rect(img, bx + 5, by + 11, 3, 1, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_off = [0, 1, 2, 1, 0, -1][f]
        bx, by = 12, 16 + bob
        draw_body_with_shading(img, bx, by, 8, 8, c)
        draw_eye(img, bx + 3, by + 2, 2, c['eye'])
        # Walking legs
        draw_rect(img, bx + 1, by + 8 + leg_off, 2, 4, c['dark'])
        draw_rect(img, bx + 1, by + 8 + leg_off, 2, 1, c['mid'])
        draw_rect(img, bx + 5, by + 8 - leg_off, 2, 4, c['dark'])
        draw_rect(img, bx + 5, by + 8 - leg_off, 2, 1, c['mid'])

    def draw_attack(img, c, f):
        lean = [0, 1, 2, 1][f]
        bx, by = 12 + lean, 16
        draw_body_with_shading(img, bx, by, 8, 8, c)
        eye_size = [2, 2, 3, 2][f]
        draw_eye(img, bx + 3, by + 2, eye_size, c['eye'])
        # Legs
        draw_rect(img, 13, 24, 2, 4, c['dark'])
        draw_rect(img, 17, 24, 2, 4, c['dark'])
        # Small punch
        px = bx + 8 + [0, 1, 3, 1][f]
        draw_rect(img, px, by + 3, 2, 2, c['light'])

    def draw_hurt(img, c, f):
        recoil = [1, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        bx = 12 - recoil
        cols = {'dark': c['dark'], 'mid': flash, 'light': c['highlight'], 'highlight': c['highlight']}
        draw_body_with_shading(img, bx, 16, 8, 8, cols)
        draw_eye(img, bx + 3, 18, 1, c['eye'])
        draw_rect(img, bx + 1, 24, 2, 4, c['dark'])
        draw_rect(img, bx + 5, 24, 2, 4, c['dark'])

    def draw_dead(img, c, f):
        squash = [0, 2, 3, 4][f]
        alpha = [1.0, 0.8, 0.5, 0.3][f]
        bx = 12
        body_h = max(1, 8 - squash)
        by = 16 + squash
        col = shade(c['mid'], alpha)
        draw_rect(img, bx, by, 8, body_h, col)
        draw_rect(img, bx, by, 8, max(1, body_h // 3), shade(c['light'], alpha))
        if f < 2:
            draw_eye(img, bx + 3, by + max(0, body_h // 2 - 1), 1, c['eye'])
        # Debris
        if f >= 2:
            draw_pixel(img, 10, 26, c['dark'])
            draw_pixel(img, 22, 25, c['dark'])

    _build_sheet('minion', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 12. TELEPORTER - Blue glowing entity, blinks in/out, portal rings
# ---------------------------------------------------------------------------

def generate_teleporter():
    def draw_idle(img, c, f):
        bob = [0, -1, -1, 0][f]
        # Warp ring around body
        ring_alpha = [200, 160, 200, 240][f]
        draw_outline_circle(img, 15, 16 + bob, 10, c['warp'], c['dark'])
        # Body (slim, ethereal)
        draw_body_with_shading(img, 11, 10 + bob, 10, 14, c)
        # Eye (glowing cyan)
        draw_eye(img, 14, 14 + bob, 3, c['eye'])
        # Portal sparks
        sx = [8, 22, 6, 24][f]
        sy = [12, 18, 20, 10][f]
        draw_pixel(img, sx, sy + bob, c['warp'])
        draw_pixel(img, sx + 1, sy + 1 + bob, c['highlight'])

    def draw_walk(img, c, f):
        bob = [0, -2, -1, 1, 0, -2][f]
        # Warp trail
        trail_x = [0, -1, -2, -1, 0, 1][f]
        draw_circle(img, 15 + trail_x, 20 + bob, 3, c['warp'])
        # Body
        draw_body_with_shading(img, 11 + trail_x, 10 + bob, 10, 14, c)
        # Eye
        draw_eye(img, 14 + trail_x, 14 + bob, 3, c['eye'])
        # Trailing afterimage
        if abs(trail_x) > 0:
            draw_rect(img, 11 - trail_x, 12 + bob, 10, 10, (*c['mid'][:3], 80))

    def draw_attack(img, c, f):
        # Teleport attack: fade out, reappear
        if f == 0:
            draw_body_with_shading(img, 11, 10, 10, 14, c)
            draw_eye(img, 14, 14, 3, c['eye'])
        elif f == 1:
            # Fading - smaller, translucent
            draw_rect(img, 13, 12, 6, 8, c['warp'])
            draw_circle(img, 15, 16, 8, c['highlight'])
        elif f == 2:
            # Gone - just portal ring
            draw_outline_circle(img, 15, 16, 10, c['warp'], c['dark'])
            draw_outline_circle(img, 15, 16, 6, c['highlight'], c['mid'])
        else:
            # Reappear with burst
            draw_circle(img, 15, 16, 9, c['warp'])
            draw_body_with_shading(img, 11, 10, 10, 14, c)
            draw_eye(img, 14, 14, 4, c['eye'])

    def draw_hurt(img, c, f):
        recoil = [2, 4][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(img, 11 - recoil, 10, 10, 14,
                               {'dark': c['dark'], 'mid': flash,
                                'light': c['highlight'], 'highlight': c['highlight']})
        draw_eye(img, 14 - recoil, 14, 2, c['eye'])
        # Disrupted warp sparks
        draw_pixel(img, 8 - recoil, 12, c['warp'])
        draw_pixel(img, 24 - recoil, 18, c['warp'])

    def draw_dead(img, c, f):
        squash = [0, 3, 6, 8][f]
        body_h = max(2, 14 - squash)
        alpha_f = [1.0, 0.7, 0.4, 0.2][f]
        col = shade(c['mid'], alpha_f)
        draw_rect(img, 11, 10 + squash, 10, body_h, col)
        # Portal collapse
        r = max(1, 8 - f * 2)
        draw_outline_circle(img, 15, 16, r, shade(c['warp'], alpha_f), shade(c['dark'], alpha_f))
        if f >= 2:
            draw_pixel(img, 8, 26, c['dark'])
            draw_pixel(img, 22, 25, c['dark'])

    _build_sheet('teleporter', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 13. REFLECTOR - Silver/chrome armored, mirror shield, deflects projectiles
# ---------------------------------------------------------------------------

def generate_reflector():
    def _draw_shield(img, x, y, h, c, f):
        """Draw a mirror shield with shimmer."""
        shimmer = [0, 1, 2, 1, 0, -1][f % 6]
        draw_rect(img, x, y, 3, h, c['mirror'])
        draw_rect(img, x, y + shimmer + 2, 3, 2, c['highlight'])
        draw_rect(img, x, y, 1, h, c['light'])

    def draw_idle(img, c, f):
        bob = [0, 0, -1, -1][f]
        # Body (stocky)
        draw_body_with_shading(img, 10, 10 + bob, 10, 14, c)
        # Shield (left side)
        _draw_shield(img, 6, 10 + bob, 14, c, f)
        # Eye
        draw_eye(img, 15, 14 + bob, 3, c['eye'])
        # Legs
        draw_rect(img, 12, 24 + bob, 3, 5, c['dark'])
        draw_rect(img, 17, 24 + bob, 3, 5, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -1, 0, 1, 0, -1][f]
        leg_off = [0, 2, 3, 2, 0, -1][f]
        # Body
        draw_body_with_shading(img, 10, 10 + bob, 10, 14, c)
        # Shield bobs with walk
        _draw_shield(img, 6, 10 + bob, 14, c, f)
        # Eye
        draw_eye(img, 15, 14 + bob, 3, c['eye'])
        # Legs
        draw_rect(img, 12, 24 + bob + leg_off, 3, 5, c['dark'])
        draw_rect(img, 17, 24 + bob - leg_off, 3, 5, c['dark'])

    def draw_attack(img, c, f):
        # Shield bash forward
        shield_x = [6, 4, 2, 5][f]
        lean = [0, 1, 3, 1][f]
        draw_body_with_shading(img, 10 + lean, 10, 10, 14, c)
        _draw_shield(img, shield_x + lean, 10, 14, c, f)
        draw_eye(img, 15 + lean, 14, 3, c['eye'])
        # Impact flash on frame 2
        if f == 2:
            draw_circle(img, 4, 16, 3, c['highlight'])
        draw_rect(img, 12, 24, 3, 5, c['dark'])
        draw_rect(img, 17, 24, 3, 5, c['dark'])

    def draw_hurt(img, c, f):
        recoil = [2, 4][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(img, 10 + recoil, 10, 10, 14,
                               {'dark': c['dark'], 'mid': flash,
                                'light': c['highlight'], 'highlight': c['highlight']})
        # Shield knocked aside
        _draw_shield(img, 6 + recoil + 2, 12, 10, c, f)
        draw_eye(img, 15 + recoil, 14, 2, c['eye'])
        draw_rect(img, 12, 24, 3, 5, c['dark'])
        draw_rect(img, 17, 24, 3, 5, c['dark'])

    def draw_dead(img, c, f):
        squash = [0, 2, 5, 7][f]
        body_h = max(2, 14 - squash)
        alpha_f = [1.0, 0.7, 0.4, 0.2][f]
        col = shade(c['mid'], alpha_f)
        draw_rect(img, 10, 10 + squash, 10, body_h, col)
        # Shield falls apart
        if f < 3:
            draw_rect(img, 6, 12 + squash, 3, max(2, 10 - squash * 2), shade(c['mirror'], alpha_f))
        if f >= 2:
            draw_pixel(img, 5, 26, shade(c['mirror'], 0.5))
            draw_pixel(img, 22, 24, c['dark'])

    _build_sheet('reflector', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 14. LEECH - Dark red worm-like, attaches to drain health, small and fast
# ---------------------------------------------------------------------------

def generate_leech():
    def _draw_body_segments(img, x, y, c, wave=0):
        """Draw segmented worm body."""
        for i in range(4):
            seg_y = y + i * 3 + [0, 1, 0, -1][((i + wave) % 4)]
            w = [6, 8, 7, 5][i]
            sx = x + (8 - w) // 2
            draw_rect(img, sx, seg_y, w, 3, c['mid'] if i % 2 == 0 else c['light'])
            draw_rect(img, sx, seg_y, w, 1, c['light'] if i % 2 == 0 else c['highlight'])

    def draw_idle(img, c, f):
        wave = [0, 1, 2, 1][f]
        _draw_body_segments(img, 12, 8, c, wave)
        # Head with mouth
        draw_outline_circle(img, 15, 8, 4, c['mid'], c['dark'])
        draw_eye(img, 14, 7, 2, c['eye'])
        draw_rect(img, 13, 11, 4, 2, c['mouth'])
        # Tail
        draw_rect(img, 14, 22 + wave, 3, 3, c['dark'])

    def draw_walk(img, c, f):
        wave = f
        dx = [0, 1, 0, -1, 0, 1][f]
        _draw_body_segments(img, 12 + dx, 8, c, wave)
        # Head
        draw_outline_circle(img, 15 + dx, 8, 4, c['mid'], c['dark'])
        draw_eye(img, 14 + dx, 7, 2, c['eye'])
        draw_rect(img, 13 + dx, 11, 4, 2, c['mouth'])
        draw_rect(img, 14 + dx, 22 + (wave % 2), 3, 3, c['dark'])

    def draw_attack(img, c, f):
        # Lunge forward to latch on
        lunge = [0, 3, 6, 4][f]
        _draw_body_segments(img, 12, 8, c, f)
        # Head lunges
        draw_outline_circle(img, 15, 8 - lunge, 4, c['mid'], c['dark'])
        draw_eye(img, 14, 7 - lunge, 2, c['eye'])
        # Mouth opens wider during attack
        mouth_h = [2, 3, 4, 3][f]
        draw_rect(img, 12, 11 - lunge, 6, mouth_h, c['mouth'])
        # Drain effect on hit frame
        if f == 2:
            draw_pixel(img, 15, 2, (255, 60, 60))
            draw_pixel(img, 14, 1, (255, 60, 60))

    def draw_hurt(img, c, f):
        recoil = [0, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        _draw_body_segments(img, 12, 8 + recoil, {'dark': c['dark'], 'mid': flash,
                            'light': c['highlight'], 'highlight': c['highlight']}, 0)
        draw_outline_circle(img, 15, 8 + recoil, 4, flash, c['dark'])
        draw_eye(img, 14, 7 + recoil, 2, c['eye'])

    def draw_dead(img, c, f):
        squash = [0, 1, 3, 5][f]
        alpha_f = [1.0, 0.7, 0.4, 0.2][f]
        col = shade(c['mid'], alpha_f)
        # Body collapses into a pile
        draw_rect(img, 12, 14 + squash, 8, max(2, 10 - squash * 2), col)
        if f < 2:
            draw_eye(img, 14, 14 + squash, 2, c['eye'])
        if f >= 2:
            draw_pixel(img, 10, 24, shade(c['dark'], 0.5))
            draw_pixel(img, 20, 23, shade(c['dark'], 0.5))

    _build_sheet('leech', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 15. SWARMER - Small yellow insect, appears in groups, tiny wings
# ---------------------------------------------------------------------------

def generate_swarmer():
    def _draw_wings(img, x, y, c, spread):
        """Draw tiny insect wings."""
        # Left wing
        draw_rect(img, x - spread - 2, y - 1, 3, 2, c['wing'])
        draw_pixel(img, x - spread - 2, y - 1, c['highlight'])
        # Right wing
        draw_rect(img, x + spread + 3, y - 1, 3, 2, c['wing'])
        draw_pixel(img, x + spread + 4, y - 1, c['highlight'])

    def draw_idle(img, c, f):
        bob = [0, -1, -2, -1][f]
        wing_spread = [1, 2, 1, 0][f]
        # Small body
        draw_body_with_shading(img, 13, 14 + bob, 6, 6, c)
        # Wings
        _draw_wings(img, 13, 14 + bob, c, wing_spread)
        # Eye (single dot)
        draw_eye(img, 15, 15 + bob, 2, c['eye'])
        # Stinger
        draw_pixel(img, 15, 20 + bob, c['dark'])
        draw_pixel(img, 15, 21 + bob, c['dark'])

    def draw_walk(img, c, f):
        bob = [0, -2, -1, 1, 0, -2][f]
        dx = [0, 1, 2, 1, 0, -1][f]
        wing_spread = [1, 3, 2, 0, 1, 3][f]
        # Body
        draw_body_with_shading(img, 13 + dx, 14 + bob, 6, 6, c)
        _draw_wings(img, 13 + dx, 14 + bob, c, wing_spread)
        draw_eye(img, 15 + dx, 15 + bob, 2, c['eye'])
        draw_pixel(img, 15 + dx, 20 + bob, c['dark'])

    def draw_attack(img, c, f):
        # Dive bomb with stinger
        dive = [0, 2, 5, 3][f]
        draw_body_with_shading(img, 13, 14 - dive, 6, 6, c)
        _draw_wings(img, 13, 14 - dive, c, 3)
        draw_eye(img, 15, 15 - dive, 2, c['eye'])
        # Extended stinger
        for i in range(3 + f):
            draw_pixel(img, 15, 20 - dive + i, c['dark'])
        # Impact sparks
        if f == 2:
            draw_pixel(img, 14, 24, c['highlight'])
            draw_pixel(img, 16, 25, c['highlight'])

    def draw_hurt(img, c, f):
        recoil = [1, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(img, 13, 14 + recoil, 6, 6,
                               {'dark': c['dark'], 'mid': flash,
                                'light': c['highlight'], 'highlight': c['highlight']})
        _draw_wings(img, 13, 14 + recoil, c, 0)
        draw_eye(img, 15, 15 + recoil, 1, c['eye'])

    def draw_dead(img, c, f):
        fall = [0, 3, 6, 8][f]
        alpha_f = [1.0, 0.7, 0.4, 0.2][f]
        col = shade(c['mid'], alpha_f)
        draw_rect(img, 13, 14 + fall, 6, max(2, 6 - f), col)
        # Wings crumple
        if f < 3:
            draw_rect(img, 10, 14 + fall, 3, 1, shade(c['wing'], alpha_f))
            draw_rect(img, 19, 14 + fall, 3, 1, shade(c['wing'], alpha_f))

    _build_sheet('swarmer', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 16. GRAVITYWELL - Dark purple orb, pulls entities toward it, pulsating
# ---------------------------------------------------------------------------

def generate_gravitywell():
    def draw_idle(img, c, f):
        pulse = [0, 1, 2, 1][f]
        r = 8 + pulse
        # Gravity field (outer glow)
        draw_circle(img, 15, 15, r + 3, c['dark'])
        # Core body
        draw_outline_circle(img, 15, 15, r, c['mid'], c['dark'])
        # Inner glow
        draw_circle(img, 15, 15, max(2, r - 3), c['light'])
        # Core eye
        draw_circle(img, 15, 15, 2, c['core'])
        draw_pixel(img, 15, 15, c['highlight'])
        # Orbiting particles
        import math
        angle = f * 1.57  # 90 degree increments
        ox = int(15 + (r + 2) * math.cos(angle))
        oy = int(15 + (r + 2) * math.sin(angle))
        draw_pixel(img, ox, oy, c['highlight'])

    def draw_walk(img, c, f):
        # GravityWell drifts slowly
        dx = [0, 0, 1, 1, 0, -1][f]
        dy = [0, -1, 0, 1, 0, 0][f]
        pulse = [0, 1, 1, 0, 1, 2][f]
        r = 8 + pulse
        draw_circle(img, 15 + dx, 15 + dy, r + 3, c['dark'])
        draw_outline_circle(img, 15 + dx, 15 + dy, r, c['mid'], c['dark'])
        draw_circle(img, 15 + dx, 15 + dy, max(2, r - 3), c['light'])
        draw_circle(img, 15 + dx, 15 + dy, 2, c['core'])
        draw_pixel(img, 15 + dx, 15 + dy, c['highlight'])

    def draw_attack(img, c, f):
        # Gravity pull - expands field dramatically
        r = [8, 10, 13, 11][f]
        draw_circle(img, 15, 15, r + 4, c['dark'])
        draw_outline_circle(img, 15, 15, r + 2, c['core'], c['dark'])
        draw_outline_circle(img, 15, 15, r, c['light'], c['mid'])
        draw_circle(img, 15, 15, max(2, r - 4), c['highlight'])
        draw_circle(img, 15, 15, 2, c['core'])
        # Pull lines
        if f >= 1:
            for angle_i in range(4):
                import math
                a = angle_i * 1.57 + f * 0.3
                ex = int(15 + (r + 3) * math.cos(a))
                ey = int(15 + (r + 3) * math.sin(a))
                draw_pixel(img, ex, ey, c['highlight'])

    def draw_hurt(img, c, f):
        flash = c['highlight'] if f == 0 else c['mid']
        r = [7, 5][f]
        draw_circle(img, 15, 15, r + 3, c['dark'])
        draw_outline_circle(img, 15, 15, r, flash, c['dark'])
        draw_circle(img, 15, 15, 2, c['core'])

    def draw_dead(img, c, f):
        r = [8, 6, 3, 1][f]
        alpha_f = [1.0, 0.7, 0.4, 0.15][f]
        col = shade(c['mid'], alpha_f)
        draw_circle(img, 15, 15, r + 2, shade(c['dark'], alpha_f))
        draw_circle(img, 15, 15, r, col)
        if r > 2:
            draw_circle(img, 15, 15, max(1, r - 2), shade(c['light'], alpha_f))
        # Gravity sparks disperse
        if f >= 2:
            draw_pixel(img, 8, 8, shade(c['core'], 0.3))
            draw_pixel(img, 22, 22, shade(c['core'], 0.3))
            draw_pixel(img, 22, 8, shade(c['core'], 0.3))

    _build_sheet('gravitywell', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# 17. MIMIC - Looks like a treasure chest, springs open to attack
# ---------------------------------------------------------------------------

def generate_mimic():
    def _draw_chest_closed(img, x, y, c):
        """Draw closed treasure chest."""
        # Chest body
        draw_body_with_shading(img, x, y, 14, 10, c)
        # Lid (slightly darker)
        draw_rect(img, x, y, 14, 4, c['dark'])
        draw_rect(img, x + 1, y + 1, 12, 2, c['mid'])
        # Lock
        draw_rect(img, x + 5, y + 3, 4, 3, c['highlight'])
        draw_pixel(img, x + 6, y + 4, c['dark'])

    def _draw_chest_open(img, x, y, c, mouth_h):
        """Draw open chest with teeth."""
        # Lower jaw
        draw_body_with_shading(img, x, y + mouth_h, 14, 10 - mouth_h // 2, c)
        # Upper jaw (lid open)
        draw_rect(img, x, y, 14, 3, c['dark'])
        draw_rect(img, x + 1, y + 1, 12, 1, c['mid'])
        # Teeth top
        for i in range(5):
            tx = x + 1 + i * 3
            draw_rect(img, tx, y + 3, 2, mouth_h, c['teeth'])
        # Teeth bottom
        for i in range(5):
            tx = x + 1 + i * 3
            draw_rect(img, tx, y + mouth_h - 1, 2, 2, c['teeth'])
        # Tongue
        draw_rect(img, x + 5, y + mouth_h + 2, 4, 2, (200, 50, 80))

    def draw_idle(img, c, f):
        bob = [0, 0, 0, -1][f]
        _draw_chest_closed(img, 9, 14 + bob, c)
        # Hidden eye peeks on frame 3
        if f == 3:
            draw_pixel(img, 14, 17, c['eye'])

    def draw_walk(img, c, f):
        # Chest hops along
        hop = [0, -2, -3, -2, 0, 1][f]
        dx = [0, 1, 1, 0, -1, 0][f]
        _draw_chest_closed(img, 9 + dx, 14 + hop, c)
        # Little feet peeking out
        draw_rect(img, 11 + dx, 24 + hop, 3, 2, c['dark'])
        draw_rect(img, 18 + dx, 24 + hop, 3, 2, c['dark'])

    def draw_attack(img, c, f):
        # Springs open to bite
        mouth = [0, 3, 6, 4][f]
        if f == 0:
            _draw_chest_closed(img, 9, 14, c)
        else:
            _draw_chest_open(img, 9, 12, c, mouth)
        # Eyes revealed when open
        if f >= 1:
            draw_eye(img, 13, 12 + mouth, 2, c['eye'])
            draw_eye(img, 19, 12 + mouth, 2, c['eye'])

    def draw_hurt(img, c, f):
        recoil = [1, 3][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(img, 9, 14 + recoil, 14, 10,
                               {'dark': c['dark'], 'mid': flash,
                                'light': c['highlight'], 'highlight': c['highlight']})
        draw_rect(img, 9, 14 + recoil, 14, 4, shade(c['dark'], 1.2))
        draw_eye(img, 14, 18 + recoil, 2, c['eye'])
        draw_eye(img, 19, 18 + recoil, 2, c['eye'])

    def draw_dead(img, c, f):
        squash = [0, 2, 4, 6][f]
        alpha_f = [1.0, 0.7, 0.4, 0.2][f]
        col = shade(c['mid'], alpha_f)
        body_h = max(2, 10 - squash)
        draw_rect(img, 9, 14 + squash, 14, body_h, col)
        draw_rect(img, 9, 14 + squash, 14, min(3, body_h), shade(c['dark'], alpha_f))
        # Scattered teeth/loot
        if f >= 2:
            draw_pixel(img, 7, 26, c['teeth'])
            draw_pixel(img, 24, 25, c['teeth'])
            draw_pixel(img, 12, 27, c['highlight'])

    _build_sheet('mimic', draw_idle, draw_walk, draw_attack, draw_hurt, draw_dead)


# ---------------------------------------------------------------------------
# Main generator
# ---------------------------------------------------------------------------

def generate_enemies():
    """Generate spritesheets for all 17 enemy types."""
    print("Generating enemy spritesheets...")
    generate_walker()
    generate_flyer()
    generate_turret()
    generate_charger()
    generate_phaser()
    generate_exploder()
    generate_shielder()
    generate_crawler()
    generate_summoner()
    generate_sniper()
    generate_minion()
    generate_teleporter()
    generate_reflector()
    generate_leech()
    generate_swarmer()
    generate_gravitywell()
    generate_mimic()
    print("All enemy spritesheets generated!")


if __name__ == '__main__':
    generate_enemies()
