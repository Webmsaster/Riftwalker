"""
Riftwalker Player Spritesheet Generator
Generates a 32x48 pixel art player character with 9 animation rows.
Sci-fi suit with glowing cyan visor.
"""

from palette import PLAYER, OUTLINE, TRANSPARENT
from config import PLAYER_FRAME, PLAYER_ANIMS, OUTPUT
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_body_with_shading,
    draw_eye, draw_outline_rect, apply_outline, save_sheet,
)

# Shorthand colors
SD = PLAYER['suit_dark']
SM = PLAYER['suit_mid']
SL = PLAYER['suit_light']
SH = PLAYER['suit_highlight']
VI = PLAYER['visor']
VG = PLAYER['visor_glow']
SK = PLAYER['skin']
BT = PLAYER['belt']
BO = PLAYER['boots']
OL = OUTLINE


# ---------------------------------------------------------------------------
# Helper: draw the base player character at a given pose
# ---------------------------------------------------------------------------

def _draw_helmet(frame, hx, hy, visor_bright=False):
    """Draw helmet (10w x 10h) at position hx, hy.
    visor_bright toggles visor glow pulse."""
    # Helmet shell outline
    draw_rect(frame, hx + 2, hy, 6, 1, OL)        # top edge
    draw_rect(frame, hx + 1, hy + 1, 1, 1, OL)    # top-left corner
    draw_rect(frame, hx + 8, hy + 1, 1, 1, OL)    # top-right corner
    draw_rect(frame, hx, hy + 2, 1, 6, OL)         # left edge
    draw_rect(frame, hx + 9, hy + 2, 1, 6, OL)     # right edge
    draw_rect(frame, hx + 1, hy + 8, 1, 1, OL)     # bottom-left
    draw_rect(frame, hx + 8, hy + 8, 1, 1, OL)     # bottom-right
    draw_rect(frame, hx + 2, hy + 9, 6, 1, OL)     # bottom edge

    # Helmet fill
    draw_rect(frame, hx + 2, hy + 1, 6, 1, SL)     # top highlight
    draw_rect(frame, hx + 1, hy + 2, 8, 6, SM)     # main fill
    draw_rect(frame, hx + 1, hy + 2, 2, 2, SL)     # top-left light
    draw_pixel(frame, hx + 1, hy + 2, SH)           # highlight spot
    draw_rect(frame, hx + 7, hy + 5, 2, 3, SD)     # bottom-right shadow
    draw_rect(frame, hx + 2, hy + 8, 6, 1, SD)     # bottom shadow

    # Visor slit (row 4-5 of helmet, indented)
    v_color = VG if visor_bright else VI
    draw_rect(frame, hx + 2, hy + 4, 6, 2, v_color)
    draw_pixel(frame, hx + 2, hy + 4, OL)           # visor edge left
    draw_pixel(frame, hx + 7, hy + 4, OL)           # visor edge right
    # Visor glint
    draw_pixel(frame, hx + 3, hy + 4, VG)


def _draw_torso(frame, tx, ty, lean=0):
    """Draw torso (10w x 14h) at position tx, ty.
    lean: -1 left, 0 center, 1 right."""
    ox = lean  # horizontal offset for lean
    # Outline
    draw_rect(frame, tx + ox, ty, 10, 14, OL)
    # Fill body
    draw_rect(frame, tx + 1 + ox, ty + 1, 8, 12, SM)
    # Top-left lighting
    draw_rect(frame, tx + 1 + ox, ty + 1, 8, 2, SL)
    draw_rect(frame, tx + 1 + ox, ty + 1, 2, 6, SL)
    draw_pixel(frame, tx + 1 + ox, ty + 1, SH)
    draw_pixel(frame, tx + 2 + ox, ty + 1, SH)
    # Right and bottom shadow
    draw_rect(frame, tx + 7 + ox, ty + 4, 2, 9, SD)
    draw_rect(frame, tx + 1 + ox, ty + 11, 8, 2, SD)
    # Chest detail line (suit seam)
    draw_rect(frame, tx + 5 + ox, ty + 3, 1, 8, SD)
    # Small energy indicator on chest
    draw_pixel(frame, tx + 3 + ox, ty + 4, VI)
    draw_pixel(frame, tx + 3 + ox, ty + 5, VI)


