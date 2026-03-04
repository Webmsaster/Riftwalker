"""
Riftwalker Boss Spritesheet Generator
Generates spritesheets for 5 boss types with 9 animation rows each.
"""
import math

from palette import BOSS_COLORS, OUTLINE, TRANSPARENT
from config import OUTPUT, BOSS_FRAME, BOSS_LARGE_FRAME, BOSS_ANIMS, BOSS_TYPES
from utils import (
    create_sheet, create_frame, paste_frame,
    draw_rect, draw_pixel, draw_body_with_shading, draw_eye,
    draw_outline_rect, apply_outline, save_sheet,
    draw_circle, draw_outline_circle, draw_shaded_rect,
    rgba, shade, draw_line, lerp_color,
)


# ------------------------------------------------------------------ helpers --

def _max_cols():
    """Return the max frame count across all boss animations."""
    return max(fc for _, _, fc, _ in BOSS_ANIMS)


def _anim_frames(row_index):
    """Return frame count for a given animation row index."""
    return BOSS_ANIMS[row_index][2]


def _draw_energy_particle(img, cx, cy, t, radius, color):
    """Draw a small orbiting energy particle at angle t (radians)."""
    px = int(cx + radius * math.cos(t))
    py = int(cy + radius * math.sin(t))
    draw_pixel(img, px, py, color)
    draw_pixel(img, px + 1, py, shade(color, 0.7))


def _draw_energy_orbs(img, cx, cy, count, radius, t_offset, color):
    """Draw multiple orbiting energy orbs around a center point."""
    for i in range(count):
        angle = t_offset + (2 * math.pi * i / count)
        ox = int(cx + radius * math.cos(angle))
        oy = int(cy + radius * math.sin(angle))
        draw_circle(img, ox, oy, 1, color)


def _draw_crack_lines(img, cx, cy, length, count, color):
    """Draw radiating crack/shatter lines from a center point."""
    for i in range(count):
        angle = 2 * math.pi * i / count
        ex = int(cx + length * math.cos(angle))
        ey = int(cy + length * math.sin(angle))
        draw_line(img, cx, cy, ex, ey, color)


def _draw_explosion(img, cx, cy, radius, colors, t):
    """Draw an explosion effect at progress t (0.0 to 1.0)."""
    r = int(radius * t)
    if r > 0:
        draw_circle(img, cx, cy, r, colors['highlight'])
        if r > 2:
            draw_circle(img, cx, cy, r - 2, colors['light'])
        if r > 4:
            draw_circle(img, cx, cy, r - 4, colors['mid'])


# ------------------------------------------------ rift_guardian (64x64) ------

def _generate_rift_guardian():
    """Generate the Rift Guardian boss spritesheet.
    Dark magenta armored golem with massive fists and a glowing red eye.
    """
    c = BOSS_COLORS['rift_guardian']
    fw, fh = BOSS_FRAME
    cols = _max_cols()
    rows = len(BOSS_ANIMS)
    sheet = create_sheet(fw, fh, cols, rows)

    def _draw_base(frame, y_off=0, fist_off_l=0, fist_off_r=0, mouth_open=False):
        """Draw the golem base body at an optional y offset."""
        # Legs
        draw_shaded_rect(frame, 20, 48 + y_off, 8, 14, c['dark'], c['mid'], c['light'])
        draw_shaded_rect(frame, 36, 48 + y_off, 8, 14, c['dark'], c['mid'], c['light'])
        # Feet
        draw_rect(frame, 18, 60 + y_off, 12, 3, c['dark'])
        draw_rect(frame, 34, 60 + y_off, 12, 3, c['dark'])
        # Main body (torso)
        draw_body_with_shading(frame, 16, 22 + y_off, 32, 28, c)
        # Chest plate
        draw_shaded_rect(frame, 22, 28 + y_off, 20, 12, c['dark'], c['mid'], c['light'], c['highlight'])
        # Core glow in chest
        draw_circle(frame, 32, 34 + y_off, 3, c['core'])
        draw_pixel(frame, 32, 34 + y_off, (255, 255, 255))
        # Shoulders
        draw_outline_rect(frame, 10, 22 + y_off, 10, 10, c['mid'])
        draw_outline_rect(frame, 44, 22 + y_off, 10, 10, c['mid'])
        # Arms
        draw_shaded_rect(frame, 10, 32 + y_off, 8, 14, c['dark'], c['mid'], c['light'])
        draw_shaded_rect(frame, 46, 32 + y_off, 8, 14, c['dark'], c['mid'], c['light'])
        # Fists (massive)
        draw_outline_rect(frame, 7, 44 + y_off + fist_off_l, 14, 10, c['light'])
        draw_outline_rect(frame, 43, 44 + y_off + fist_off_r, 14, 10, c['light'])
        # Head
        draw_body_with_shading(frame, 22, 8 + y_off, 20, 16, c)
        # Helmet ridge
        draw_rect(frame, 24, 8 + y_off, 16, 3, c['light'])
        draw_rect(frame, 26, 6 + y_off, 12, 3, c['highlight'])
        # Single glowing red eye
        draw_eye(frame, 29, 15 + y_off, 4, c['eye'])
        draw_pixel(frame, 30, 15 + y_off, (255, 200, 200))
        if mouth_open:
            draw_rect(frame, 28, 21 + y_off, 8, 2, c['dark'])

    # Row 0: Idle (6 frames) - subtle floating/breathing
    for f in range(_anim_frames(0)):
        frame = create_frame(fw, fh)
        bob = [0, -1, -1, 0, 0, 0][f]
        _draw_base(frame, y_off=bob)
        # Core pulse
        if f in (1, 2):
            draw_circle(frame, 32, 34 + bob, 4, shade(c['core'], 1.2))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, fw, fh)

    # Row 1: Move (6 frames) - heavy lumbering walk
    for f in range(_anim_frames(1)):
        frame = create_frame(fw, fh)
        step = [-1, 0, 1, 1, 0, -1][f]
        leg_shift = [2, 0, -2, -2, 0, 2][f]
        _draw_base(frame, y_off=step)
        # Override legs with walking motion
        draw_shaded_rect(frame, 20, 48 + step + leg_shift, 8, 14, c['dark'], c['mid'], c['light'])
        draw_shaded_rect(frame, 36, 48 + step - leg_shift, 8, 14, c['dark'], c['mid'], c['light'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, fw, fh)

    # Row 2: Attack1 (6 frames) - ground pound with right fist
    for f in range(_anim_frames(2)):
        frame = create_frame(fw, fh)
        if f < 2:
            _draw_base(frame, fist_off_r=-8 - f * 4)
        elif f == 2:
            _draw_base(frame, fist_off_r=6)
            # Impact effect
            draw_rect(frame, 40, 58, 20, 3, c['highlight'])
        elif f == 3:
            _draw_base(frame, fist_off_r=4)
            _draw_crack_lines(frame, 50, 58, 8, 5, c['core'])
        else:
            _draw_base(frame, fist_off_r=max(0, 4 - (f - 3) * 2))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, fw, fh)

    # Row 3: Attack2 (6 frames) - double fist slam
    for f in range(_anim_frames(3)):
        frame = create_frame(fw, fh)
        if f < 2:
            _draw_base(frame, fist_off_l=-6 - f * 3, fist_off_r=-6 - f * 3)
        elif f == 2:
            _draw_base(frame, fist_off_l=6, fist_off_r=6)
            draw_rect(frame, 5, 58, 54, 4, c['highlight'])
        elif f == 3:
            _draw_base(frame, fist_off_l=4, fist_off_r=4)
            _draw_crack_lines(frame, 32, 60, 12, 8, c['core'])
        else:
            _draw_base(frame, fist_off_l=max(0, 4 - (f - 3) * 2),
                       fist_off_r=max(0, 4 - (f - 3) * 2))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, fw, fh)

    # Row 4: Attack3 (6 frames) - core beam blast
    for f in range(_anim_frames(4)):
        frame = create_frame(fw, fh)
        _draw_base(frame, mouth_open=(f >= 2))
        if f >= 2:
            beam_w = 4 + (f - 2) * 2
            draw_rect(frame, 32 - beam_w // 2, 34, beam_w, 30, c['core'])
            draw_rect(frame, 32 - beam_w // 4, 34, beam_w // 2, 30, (255, 255, 255))
        if f >= 1:
            draw_circle(frame, 32, 34, 4 + f, c['core'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, fw, fh)

    # Row 5: Phase Transition (8 frames) - armor cracking, core overload
    for f in range(_anim_frames(5)):
        frame = create_frame(fw, fh)
        _draw_base(frame, y_off=-f // 2)
        t = f / 7.0
        # Cracks appear on body
        if f >= 2:
            num_cracks = min(f - 1, 6)
            for i in range(num_cracks):
                cx = 20 + (i * 7) % 24
                cy = 25 + (i * 5) % 20
                draw_pixel(frame, cx, cy, c['core'])
                draw_pixel(frame, cx + 1, cy + 1, c['core'])
        # Core intensifies
        if f >= 4:
            draw_circle(frame, 32, 34 - f // 2, 5 + f - 4, c['core'])
        # Energy burst at peak
        if f >= 6:
            _draw_energy_orbs(frame, 32, 30, 6, 16 + f, t * math.pi, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 5, fw, fh)

    # Row 6: Hurt (3 frames) - recoil, flash
    for f in range(_anim_frames(6)):
        frame = create_frame(fw, fh)
        x_shift = [2, -2, 0][f]
        flash = c['highlight'] if f == 0 else c['mid']
        draw_body_with_shading(frame, 16 + x_shift, 22, 32, 28,
                               {'dark': c['dark'], 'mid': flash, 'light': c['light'],
                                'highlight': c['highlight']})
        # Simplified head and limbs at shifted position
        draw_body_with_shading(frame, 22 + x_shift, 8, 20, 16, c)
        draw_eye(frame, 29 + x_shift, 15, 4, c['eye'])
        draw_shaded_rect(frame, 20 + x_shift, 48, 8, 14, c['dark'], c['mid'], c['light'])
        draw_shaded_rect(frame, 36 + x_shift, 48, 8, 14, c['dark'], c['mid'], c['light'])
        # Fists
        draw_outline_rect(frame, 7 + x_shift, 44, 14, 10, c['light'])
        draw_outline_rect(frame, 43 + x_shift, 44, 14, 10, c['light'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 6, fw, fh)

    # Row 7: Enrage (6 frames) - glowing intensifies, posture widens
    for f in range(_anim_frames(7)):
        frame = create_frame(fw, fh)
        intensity = 0.8 + 0.4 * (f / 5.0)
        rage_c = {
            'dark': shade(c['dark'], intensity),
            'mid': shade(c['mid'], intensity),
            'light': shade(c['light'], intensity),
            'highlight': shade(c['highlight'], intensity),
        }
        # Wider stance
        spread = min(f, 3)
        _draw_base(frame, y_off=-1)
        # Override body with rage colors
        draw_body_with_shading(frame, 16 - spread, 22, 32 + spread * 2, 28, rage_c)
        # Core blazing
        draw_circle(frame, 32, 34, 4 + f, c['core'])
        draw_pixel(frame, 32, 34, (255, 255, 255))
        # Energy aura
        if f >= 3:
            _draw_energy_orbs(frame, 32, 32, 4, 20, f * 0.8, c['core'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 7, fw, fh)

    # Row 8: Dead (8 frames) - collapse, explode, scatter
    for f in range(_anim_frames(8)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 3:
            # Crumbling
            _draw_base(frame, y_off=f * 2)
            _draw_crack_lines(frame, 32, 34 + f * 2, 6 + f * 2, 4 + f, c['core'])
        elif f < 5:
            # Explosion
            _draw_explosion(frame, 32, 34, 20 + (f - 3) * 8, c, t)
        elif f < 7:
            # Debris scatter
            for i in range(8):
                angle = 2 * math.pi * i / 8
                dist = 10 + (f - 5) * 8
                dx = int(32 + dist * math.cos(angle))
                dy = int(34 + dist * math.sin(angle))
                draw_rect(frame, dx, dy, 3, 3, c['mid'])
            _draw_explosion(frame, 32, 34, max(0, 28 - (f - 4) * 6), c, 0.5)
        else:
            # Fading remains
            for i in range(5):
                rx = 12 + i * 9
                draw_rect(frame, rx, 56, 4, 3, shade(c['dark'], 0.5))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 8, fw, fh)

    save_sheet(sheet, OUTPUT['bosses'], 'rift_guardian.png')
    print("  Rift Guardian complete.")


# --------------------------------------------------- void_wyrm (64x64) ------

def _generate_void_wyrm():
    """Generate the Void Wyrm boss spritesheet.
    Teal-green serpent/dragon with sinuous body, fangs, and dripping venom.
    """
    c = BOSS_COLORS['void_wyrm']
    fw, fh = BOSS_FRAME
    cols = _max_cols()
    rows = len(BOSS_ANIMS)
    sheet = create_sheet(fw, fh, cols, rows)

    def _draw_segment(frame, x, y, w, h, is_head=False):
        """Draw a single serpent body segment."""
        draw_body_with_shading(frame, x, y, w, h, c)
        # Belly stripe (lighter underside)
        belly_y = y + h - h // 3
        draw_rect(frame, x + 2, belly_y, w - 4, h // 3 - 1, c['highlight'])

    def _draw_head(frame, x, y, fangs=False, venom=False):
        """Draw the wyrm head with optional fangs and venom."""
        # Head shape (wider than body)
        draw_body_with_shading(frame, x, y, 18, 14, c)
        # Snout
        draw_shaded_rect(frame, x + 14, y + 3, 8, 8, c['dark'], c['mid'], c['light'])
        # Eyes (pair)
        draw_eye(frame, x + 4, y + 3, 3, c['eye'])
        draw_eye(frame, x + 10, y + 3, 3, c['eye'])
        # Nostrils
        draw_pixel(frame, x + 19, y + 4, c['dark'])
        draw_pixel(frame, x + 19, y + 7, c['dark'])
        if fangs:
            draw_rect(frame, x + 16, y + 11, 2, 4, (255, 255, 255))
            draw_rect(frame, x + 20, y + 11, 2, 4, (255, 255, 255))
        if venom:
            for i in range(3):
                draw_pixel(frame, x + 17 + i, y + 15 + i, c['venom'])
                draw_pixel(frame, x + 21 + i, y + 16 + i, c['venom'])

    def _draw_body_wave(frame, base_x, base_y, segments, wave_phase, amplitude=3):
        """Draw a sinuous serpent body as connected segments."""
        seg_w, seg_h = 8, 10
        for i in range(segments):
            sx = base_x - i * (seg_w - 2)
            sy = base_y + int(amplitude * math.sin(wave_phase + i * 0.8))
            _draw_segment(frame, sx, sy, seg_w, seg_h)

    # Row 0: Idle (6 frames) - gentle undulation
    for f in range(_anim_frames(0)):
        frame = create_frame(fw, fh)
        phase = f * 0.5
        _draw_body_wave(frame, 40, 28, 5, phase, amplitude=2)
        # Tail
        draw_rect(frame, 4, 30 + int(2 * math.sin(phase + 4)), 6, 4, c['mid'])
        draw_pixel(frame, 2, 31 + int(2 * math.sin(phase + 4.5)), c['light'])
        _draw_head(frame, 38, 22 + int(2 * math.sin(phase)), venom=False)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, fw, fh)

    # Row 1: Move (6 frames) - fast slithering
    for f in range(_anim_frames(1)):
        frame = create_frame(fw, fh)
        phase = f * 0.8
        x_shift = f * 2 % 4 - 2
        _draw_body_wave(frame, 40 + x_shift, 28, 5, phase, amplitude=4)
        draw_rect(frame, 4 + x_shift, 30 + int(3 * math.sin(phase + 4)), 6, 4, c['mid'])
        _draw_head(frame, 38 + x_shift, 22 + int(3 * math.sin(phase)))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, fw, fh)

    # Row 2: Attack1 (6 frames) - lunging bite
    for f in range(_anim_frames(2)):
        frame = create_frame(fw, fh)
        phase = f * 0.3
        lunge = [0, 4, 10, 10, 6, 0][f]
        _draw_body_wave(frame, 36, 28, 5, phase, amplitude=2)
        _draw_head(frame, 38 + lunge, 22, fangs=True, venom=(f >= 3))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, fw, fh)

    # Row 3: Attack2 (6 frames) - venom spray
    for f in range(_anim_frames(3)):
        frame = create_frame(fw, fh)
        _draw_body_wave(frame, 38, 30, 5, f * 0.3, amplitude=2)
        _draw_head(frame, 36, 24, fangs=True, venom=True)
        # Venom projectiles
        if f >= 2:
            for i in range(f - 1):
                vx = 56 + i * 4
                vy = 30 + int(4 * math.sin(i * 1.5))
                if vx < fw:
                    draw_circle(frame, vx, vy, 2, c['venom'])
                    draw_pixel(frame, vx, vy, shade(c['venom'], 1.3))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, fw, fh)

    # Row 4: Attack3 (6 frames) - tail whip
    for f in range(_anim_frames(4)):
        frame = create_frame(fw, fh)
        # Body coils
        _draw_body_wave(frame, 40, 28, 4, f * 0.4, amplitude=3)
        _draw_head(frame, 38, 24)
        # Tail swipe arc
        tail_angle = -1.0 + f * 0.6
        for i in range(4):
            tx = int(10 + 12 * math.cos(tail_angle + i * 0.3))
            ty = int(40 + 10 * math.sin(tail_angle + i * 0.3))
            draw_rect(frame, tx, ty, 5, 4, c['light'])
        if f >= 2 and f <= 4:
            # Swipe trail
            draw_line(frame, 4, 44, 24, 36, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, fw, fh)

    # Row 5: Phase Transition (8 frames) - shedding skin, venom aura
    for f in range(_anim_frames(5)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        _draw_body_wave(frame, 38, 28, 5, f * 0.6, amplitude=3 + f)
        _draw_head(frame, 36, 22 - f // 2, fangs=True, venom=True)
        # Shedding particles
        if f >= 2:
            for i in range(f * 2):
                px = 10 + (i * 13) % 44
                py = 20 + (i * 7) % 30
                draw_pixel(frame, px, py, c['highlight'])
        # Venom aura
        if f >= 4:
            _draw_energy_orbs(frame, 38, 30, f - 2, 14 + f, t * math.pi * 2, c['venom'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 5, fw, fh)

    # Row 6: Hurt (3 frames)
    for f in range(_anim_frames(6)):
        frame = create_frame(fw, fh)
        x_shift = [3, -3, 0][f]
        flash = c['highlight'] if f == 0 else c['mid']
        flash_c = {'dark': c['dark'], 'mid': flash, 'light': c['light'], 'highlight': c['highlight']}
        # Simplified body
        for i in range(4):
            sx = 36 + x_shift - i * 8
            sy = 30 + int(2 * math.sin(i))
            draw_body_with_shading(frame, sx, sy, 8, 10, flash_c)
        _draw_head(frame, 36 + x_shift, 24)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 6, fw, fh)

    # Row 7: Enrage (6 frames) - venom glowing, body coils tighter
    for f in range(_anim_frames(7)):
        frame = create_frame(fw, fh)
        intensity = 0.8 + 0.4 * (f / 5.0)
        _draw_body_wave(frame, 38, 26, 5, f * 0.7, amplitude=4 + f)
        _draw_head(frame, 36, 20, fangs=True, venom=True)
        # Venom dripping from all segments
        for i in range(5):
            vx = 38 - i * 7
            vy = 38 + int(3 * math.sin(f * 0.5 + i))
            draw_pixel(frame, vx, vy, shade(c['venom'], intensity))
            draw_pixel(frame, vx, vy + 1, shade(c['venom'], intensity * 0.7))
        # Rage aura
        if f >= 3:
            _draw_energy_orbs(frame, 32, 32, 3, 18, f * 1.0, c['venom'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 7, fw, fh)

    # Row 8: Dead (8 frames) - coils loosen, collapse, dissolve
    for f in range(_anim_frames(8)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 3:
            # Loosening coils
            _draw_body_wave(frame, 38, 28 + f * 3, 5, f * 0.2, amplitude=1)
            _draw_head(frame, 36, 24 + f * 4, fangs=True)
        elif f < 5:
            # Collapsing to ground
            for i in range(5):
                sx = 10 + i * 9
                draw_rect(frame, sx, 50 + (f - 3) * 3, 7, 5, c['mid'])
            draw_body_with_shading(frame, 36, 48 + (f - 3) * 4, 16, 10, c)
        elif f < 7:
            # Dissolving
            for i in range(8):
                sx = 8 + i * 7
                sy = 52 + (i % 3)
                col = shade(c['mid'], 1.0 - t * 0.4)
                draw_rect(frame, sx, sy, 4, 3, col)
            # Venom pools
            for i in range(3):
                draw_circle(frame, 15 + i * 16, 58, 2, c['venom'])
        else:
            # Final remains
            for i in range(3):
                draw_circle(frame, 15 + i * 16, 58, 2, shade(c['venom'], 0.5))
            draw_rect(frame, 30, 55, 6, 3, shade(c['dark'], 0.5))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 8, fw, fh)

    save_sheet(sheet, OUTPUT['bosses'], 'void_wyrm.png')
    print("  Void Wyrm complete.")


# ---------------------------------------- dimensional_architect (64x64) ------

def _generate_dimensional_architect():
    """Generate the Dimensional Architect boss spritesheet.
    Deep purple floating geometric entity with rotating shapes and rift energy.
    """
    c = BOSS_COLORS['dimensional_architect']
    fw, fh = BOSS_FRAME
    cols = _max_cols()
    rows = len(BOSS_ANIMS)
    sheet = create_sheet(fw, fh, cols, rows)

    def _draw_base(frame, y_off=0, rotation=0.0, rift_active=False):
        """Draw the floating geometric entity."""
        cx, cy = 32, 30 + y_off

        # Central octahedron body (diamond shape with shading)
        draw_body_with_shading(frame, cx - 10, cy - 12, 20, 24, c)

        # Inner diamond highlights
        draw_shaded_rect(frame, cx - 6, cy - 8, 12, 16, c['dark'], c['mid'], c['light'], c['highlight'])

        # Central eye
        draw_eye(frame, cx - 2, cy - 2, 4, c['eye'])

        # Orbiting geometric fragments
        for i in range(4):
            angle = rotation + (math.pi / 2) * i
            ox = int(cx + 16 * math.cos(angle))
            oy = int(cy + 12 * math.sin(angle))
            # Small rotating squares
            draw_outline_rect(frame, ox - 3, oy - 3, 6, 6, c['light'])
            draw_pixel(frame, ox, oy, c['highlight'])

        # Floating bottom trail
        for i in range(3):
            ty = cy + 14 + i * 3
            tw = max(2, 8 - i * 2)
            draw_rect(frame, cx - tw // 2, ty, tw, 2, shade(c['mid'], 0.7 - i * 0.15))

        if rift_active:
            # Rift energy lines
            for i in range(3):
                angle = rotation + i * 2.1
                rx = int(cx + 20 * math.cos(angle))
                ry = int(cy + 16 * math.sin(angle))
                draw_line(frame, cx, cy, rx, ry, c['rift'])

    # Row 0: Idle (6 frames) - gentle floating, slow rotation
    for f in range(_anim_frames(0)):
        frame = create_frame(fw, fh)
        bob = int(2 * math.sin(f * 0.5))
        rot = f * 0.3
        _draw_base(frame, y_off=bob, rotation=rot)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, fw, fh)

    # Row 1: Move (6 frames) - faster rotation, lateral drift
    for f in range(_anim_frames(1)):
        frame = create_frame(fw, fh)
        bob = int(3 * math.sin(f * 0.7))
        rot = f * 0.6
        _draw_base(frame, y_off=bob, rotation=rot)
        # Movement trail
        trail_x = 20 - f * 2
        if trail_x > 4:
            draw_rect(frame, trail_x, 28 + bob, 3, 6, shade(c['rift'], 0.5))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, fw, fh)

    # Row 2: Attack1 (6 frames) - fragment launch
    for f in range(_anim_frames(2)):
        frame = create_frame(fw, fh)
        _draw_base(frame, rotation=f * 0.5)
        # Launching fragments outward
        if f >= 2:
            dist = (f - 2) * 8
            for i in range(4):
                angle = (math.pi / 2) * i + 0.4
                fx = int(32 + (16 + dist) * math.cos(angle))
                fy = int(30 + (12 + dist) * math.sin(angle))
                if 0 <= fx < fw - 4 and 0 <= fy < fh - 4:
                    draw_outline_rect(frame, fx - 2, fy - 2, 4, 4, c['rift'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, fw, fh)

    # Row 3: Attack2 (6 frames) - rift beam
    for f in range(_anim_frames(3)):
        frame = create_frame(fw, fh)
        _draw_base(frame, rotation=f * 0.4, rift_active=(f >= 1))
        # Beam from eye
        if f >= 2:
            beam_len = (f - 1) * 10
            draw_rect(frame, 34, 28, beam_len, 3, c['rift'])
            draw_rect(frame, 34, 29, beam_len, 1, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, fw, fh)

    # Row 4: Attack3 (6 frames) - dimensional collapse (area attack)
    for f in range(_anim_frames(4)):
        frame = create_frame(fw, fh)
        _draw_base(frame, rotation=f * 1.0)
        # Expanding rift circles
        if f >= 1:
            r = (f) * 5
            draw_outline_circle(frame, 32, 30, r, TRANSPARENT, c['rift'])
            if f >= 3:
                draw_outline_circle(frame, 32, 30, r - 4, TRANSPARENT, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, fw, fh)

    # Row 5: Phase Transition (8 frames) - deconstruct and reconstruct
    for f in range(_anim_frames(5)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 3:
            # Deconstructing - pieces floating apart
            spread = f * 4
            for i in range(6):
                angle = i * math.pi / 3
                px = int(32 + spread * math.cos(angle))
                py = int(30 + spread * math.sin(angle))
                draw_outline_rect(frame, px - 3, py - 3, 6, 6, c['light'])
            draw_eye(frame, 30, 28, 4, c['eye'])
        elif f < 6:
            # Rift energy swirling
            _draw_energy_orbs(frame, 32, 30, 8, 12, t * math.pi * 4, c['rift'])
            draw_circle(frame, 32, 30, 6, c['rift'])
            draw_eye(frame, 30, 28, 4, c['eye'])
        else:
            # Reconstructed, more powerful
            _draw_base(frame, rotation=f * 0.8, rift_active=True)
            draw_circle(frame, 32, 30, 4, c['rift'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 5, fw, fh)

    # Row 6: Hurt (3 frames)
    for f in range(_anim_frames(6)):
        frame = create_frame(fw, fh)
        x_shift = [3, -3, 0][f]
        flash = c['highlight'] if f == 0 else c['mid']
        # Flickering geometry
        draw_body_with_shading(frame, 22 + x_shift, 18, 20, 24,
                               {'dark': c['dark'], 'mid': flash, 'light': c['light'],
                                'highlight': c['highlight']})
        draw_eye(frame, 30 + x_shift, 28, 4, c['eye'])
        # Scattered fragments
        for i in range(3):
            draw_rect(frame, 12 + i * 15 + x_shift, 20 + i * 8, 4, 4, c['light'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 6, fw, fh)

    # Row 7: Enrage (6 frames) - faster rotation, rift energy overload
    for f in range(_anim_frames(7)):
        frame = create_frame(fw, fh)
        _draw_base(frame, rotation=f * 1.5, rift_active=True)
        # Extra rift energy orbiting
        _draw_energy_orbs(frame, 32, 30, 6 + f, 18, f * 1.2, c['rift'])
        # Pulsing eye
        eye_size = 4 + (f % 2)
        draw_eye(frame, 32 - eye_size // 2, 30 - eye_size // 2, eye_size, c['eye'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 7, fw, fh)

    # Row 8: Dead (8 frames) - shatters into fragments
    for f in range(_anim_frames(8)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 2:
            # Cracks forming
            _draw_base(frame, rotation=f * 0.2)
            _draw_crack_lines(frame, 32, 30, 8 + f * 4, 4 + f * 2, c['rift'])
        elif f < 4:
            # Shattering outward
            spread = (f - 2) * 10
            for i in range(8):
                angle = i * math.pi / 4
                fx = int(32 + spread * math.cos(angle))
                fy = int(30 + spread * math.sin(angle))
                if 0 <= fx < fw - 3 and 0 <= fy < fh - 3:
                    draw_rect(frame, fx, fy, 3, 3, c['light'])
            draw_circle(frame, 32, 30, max(1, 6 - (f - 2) * 2), c['rift'])
        elif f < 6:
            # Fragments dissolving
            for i in range(6):
                fx = 8 + i * 9
                fy = 10 + (i * 7) % 40
                col = shade(c['mid'], 1.0 - t * 0.5)
                draw_rect(frame, fx, fy, 3, 3, col)
        else:
            # Rift closing
            r = max(1, int(8 * (1.0 - t)))
            draw_outline_circle(frame, 32, 30, r, TRANSPARENT, shade(c['rift'], 0.4))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 8, fw, fh)

    save_sheet(sheet, OUTPUT['bosses'], 'dimensional_architect.png')
    print("  Dimensional Architect complete.")


# ------------------------------------------- temporal_weaver (64x64) ------

def _generate_temporal_weaver():
    """Generate the Temporal Weaver boss spritesheet.
    Golden clockwork entity with gears, floating, and time motifs.
    """
    c = BOSS_COLORS['temporal_weaver']
    fw, fh = BOSS_FRAME
    cols = _max_cols()
    rows = len(BOSS_ANIMS)
    sheet = create_sheet(fw, fh, cols, rows)

    def _draw_gear(frame, cx, cy, radius, angle, color, teeth=6):
        """Draw a simple gear with teeth at the given angle."""
        draw_circle(frame, cx, cy, radius, color)
        draw_circle(frame, cx, cy, max(1, radius - 2), shade(color, 0.8))
        # Teeth
        for i in range(teeth):
            t_angle = angle + (2 * math.pi * i / teeth)
            tx = int(cx + (radius + 1) * math.cos(t_angle))
            ty = int(cy + (radius + 1) * math.sin(t_angle))
            draw_pixel(frame, tx, ty, color)

    def _draw_clock_hand(frame, cx, cy, length, angle, color):
        """Draw a clock hand from center at given angle."""
        ex = int(cx + length * math.cos(angle))
        ey = int(cy + length * math.sin(angle))
        draw_line(frame, cx, cy, ex, ey, color)

    def _draw_base(frame, y_off=0, gear_angle=0.0, hands_angle=0.0):
        """Draw the temporal weaver base form."""
        cx, cy = 32, 28 + y_off

        # Main body - large central gear/clock face
        draw_outline_circle(frame, cx, cy, 14, c['mid'])
        draw_circle(frame, cx, cy, 12, c['light'])
        draw_circle(frame, cx, cy, 10, c['mid'])

        # Clock face markings (12 positions)
        for i in range(12):
            angle = (math.pi / 6) * i - math.pi / 2
            mx = int(cx + 9 * math.cos(angle))
            my = int(cy + 9 * math.sin(angle))
            draw_pixel(frame, mx, my, c['dark'])

        # Clock hands
        _draw_clock_hand(frame, cx, cy, 7, hands_angle, c['clock'])
        _draw_clock_hand(frame, cx, cy, 5, hands_angle * 0.3, c['dark'])

        # Central eye
        draw_eye(frame, cx - 2, cy - 2, 4, c['eye'])

        # Shoulder gears
        _draw_gear(frame, cx - 18, cy - 4, 5, gear_angle, c['clock'])
        _draw_gear(frame, cx + 18, cy - 4, 5, -gear_angle, c['clock'])

        # Floating lower gears
        _draw_gear(frame, cx - 10, cy + 16, 4, gear_angle * 0.7, c['mid'])
        _draw_gear(frame, cx + 10, cy + 16, 4, -gear_angle * 0.7, c['mid'])

        # Crown / top decoration
        draw_shaded_rect(frame, cx - 6, cy - 18, 12, 5, c['dark'], c['clock'], c['highlight'])
        draw_pixel(frame, cx, cy - 19, c['highlight'])
        draw_pixel(frame, cx - 4, cy - 17, c['highlight'])
        draw_pixel(frame, cx + 4, cy - 17, c['highlight'])

        # Floating trail below
        for i in range(3):
            ty = cy + 20 + i * 3
            tw = max(2, 6 - i * 2)
            draw_rect(frame, cx - tw // 2, ty, tw, 2, shade(c['clock'], 0.6 - i * 0.15))

    # Row 0: Idle (6 frames) - gears turning slowly, floating
    for f in range(_anim_frames(0)):
        frame = create_frame(fw, fh)
        bob = int(2 * math.sin(f * 0.5))
        gear_rot = f * 0.4
        hand_rot = -math.pi / 2 + f * 0.3
        _draw_base(frame, y_off=bob, gear_angle=gear_rot, hands_angle=hand_rot)
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, fw, fh)

    # Row 1: Move (6 frames) - faster rotation, drift
    for f in range(_anim_frames(1)):
        frame = create_frame(fw, fh)
        bob = int(3 * math.sin(f * 0.7))
        gear_rot = f * 0.7
        hand_rot = -math.pi / 2 + f * 0.5
        _draw_base(frame, y_off=bob, gear_angle=gear_rot, hands_angle=hand_rot)
        # Time trail
        for i in range(f):
            tx = 20 - i * 5
            if tx > 2:
                draw_pixel(frame, tx, 28 + bob, shade(c['clock'], 0.5))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, fw, fh)

    # Row 2: Attack1 (6 frames) - time bolt
    for f in range(_anim_frames(2)):
        frame = create_frame(fw, fh)
        _draw_base(frame, gear_angle=f * 0.6, hands_angle=-math.pi / 2 + f * 0.8)
        # Time bolt projectile
        if f >= 2:
            bolt_x = 46 + (f - 2) * 6
            if bolt_x < fw - 2:
                draw_outline_rect(frame, bolt_x, 26, 5, 5, c['clock'])
                draw_pixel(frame, bolt_x + 2, 28, c['highlight'])
                # Trail
                draw_line(frame, 46, 28, bolt_x, 28, shade(c['clock'], 0.6))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, fw, fh)

    # Row 3: Attack2 (6 frames) - time stop zone
    for f in range(_anim_frames(3)):
        frame = create_frame(fw, fh)
        _draw_base(frame, gear_angle=f * 0.3, hands_angle=-math.pi / 2)
        # Expanding clock zone
        if f >= 1:
            r = (f) * 6
            draw_outline_circle(frame, 32, 28, r, TRANSPARENT, c['clock'])
            # Clock marks in zone
            for i in range(4):
                angle = (math.pi / 2) * i
                mx = int(32 + (r - 2) * math.cos(angle))
                my = int(28 + (r - 2) * math.sin(angle))
                if 0 <= mx < fw and 0 <= my < fh:
                    draw_pixel(frame, mx, my, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, fw, fh)

    # Row 4: Attack3 (6 frames) - gear barrage
    for f in range(_anim_frames(4)):
        frame = create_frame(fw, fh)
        _draw_base(frame, gear_angle=f * 1.2, hands_angle=-math.pi / 2 + f * 1.0)
        # Launched gears
        if f >= 1:
            for i in range(min(f, 4)):
                gx = 48 + i * 5 + (f - 1) * 3
                gy = 20 + i * 8
                if gx < fw - 3 and gy < fh - 3:
                    _draw_gear(frame, gx, gy, 3, f * 1.5 + i, c['clock'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, fw, fh)

    # Row 5: Phase Transition (8 frames) - time rewind effect
    for f in range(_anim_frames(5)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 3:
            # Hands spinning wildly
            hand_rot = f * 3.0
            _draw_base(frame, gear_angle=f * 2.0, hands_angle=hand_rot)
        elif f < 6:
            # Time distortion - afterimages
            for lag in range(3):
                alpha_factor = 0.3 + lag * 0.2
                off = lag * 3
                draw_outline_circle(frame, 32 - off, 28, 12,
                                    shade(c['mid'], alpha_factor))
            _draw_base(frame, gear_angle=f * 1.5, hands_angle=f * 2.0)
        else:
            # Stabilized, enhanced
            _draw_base(frame, gear_angle=f * 0.5, hands_angle=-math.pi / 2 + f * 0.3)
            _draw_energy_orbs(frame, 32, 28, 6, 18, t * math.pi * 2, c['clock'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 5, fw, fh)

    # Row 6: Hurt (3 frames) - gears jam
    for f in range(_anim_frames(6)):
        frame = create_frame(fw, fh)
        x_shift = [3, -3, 0][f]
        flash = c['highlight'] if f == 0 else c['mid']
        # Body with flash
        draw_outline_circle(frame, 32 + x_shift, 28, 14, flash)
        draw_circle(frame, 32 + x_shift, 28, 10, c['mid'])
        draw_eye(frame, 30 + x_shift, 26, 4, c['eye'])
        # Jammed gears (tilted)
        _draw_gear(frame, 14 + x_shift, 24, 5, 0.3, c['dark'])
        _draw_gear(frame, 50 + x_shift, 24, 5, -0.3, c['dark'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 6, fw, fh)

    # Row 7: Enrage (6 frames) - overclock
    for f in range(_anim_frames(7)):
        frame = create_frame(fw, fh)
        speed = 1.0 + f * 0.5
        _draw_base(frame, gear_angle=f * speed, hands_angle=-math.pi / 2 + f * speed)
        # Sparks flying off gears
        for i in range(f + 1):
            angle = f * 1.5 + i * 1.2
            sx = int(32 + 20 * math.cos(angle))
            sy = int(28 + 16 * math.sin(angle))
            if 0 <= sx < fw and 0 <= sy < fh:
                draw_pixel(frame, sx, sy, c['highlight'])
                draw_pixel(frame, min(fw - 1, sx + 1), sy, c['clock'])
        # Intensified glow
        draw_circle(frame, 32, 28, 3, c['clock'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 7, fw, fh)

    # Row 8: Dead (8 frames) - gears fly apart, clock stops
    for f in range(_anim_frames(8)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        if f < 2:
            # Clock hands stop, cracks
            _draw_base(frame, gear_angle=0.0, hands_angle=-math.pi / 4)
            _draw_crack_lines(frame, 32, 28, 6 + f * 3, 3 + f * 2, c['clock'])
        elif f < 4:
            # Gears flying apart
            spread = (f - 2) * 12
            for i in range(6):
                angle = i * math.pi / 3
                gx = int(32 + spread * math.cos(angle))
                gy = int(28 + spread * math.sin(angle))
                if 0 <= gx < fw - 4 and 0 <= gy < fh - 4:
                    _draw_gear(frame, gx, gy, 3, f * 0.5, shade(c['clock'], 0.8))
            draw_circle(frame, 32, 28, max(1, 8 - (f - 2) * 3), c['mid'])
        elif f < 6:
            # Scattered gear fragments
            for i in range(8):
                gx = 4 + (i * 9) % 56
                gy = 10 + (i * 11) % 44
                draw_rect(frame, gx, gy, 3, 3, shade(c['clock'], 0.5))
        else:
            # Final ticking remnant
            draw_circle(frame, 32, 50, 2, shade(c['dark'], 0.4))
            if f == 7:
                draw_pixel(frame, 32, 50, shade(c['clock'], 0.3))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 8, fw, fh)

    save_sheet(sheet, OUTPUT['bosses'], 'temporal_weaver.png')
    print("  Temporal Weaver complete.")


# ------------------------------------------- void_sovereign (96x96) ------

def _generate_void_sovereign():
    """Generate the Void Sovereign boss spritesheet.
    Dark violet massive entity with crown, void energy orbs - the final boss.
    Uses 96x96 frame size.
    """
    c = BOSS_COLORS['void_sovereign']
    fw, fh = BOSS_LARGE_FRAME
    cols = _max_cols()
    rows = len(BOSS_ANIMS)
    sheet = create_sheet(fw, fh, cols, rows)

    def _draw_crown(frame, cx, cy, glow=False):
        """Draw the sovereign's crown."""
        # Crown base
        draw_shaded_rect(frame, cx - 14, cy, 28, 5, c['dark'], c['crown'], shade(c['crown'], 1.2))
        # Crown points
        for i in range(5):
            px = cx - 12 + i * 6
            py = cy - 4 - (2 if i % 2 == 0 else 0)
            draw_rect(frame, px, py, 4, cy - py + 1, c['crown'])
            draw_pixel(frame, px + 1, py, shade(c['crown'], 1.3))
        if glow:
            # Crown glow
            for i in range(5):
                px = cx - 11 + i * 6
                draw_pixel(frame, px, cy - 6 - (2 if i % 2 == 0 else 0), (255, 255, 200))

    def _draw_void_orbs(frame, cx, cy, count, radius, angle_off, color=None):
        """Draw void energy orbs orbiting the sovereign."""
        orb_c = color or c['void']
        for i in range(count):
            angle = angle_off + (2 * math.pi * i / count)
            ox = int(cx + radius * math.cos(angle))
            oy = int(cy + radius * math.sin(angle))
            draw_outline_circle(frame, ox, oy, 3, orb_c)
            draw_pixel(frame, ox, oy, c['highlight'])

    def _draw_base(frame, y_off=0, orb_angle=0.0, crown_glow=False, arms_up=False):
        """Draw the Void Sovereign base form."""
        cx, cy = 48, 44 + y_off

        # Ethereal lower body (flowing robe/void energy)
        for i in range(6):
            rw = max(4, 30 - i * 4)
            ry = cy + 20 + i * 4
            col = shade(c['mid'], 0.9 - i * 0.1)
            draw_rect(frame, cx - rw // 2, ry, rw, 4, col)

        # Main body (massive torso)
        draw_body_with_shading(frame, cx - 18, cy - 14, 36, 36, c)

        # Inner chest void core
        draw_circle(frame, cx, cy, 6, c['void'])
        draw_circle(frame, cx, cy, 3, c['highlight'])
        draw_pixel(frame, cx, cy, (255, 255, 255))

        # Shoulder pauldrons
        draw_body_with_shading(frame, cx - 26, cy - 12, 12, 12, c)
        draw_body_with_shading(frame, cx + 14, cy - 12, 12, 12, c)
        # Pauldron spikes
        draw_rect(frame, cx - 28, cy - 16, 4, 6, c['light'])
        draw_rect(frame, cx + 24, cy - 16, 4, 6, c['light'])

        # Arms
        if arms_up:
            # Arms raised
            draw_shaded_rect(frame, cx - 30, cy - 20, 8, 18, c['dark'], c['mid'], c['light'])
            draw_shaded_rect(frame, cx + 22, cy - 20, 8, 18, c['dark'], c['mid'], c['light'])
            # Hands with void energy
            draw_outline_rect(frame, cx - 32, cy - 24, 10, 8, c['light'])
            draw_outline_rect(frame, cx + 22, cy - 24, 10, 8, c['light'])
        else:
            # Arms at sides
            draw_shaded_rect(frame, cx - 28, cy - 4, 8, 20, c['dark'], c['mid'], c['light'])
            draw_shaded_rect(frame, cx + 20, cy - 4, 8, 20, c['dark'], c['mid'], c['light'])
            # Hands
            draw_outline_rect(frame, cx - 30, cy + 14, 10, 8, c['light'])
            draw_outline_rect(frame, cx + 20, cy + 14, 10, 8, c['light'])

        # Head
        draw_body_with_shading(frame, cx - 10, cy - 28, 20, 16, c)
        # Face
        draw_eye(frame, cx - 6, cy - 22, 5, c['eye'])
        draw_eye(frame, cx + 2, cy - 22, 5, c['eye'])
        # Mouth (sinister glow)
        draw_rect(frame, cx - 4, cy - 16, 8, 2, c['void'])

        # Crown
        _draw_crown(frame, cx, cy - 28, glow=crown_glow)

        # Void orbs
        _draw_void_orbs(frame, cx, cy, 3, 30, orb_angle)

    # Row 0: Idle (6 frames) - majestic floating, void orbs orbit
    for f in range(_anim_frames(0)):
        frame = create_frame(fw, fh)
        bob = int(3 * math.sin(f * 0.4))
        orb_rot = f * 0.4
        _draw_base(frame, y_off=bob, orb_angle=orb_rot, crown_glow=(f % 3 == 0))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 0, fw, fh)

    # Row 1: Move (6 frames) - gliding forward
    for f in range(_anim_frames(1)):
        frame = create_frame(fw, fh)
        bob = int(2 * math.sin(f * 0.6))
        orb_rot = f * 0.6
        _draw_base(frame, y_off=bob, orb_angle=orb_rot)
        # Void trail
        for i in range(f + 1):
            tx = 20 - i * 5
            ty = 50 + bob
            if tx > 0:
                draw_rect(frame, tx, ty, 4, 6, shade(c['void'], 0.5 - i * 0.08))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 1, fw, fh)

    # Row 2: Attack1 (6 frames) - void orb barrage
    for f in range(_anim_frames(2)):
        frame = create_frame(fw, fh)
        _draw_base(frame, orb_angle=f * 0.5, arms_up=(f >= 1 and f <= 3))
        # Launching void orbs
        if f >= 2:
            for i in range(min(f - 1, 3)):
                ox = 60 + (f - 2) * 8 + i * 6
                oy = 30 + i * 10
                if ox < fw - 3:
                    draw_outline_circle(frame, ox, oy, 3, c['void'])
                    draw_pixel(frame, ox, oy, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 2, fw, fh)

    # Row 3: Attack2 (6 frames) - void beam from chest core
    for f in range(_anim_frames(3)):
        frame = create_frame(fw, fh)
        _draw_base(frame, orb_angle=f * 0.3, arms_up=True)
        # Chest core charging then firing
        if f >= 1:
            charge = min(f, 3)
            draw_circle(frame, 48, 44, 6 + charge, c['void'])
        if f >= 3:
            beam_w = 4 + (f - 3) * 3
            draw_rect(frame, 54, 44 - beam_w // 2, 42, beam_w, c['void'])
            draw_rect(frame, 54, 44 - beam_w // 4, 42, beam_w // 2, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 3, fw, fh)

    # Row 4: Attack3 (6 frames) - summon void rift (area)
    for f in range(_anim_frames(4)):
        frame = create_frame(fw, fh)
        _draw_base(frame, orb_angle=f * 0.8, arms_up=True, crown_glow=True)
        # Void rift expanding beneath
        if f >= 1:
            rift_r = f * 6
            draw_outline_circle(frame, 48, 70, rift_r, TRANSPARENT, c['void'])
            if f >= 3:
                draw_outline_circle(frame, 48, 70, rift_r - 3, TRANSPARENT, c['highlight'])
            # Rift tendrils reaching up
            for i in range(min(f, 4)):
                angle = i * math.pi / 2 + f * 0.5
                tx = int(48 + (rift_r - 2) * math.cos(angle))
                ty_start = 70
                ty_end = 70 - f * 3
                if 0 <= tx < fw:
                    draw_line(frame, tx, ty_start, tx, max(ty_end, 0), c['void'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 4, fw, fh)

    # Row 5: Phase Transition (8 frames) - void cocoon, transformation
    for f in range(_anim_frames(5)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        cx, cy = 48, 44
        if f < 3:
            # Void energy drawing inward
            _draw_base(frame, orb_angle=f * 1.0)
            _draw_energy_orbs(frame, cx, cy, 8, max(5, 28 - f * 8), t * math.pi * 4, c['void'])
        elif f < 5:
            # Void cocoon
            cocoon_r = 20 + (f - 3) * 4
            draw_circle(frame, cx, cy, cocoon_r, c['dark'])
            draw_outline_circle(frame, cx, cy, cocoon_r, c['void'])
            # Pulsing lines
            for i in range(6):
                angle = i * math.pi / 3 + f * 0.8
                lx = int(cx + cocoon_r * math.cos(angle))
                ly = int(cy + cocoon_r * math.sin(angle))
                draw_line(frame, cx, cy, lx, ly, c['highlight'])
        elif f < 7:
            # Breaking out - more powerful
            _draw_base(frame, orb_angle=f * 0.5, arms_up=True, crown_glow=True)
            # Shatter particles
            spread = (f - 5) * 10
            for i in range(10):
                angle = i * math.pi / 5
                px = int(cx + spread * math.cos(angle))
                py = int(cy + spread * math.sin(angle))
                if 0 <= px < fw - 2 and 0 <= py < fh - 2:
                    draw_rect(frame, px, py, 3, 3, c['void'])
        else:
            # Final empowered form
            _draw_base(frame, orb_angle=f * 0.3, arms_up=True, crown_glow=True)
            _draw_void_orbs(frame, cx, cy, 6, 32, t * math.pi * 2, c['highlight'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 5, fw, fh)

    # Row 6: Hurt (3 frames) - massive recoil
    for f in range(_anim_frames(6)):
        frame = create_frame(fw, fh)
        x_shift = [4, -4, 0][f]
        flash = c['highlight'] if f == 0 else c['mid']
        cx, cy = 48 + x_shift, 44
        # Flash body
        flash_c = {'dark': c['dark'], 'mid': flash, 'light': c['light'], 'highlight': c['highlight']}
        draw_body_with_shading(frame, cx - 18, cy - 14, 36, 36, flash_c)
        # Head
        draw_body_with_shading(frame, cx - 10, cy - 28, 20, 16, c)
        draw_eye(frame, cx - 6, cy - 22, 5, c['eye'])
        draw_eye(frame, cx + 2, cy - 22, 5, c['eye'])
        _draw_crown(frame, cx, cy - 28)
        # Void orbs scattered
        for i in range(3):
            angle = i * 2.1
            ox = int(cx + 26 * math.cos(angle))
            oy = int(cy + 20 * math.sin(angle))
            if 0 <= ox < fw - 3 and 0 <= oy < fh - 3:
                draw_circle(frame, ox, oy, 2, c['void'])
        # Lower body
        for i in range(4):
            rw = max(4, 26 - i * 5)
            ry = cy + 20 + i * 4
            draw_rect(frame, cx - rw // 2, ry, rw, 4, shade(c['mid'], 0.8))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 6, fw, fh)

    # Row 7: Enrage (6 frames) - void overload, crown blazing
    for f in range(_anim_frames(7)):
        frame = create_frame(fw, fh)
        intensity = 0.8 + 0.4 * (f / 5.0)
        _draw_base(frame, orb_angle=f * 1.0, arms_up=True, crown_glow=True)
        # Void aura expanding
        aura_r = 20 + f * 3
        draw_outline_circle(frame, 48, 44, aura_r, TRANSPARENT, shade(c['void'], intensity * 0.6))
        # Extra orbs
        _draw_void_orbs(frame, 48, 44, 4 + f, 28, f * 0.8, shade(c['void'], intensity))
        # Crown fire
        for i in range(f + 1):
            fx = 36 + (i * 5) % 24
            draw_rect(frame, fx, 14 - i % 3, 3, 4, c['crown'])
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 7, fw, fh)

    # Row 8: Dead (8 frames) - epic death: void implosion then explosion
    for f in range(_anim_frames(8)):
        frame = create_frame(fw, fh)
        t = f / 7.0
        cx, cy = 48, 44
        if f < 2:
            # Staggering, cracks
            _draw_base(frame, y_off=f * 2, orb_angle=0)
            _draw_crack_lines(frame, cx, cy, 8 + f * 5, 6 + f * 2, c['void'])
        elif f < 4:
            # Void implosion - everything pulling inward
            imp_r = max(5, 25 - (f - 2) * 10)
            draw_circle(frame, cx, cy, imp_r, c['dark'])
            draw_outline_circle(frame, cx, cy, imp_r, c['void'])
            # Crown falling
            _draw_crown(frame, cx, cy - 10 + (f - 2) * 8)
            # Sucked-in particles
            for i in range(8):
                angle = i * math.pi / 4
                dist = imp_r + 10 - (f - 2) * 5
                px = int(cx + dist * math.cos(angle))
                py = int(cy + dist * math.sin(angle))
                if 0 <= px < fw and 0 <= py < fh:
                    draw_pixel(frame, px, py, c['highlight'])
        elif f < 6:
            # Massive void explosion
            exp_r = (f - 4) * 18
            _draw_explosion(frame, cx, cy, exp_r, c, t)
            # Void energy rings
            draw_outline_circle(frame, cx, cy, exp_r + 4, TRANSPARENT, c['void'])
        elif f == 6:
            # Fading blast
            for i in range(12):
                angle = i * math.pi / 6
                dist = 30 + i * 2
                px = int(cx + dist * math.cos(angle))
                py = int(cy + dist * math.sin(angle))
                if 0 <= px < fw - 2 and 0 <= py < fh - 2:
                    draw_rect(frame, px, py, 3, 3, shade(c['mid'], 0.6))
            # Crown remnant
            draw_rect(frame, cx - 4, cy + 20, 8, 3, shade(c['crown'], 0.5))
        else:
            # Final void scar
            draw_outline_circle(frame, cx, cy, 4, shade(c['void'], 0.3))
            draw_pixel(frame, cx, cy, shade(c['void'], 0.2))
            # Crown fragment
            draw_rect(frame, cx - 2, cy + 30, 4, 2, shade(c['crown'], 0.3))
        frame = apply_outline(frame)
        paste_frame(sheet, frame, f, 8, fw, fh)

    save_sheet(sheet, OUTPUT['bosses'], 'void_sovereign.png')
    print("  Void Sovereign complete.")


# ----------------------------------------------------------- main entry ------

def generate_bosses():
    """Generate spritesheets for all 5 boss types."""
    print("Generating boss spritesheets...")
    _generate_rift_guardian()
    _generate_void_wyrm()
    _generate_dimensional_architect()
    _generate_temporal_weaver()
    _generate_void_sovereign()
    print("All boss spritesheets generated!")


if __name__ == '__main__':
    generate_bosses()