def _draw_belt(frame, bx, by, lean=0):
    """Draw belt (10w x 2h)."""
    ox = lean
    draw_rect(frame, bx + ox, by, 10, 2, OL)
    draw_rect(frame, bx + 1 + ox, by, 8, 2, BT)
    # Belt buckle
    draw_pixel(frame, bx + 4 + ox, by, SH)
    draw_pixel(frame, bx + 5 + ox, by, SH)


def _draw_legs(frame, lx, ly, left_off=0, right_off=0, spread=0):
    """Draw two legs. Offsets control vertical displacement for animation.
    spread: horizontal extra gap between legs."""
    leg_w = 4
    leg_h = 12

    # Left leg
    llx = lx
    lly = ly + left_off
    draw_rect(frame, llx, lly, leg_w, leg_h, OL)
    draw_rect(frame, llx + 1, lly + 1, leg_w - 2, leg_h - 2, SM)
    draw_rect(frame, llx + 1, lly + 1, leg_w - 2, 2, SL)
    draw_rect(frame, llx + 1, lly + leg_h - 4, leg_w - 2, 3, SD)
    # Boot
    draw_rect(frame, llx, lly + leg_h - 3, leg_w + 1, 3, OL)
    draw_rect(frame, llx + 1, lly + leg_h - 2, leg_w - 1, 1, BO)
    draw_pixel(frame, llx + 1, lly + leg_h - 3, BO)

    # Right leg
    rlx = lx + leg_w + 2 + spread
    rly = ly + right_off
    draw_rect(frame, rlx, rly, leg_w, leg_h, OL)
    draw_rect(frame, rlx + 1, rly + 1, leg_w - 2, leg_h - 2, SM)
    draw_rect(frame, rlx + 1, rly + 1, leg_w - 2, 2, SL)
    draw_rect(frame, rlx + 1, rly + leg_h - 4, leg_w - 2, 3, SD)
    # Boot
    draw_rect(frame, rlx - 1, rly + leg_h - 3, leg_w + 1, 3, OL)
    draw_rect(frame, rlx, rly + leg_h - 2, leg_w - 1, 1, BO)
    draw_pixel(frame, rlx + 1, rly + leg_h - 3, BO)


def _draw_arm(frame, ax, ay, length=10, raised=False, swing=0):
    """Draw a single arm. swing: vertical pixel offset for running."""
    arm_w = 3
    if raised:
        # Arm going up
        draw_rect(frame, ax, ay - length + 3, arm_w, length, OL)
        draw_rect(frame, ax + 1, ay - length + 4, arm_w - 2, length - 2, SM)
        draw_rect(frame, ax + 1, ay - length + 4, arm_w - 2, 2, SL)
    else:
        # Arm hanging or swinging
        draw_rect(frame, ax, ay + swing, arm_w, length, OL)
        draw_rect(frame, ax + 1, ay + 1 + swing, arm_w - 2, length - 2, SM)
        draw_rect(frame, ax + 1, ay + 1 + swing, arm_w - 2, 2, SL)
        draw_rect(frame, ax + 1, ay + length - 3 + swing, arm_w - 2, 2, SD)


def _draw_player_base(frame, y_off=0, visor_bright=False, lean=0,
                      left_leg=0, right_leg=0, leg_spread=0,
                      left_arm_swing=0, right_arm_swing=0,
                      arms_up=False):
    """Draw full player character centered in a 32x48 frame."""
    cx = 11   # center x for 10-wide body in 32-wide frame
    base_y = 2 + y_off

    # Head
    _draw_helmet(frame, cx, base_y, visor_bright)
    # Torso
    _draw_torso(frame, cx, base_y + 10, lean)
    # Belt
    _draw_belt(frame, cx, base_y + 24, lean)
    # Arms (behind body conceptually, drawn on sides)
    arm_x_l = cx - 3 + lean
    arm_x_r = cx + 10 + lean
    arm_y = base_y + 11
    if arms_up:
        _draw_arm(frame, arm_x_l, arm_y, length=10, raised=True)
        _draw_arm(frame, arm_x_r, arm_y, length=10, raised=True)
    else:
        _draw_arm(frame, arm_x_l, arm_y, length=10, swing=left_arm_swing)
        _draw_arm(frame, arm_x_r, arm_y, length=10, swing=right_arm_swing)
    # Legs
    _draw_legs(frame, cx + 1, base_y + 26, left_leg, right_leg, leg_spread)


# ---------------------------------------------------------------------------
# Animation row generators
# ---------------------------------------------------------------------------

def _gen_idle(fw, fh):
    """Row 0: Idle (6 frames) - subtle breathing, visor pulse."""
    frames = []
    # Breathing offsets (y): slight up-down
    breath = [0, 0, -1, -1, 0, 0]
    visor_pulse = [False, False, True, True, True, False]
    for i in range(6):
        f = create_frame(fw, fh)
        _draw_player_base(f, y_off=breath[i], visor_bright=visor_pulse[i])
        frames.append(apply_outline(f))
    return frames


def _gen_run(fw, fh):
    """Row 1: Run (8 frames) - legs pumping, arms swinging."""
    frames = []
    # Leg offsets: left_off, right_off to simulate pumping
    leg_l = [-3, -2, 0, 2, 3, 2, 0, -2]
    leg_r = [3, 2, 0, -2, -3, -2, 0, 2]
    arm_l = [3, 2, 0, -2, -3, -2, 0, 2]
    arm_r = [-3, -2, 0, 2, 3, 2, 0, -2]
    bob = [0, -1, 0, 0, 0, -1, 0, 0]
    for i in range(8):
        f = create_frame(fw, fh)
        _draw_player_base(
            f, y_off=bob[i],
            left_leg=leg_l[i], right_leg=leg_r[i],
            left_arm_swing=arm_l[i], right_arm_swing=arm_r[i],
        )
        frames.append(apply_outline(f))
    return frames


def _gen_jump(fw, fh):
    """Row 2: Jump (3 frames) - crouch, launch, arms up."""
    frames = []
    # Frame 0: crouch
    f0 = create_frame(fw, fh)
    _draw_player_base(f0, y_off=2, left_leg=-1, right_leg=-1)
    frames.append(apply_outline(f0))

    # Frame 1: launch (body up, legs together)
    f1 = create_frame(fw, fh)
    _draw_player_base(f1, y_off=-2, arms_up=True)
    frames.append(apply_outline(f1))

    # Frame 2: arms up, legs slightly bent
    f2 = create_frame(fw, fh)
    _draw_player_base(f2, y_off=-3, arms_up=True, left_leg=1, right_leg=1)
    frames.append(apply_outline(f2))

    return frames


def _gen_fall(fw, fh):
    """Row 3: Fall (3 frames) - legs dangling, arms up."""
    frames = []
    dangle_l = [2, 3, 2]
    dangle_r = [3, 2, 3]
    for i in range(3):
        f = create_frame(fw, fh)
        _draw_player_base(
            f, y_off=-2, arms_up=True,
            left_leg=dangle_l[i], right_leg=dangle_r[i],
        )
        frames.append(apply_outline(f))
    return frames


def _gen_dash(fw, fh):
    """Row 4: Dash (4 frames) - horizontal lean with blur lines."""
    frames = []
    leans = [0, 1, 1, 1]
    for i in range(4):
        f = create_frame(fw, fh)
        _draw_player_base(f, lean=leans[i], left_leg=-1, right_leg=2)
        # Speed blur lines behind character
        blur_count = [0, 2, 3, 1]
        for b in range(blur_count[i]):
            by = 14 + b * 8
            bx = 3 - i
            draw_rect(f, bx, by, 5, 1, SH)
            draw_rect(f, bx + 1, by + 1, 3, 1, SL)
        frames.append(apply_outline(f))
    return frames


def _gen_wallslide(fw, fh):
    """Row 5: WallSlide (3 frames) - gripping wall, sliding."""
    frames = []
    slide_off = [0, 1, 2]
    for i in range(3):
        f = create_frame(fw, fh)
        cx = 11
        base_y = 2 + slide_off[i]
        # Head
        _draw_helmet(f, cx, base_y, visor_bright=False)
        # Torso, slight lean toward wall (right side)
        _draw_torso(f, cx, base_y + 10, lean=1)
        # Belt
        _draw_belt(f, cx, base_y + 24, lean=1)
        # Left arm hanging
        _draw_arm(f, cx - 3 + 1, base_y + 11, length=10, swing=-1)
        # Right arm reaching up (gripping wall)
        _draw_arm(f, cx + 11, base_y + 11, length=10, raised=True)
        # Legs together, slightly bent
        _draw_legs(f, cx + 2, base_y + 26, left_off=1, right_off=0)
        # Wall contact particles
        draw_pixel(f, cx + 14, base_y + 10 + i * 4, SH)
        draw_pixel(f, cx + 14, base_y + 14 + i * 3, SM)
        frames.append(apply_outline(f))
    return frames


def _gen_attack(fw, fh):
    """Row 6: Attack (6 frames) - energy blade swing."""
    frames = []
    # Phases: windup(2), swing(2), follow-through(2)
    for i in range(6):
        f = create_frame(fw, fh)
        cx = 9  # shifted left to make room for blade
        base_y = 2

        # Head
        _draw_helmet(f, cx, base_y, visor_bright=(i >= 2))
        # Torso
        torso_lean = [0, 0, 0, 1, 1, 0][i]
        _draw_torso(f, cx, base_y + 10, lean=torso_lean)
        # Belt
        _draw_belt(f, cx, base_y + 24, lean=torso_lean)
        # Left arm normal
        _draw_arm(f, cx - 3 + torso_lean, base_y + 11, length=10, swing=0)
        # Right arm: attack swing positions
        arm_raised = [True, True, False, False, False, False][i]
        arm_swing = [0, 0, -2, 0, 2, 3][i]
        _draw_arm(f, cx + 10 + torso_lean, base_y + 11,
                  length=10, raised=arm_raised, swing=arm_swing)
        # Legs
        _draw_legs(f, cx + 1, base_y + 26)

        # Energy blade
        if i >= 1 and i <= 4:
            blade_x = cx + 13 + torso_lean
            blade_positions = {
                1: (blade_x, base_y + 4, 3, 10),   # raised
                2: (blade_x, base_y + 8, 8, 3),     # horizontal swing
                3: (blade_x - 2, base_y + 12, 10, 3),  # extended
                4: (blade_x, base_y + 16, 6, 3),    # follow through
            }
            bx, by, bw, bh = blade_positions[i]
            # Blade glow
            draw_rect(f, bx, by, bw, bh, VI)
            draw_rect(f, bx + 1, by, bw - 2, bh, VG)
            # Core bright line
            if bw > bh:
                draw_rect(f, bx + 1, by + bh // 2, bw - 2, 1, (255, 255, 255))
            else:
                draw_rect(f, bx + bw // 2, by + 1, 1, bh - 2, (255, 255, 255))

        frames.append(apply_outline(f))
    return frames


def _gen_hurt(fw, fh):
    """Row 7: Hurt (3 frames) - knocked back, flash."""
    frames = []
    knock = [0, 2, 1]
    flash_colors = [False, True, False]
    for i in range(3):
        f = create_frame(fw, fh)
        cx = 11 + knock[i]
        base_y = 2

        if flash_colors[i]:
            # Flash white-ish: draw with highlight colors
            _draw_helmet(f, cx, base_y, visor_bright=True)
            # White flash torso
            draw_rect(f, cx, base_y + 10, 10, 14, OL)
            draw_rect(f, cx + 1, base_y + 11, 8, 12, SH)
            _draw_belt(f, cx, base_y + 24)
            _draw_legs(f, cx + 1, base_y + 26, left_off=1, right_off=-1)
            # Arms flung back
            _draw_arm(f, cx - 4, base_y + 11, length=10, swing=2)
            _draw_arm(f, cx + 10, base_y + 11, length=10, swing=-2)
        else:
            _draw_helmet(f, cx, base_y, visor_bright=False)
            _draw_torso(f, cx, base_y + 10, lean=-1 if i == 2 else 0)
            _draw_belt(f, cx, base_y + 24)
            _draw_legs(f, cx + 1, base_y + 26, left_off=1, right_off=-1)
            _draw_arm(f, cx - 3, base_y + 11, length=10, swing=2)
            _draw_arm(f, cx + 10, base_y + 11, length=10, swing=-1)

        frames.append(apply_outline(f))
    return frames


def _gen_dead(fw, fh):
    """Row 8: Dead (5 frames) - collapse sequence."""
    frames = []
    for i in range(5):
        f = create_frame(fw, fh)

        if i == 0:
            # Still standing, wobble
            _draw_player_base(f, y_off=0, visor_bright=False,
                              left_leg=1, right_leg=-1)
        elif i == 1:
            # Knees buckling
            _draw_player_base(f, y_off=3, visor_bright=False,
                              left_leg=2, right_leg=2, lean=-1)
        elif i == 2:
            # Falling sideways
            cx = 10
            base_y = 8
            _draw_helmet(f, cx, base_y, visor_bright=False)
            _draw_torso(f, cx, base_y + 10, lean=-1)
            _draw_belt(f, cx, base_y + 24)
            _draw_legs(f, cx + 1, base_y + 26, left_off=3, right_off=2)
            _draw_arm(f, cx - 3, base_y + 13, length=8, swing=3)
            _draw_arm(f, cx + 10, base_y + 11, length=8, swing=2)
        elif i == 3:
            # Almost on ground, horizontal-ish
            # Draw a collapsed body near bottom of frame
            ground_y = 34
            # Torso on ground (horizontal)
            draw_rect(f, 5, ground_y, 18, 5, OL)
            draw_rect(f, 6, ground_y + 1, 16, 3, SM)
            draw_rect(f, 6, ground_y + 1, 8, 1, SL)
            draw_rect(f, 14, ground_y + 2, 8, 2, SD)
            # Head
            draw_rect(f, 2, ground_y - 2, 6, 6, OL)
            draw_rect(f, 3, ground_y - 1, 4, 4, SM)
            draw_rect(f, 3, ground_y, 3, 1, VI)
            # Legs trailing
            draw_rect(f, 22, ground_y + 1, 6, 4, OL)
            draw_rect(f, 23, ground_y + 2, 4, 2, BO)
            # Belt
            draw_rect(f, 18, ground_y + 1, 4, 3, BT)
        elif i == 4:
            # Fully collapsed, visor flickers off
            ground_y = 36
            draw_rect(f, 5, ground_y, 18, 4, OL)
            draw_rect(f, 6, ground_y + 1, 16, 2, SD)
            draw_rect(f, 6, ground_y + 1, 6, 1, SM)
            # Head (visor dim)
            draw_rect(f, 2, ground_y - 1, 6, 5, OL)
            draw_rect(f, 3, ground_y, 4, 3, SD)
            dim_visor = (0, 80, 100)
            draw_rect(f, 3, ground_y + 1, 3, 1, dim_visor)
            # Legs
            draw_rect(f, 22, ground_y, 6, 4, OL)
            draw_rect(f, 23, ground_y + 1, 4, 2, BO)
            # Belt
            draw_rect(f, 18, ground_y, 4, 3, BT)

        frames.append(apply_outline(f))
    return frames


# ---------------------------------------------------------------------------
# Main generator
# ---------------------------------------------------------------------------

def generate_player():
    """Generate the complete player spritesheet."""
    fw, fh = PLAYER_FRAME
    max_cols = max(anim[2] for anim in PLAYER_ANIMS)
    num_rows = len(PLAYER_ANIMS)

    sheet = create_sheet(fw, fh, max_cols, num_rows)

    generators = {
        0: _gen_idle,
        1: _gen_run,
        2: _gen_jump,
        3: _gen_fall,
        4: _gen_dash,
        5: _gen_wallslide,
        6: _gen_attack,
        7: _gen_hurt,
        8: _gen_dead,
    }

    for anim_name, row, count, loop in PLAYER_ANIMS:
        print(f"  Generating {anim_name} ({count} frames)...")
        gen_func = generators[row]
        anim_frames = gen_func(fw, fh)
        for col, frame in enumerate(anim_frames):
            paste_frame(sheet, frame, col, row, fw, fh)

    save_sheet(sheet, OUTPUT['player'], 'player.png')
    print("Player spritesheet complete!")


if __name__ == '__main__':
    generate_player()
