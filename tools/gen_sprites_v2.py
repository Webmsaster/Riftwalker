"""
Riftwalker Sprite Generator v2 — Quality Overhaul
Generates game-quality sprites via ComfyUI with:
- Distinct poses per animation (not crop-shifting)
- Proper animation transforms (squash/stretch/lean)
- Post-processing: alpha cleanup, outline, palette consistency
"""
import sys, os, random, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comfyui_gen import generate_image, check_server
from PIL import Image, ImageFilter, ImageEnhance, ImageDraw

TEMP = 'tools/comfyui_temp_v2'
os.makedirs(TEMP, exist_ok=True)

# === STYLE ===
# Game sprite style: simple, readable at small sizes, strong silhouette
S = 'game character sprite, digital painting, simple clean design, strong silhouette, centered, single character, '
E = ', dark void background, full body visible, no background clutter, sharp edges, high contrast, game art style'
NEG = ("pixel art, pixelated, 8-bit, text, watermark, blurry, deformed, ugly, "
       "multiple characters, collage, border, frame, busy background, "
       "photo, photograph, 3d render, realistic")

# === POST-PROCESSING ===

def remove_background(img, threshold=40):
    """Remove background via flood-fill from image edges.
    Handles both dark AND light backgrounds by detecting edge color."""
    from collections import deque
    img = img.convert('RGBA')
    w, h = img.size
    pixels = img.load()

    # Sample edge pixels to detect background color
    edge_colors = []
    for x in range(0, w, max(1, w // 20)):
        edge_colors.append(pixels[x, 0][:3])
        edge_colors.append(pixels[x, h - 1][:3])
    for y in range(0, h, max(1, h // 20)):
        edge_colors.append(pixels[0, y][:3])
        edge_colors.append(pixels[w - 1, y][:3])

    # Average edge color
    avg_r = sum(c[0] for c in edge_colors) // len(edge_colors)
    avg_g = sum(c[1] for c in edge_colors) // len(edge_colors)
    avg_b = sum(c[2] for c in edge_colors) // len(edge_colors)

    def is_bg_pixel(r, g, b):
        """Check if pixel is similar to the detected background color."""
        dr = abs(r - avg_r)
        dg = abs(g - avg_g)
        db = abs(b - avg_b)
        return (dr + dg + db) < threshold * 3  # color distance threshold

    visited = set()
    queue = deque()

    # Seed from all edge pixels matching background color
    for x in range(w):
        for y in [0, h - 1]:
            r, g, b, a = pixels[x, y]
            if is_bg_pixel(r, g, b):
                queue.append((x, y))
    for y in range(1, h - 1):
        for x in [0, w - 1]:
            r, g, b, a = pixels[x, y]
            if is_bg_pixel(r, g, b):
                queue.append((x, y))

    # BFS flood fill
    while queue:
        cx, cy = queue.popleft()
        if (cx, cy) in visited:
            continue
        if cx < 0 or cy < 0 or cx >= w or cy >= h:
            continue
        r, g, b, a = pixels[cx, cy]
        if not is_bg_pixel(r, g, b):
            continue
        visited.add((cx, cy))
        pixels[cx, cy] = (0, 0, 0, 0)
        for dx, dy in [(-1,0),(1,0),(0,-1),(0,1)]:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in visited:
                queue.append((nx, ny))

    return img

def add_outline(img, color=(20, 20, 20, 200), thickness=1):
    """Add a dark outline around non-transparent pixels for readability."""
    img = img.convert('RGBA')
    w, h = img.size
    outline = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    pixels = img.load()
    out_pixels = outline.load()

    for y in range(h):
        for x in range(w):
            if pixels[x, y][3] < 50:  # transparent pixel
                # Check neighbors for non-transparent pixels
                has_neighbor = False
                for dx in range(-thickness, thickness + 1):
                    for dy in range(-thickness, thickness + 1):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < w and 0 <= ny < h and pixels[nx, ny][3] > 100:
                            has_neighbor = True
                            break
                    if has_neighbor:
                        break
                if has_neighbor:
                    out_pixels[x, y] = color

    # Composite: outline behind sprite
    result = Image.alpha_composite(outline, img)
    return result

def boost_saturation(img, factor=1.3):
    """Slightly boost color saturation for game-style vibrancy."""
    rgb = img.convert('RGB')
    enhancer = ImageEnhance.Color(rgb)
    rgb = enhancer.enhance(factor)
    # Re-apply alpha
    result = rgb.convert('RGBA')
    result.putalpha(img.split()[3])
    return result

def process_sprite(img, target_w, target_h, remove_bg=True):
    """Post-processing pipeline for game sprites.
    For dark characters, set remove_bg=False to keep the dark background
    (it blends naturally with the game's dark background).
    """
    img = img.convert('RGBA')

    if remove_bg:
        # Only for characters with bright/medium colors where bg removal helps
        img = remove_background(img, threshold=20)

    # Resize to target size
    img = img.resize((target_w, target_h), Image.LANCZOS)

    # Boost saturation + contrast for readability at small sizes
    img = boost_saturation(img, 1.4)
    rgb = img.convert('RGB')
    enhancer = ImageEnhance.Contrast(rgb)
    rgb = enhancer.enhance(1.3)
    # Boost brightness slightly so details pop at small scale
    bright = ImageEnhance.Brightness(rgb)
    rgb = bright.enhance(1.1)
    result = rgb.convert('RGBA')
    result.putalpha(img.split()[3])
    return result

# === ANIMATION TRANSFORMS ===
# These create frame variations from a single source image

def anim_idle(src, frame_idx, num_frames, fw, fh):
    """Idle: gentle breathing (vertical squash/stretch cycle)."""
    t = (frame_idx / num_frames) * 2 * math.pi
    # Subtle breathing: ±3% vertical scale
    scale_y = 1.0 + 0.03 * math.sin(t)
    scale_x = 1.0 - 0.015 * math.sin(t)  # inverse to maintain volume
    nw = max(1, int(fw * scale_x))
    nh = max(1, int(fh * scale_y))
    frame = src.resize((nw, nh), Image.LANCZOS)
    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = (fw - nw) // 2
    oy = fh - nh  # anchor at feet
    result.paste(frame, (ox, oy), frame)
    return result

def anim_walk(src, frame_idx, num_frames, fw, fh):
    """Walk: horizontal bob + lean + vertical bounce."""
    t = (frame_idx / num_frames) * 2 * math.pi
    # Horizontal shift (walking sway)
    shift_x = int(2 * math.sin(t))
    # Vertical bounce
    bounce_y = int(abs(2 * math.sin(t)))
    # Slight lean (shear)
    lean = 0.05 * math.sin(t)

    frame = src.copy()
    # Apply shear transform for lean
    w, h = frame.size
    shear_matrix = (1, lean, -lean * h / 2, 0, 1, 0)
    frame = frame.transform((w, h), Image.AFFINE, shear_matrix, Image.BILINEAR)
    frame = frame.resize((fw, fh), Image.LANCZOS)

    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    result.paste(frame, (shift_x, -bounce_y), frame)
    return result

def anim_attack(src, frame_idx, num_frames, fw, fh):
    """Attack: wind-up then strike forward with stretch."""
    progress = frame_idx / max(1, num_frames - 1)
    if progress < 0.3:
        # Wind-up: pull back
        shift_x = int(-3 * (progress / 0.3))
        scale_x = 0.95
    elif progress < 0.6:
        # Strike: lunge forward + stretch
        strike = (progress - 0.3) / 0.3
        shift_x = int(4 * strike)
        scale_x = 1.0 + 0.12 * strike
    else:
        # Recovery: return to normal
        recovery = (progress - 0.6) / 0.4
        shift_x = int(4 * (1 - recovery))
        scale_x = 1.0 + 0.12 * (1 - recovery)

    nw = max(1, int(fw * scale_x))
    nh = fh
    frame = src.resize((nw, nh), Image.LANCZOS)
    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = shift_x + (fw - nw) // 2
    result.paste(frame, (max(-fw//4, ox), 0), frame)
    return result

def anim_hurt(src, frame_idx, num_frames, fw, fh):
    """Hurt: recoil + flash white + squish."""
    progress = frame_idx / max(1, num_frames - 1)
    # Recoil back
    shift_x = int(-3 * (1 - progress))
    # Horizontal squish
    scale_x = 0.85 + 0.15 * progress
    nw = max(1, int(fw * scale_x))
    frame = src.resize((nw, fh), Image.LANCZOS)

    # White flash overlay (stronger at start)
    flash = 1.0 - progress
    if flash > 0.3:
        bright = ImageEnhance.Brightness(frame)
        frame = bright.enhance(1.0 + flash * 0.8)

    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = shift_x + (fw - nw) // 2
    result.paste(frame, (max(-fw//4, ox), 0), frame)
    return result

def anim_dead(src, frame_idx, num_frames, fw, fh):
    """Dead: collapse downward + fade out."""
    progress = frame_idx / max(1, num_frames - 1)
    # Vertical collapse
    scale_y = max(0.15, 1.0 - 0.85 * progress)
    # Horizontal spread
    scale_x = 1.0 + 0.3 * progress
    # Fade out
    alpha_mult = max(0.1, 1.0 - 0.7 * progress)

    nw = max(1, int(fw * scale_x))
    nh = max(1, int(fh * scale_y))
    frame = src.resize((nw, nh), Image.LANCZOS)

    # Apply alpha fade
    r, g, b, a = frame.split()
    a = a.point(lambda p: int(p * alpha_mult))
    frame = Image.merge('RGBA', (r, g, b, a))

    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = (fw - nw) // 2
    oy = fh - nh  # anchor at bottom (collapsing down)
    result.paste(frame, (ox, oy), frame)
    return result

ANIM_FUNCS = {
    "idle": anim_idle,
    "walk": anim_walk,
    "attack": anim_attack,
    "hurt": anim_hurt,
    "dead": anim_dead,
}

# === ENEMY DEFINITIONS ===
# Format: name -> (description, frame_w, frame_h, accent_color_hint)
# Format: name -> (description, frame_w, frame_h, remove_bg)
# remove_bg=False for dark characters (dark bg blends with game bg)
# remove_bg=True for bright/colorful characters where bg removal helps
ENEMIES = {
    "walker": ("dark robotic quadruped patrol bot, mechanical legs, single glowing red eye, armored shell, small ground robot", 32, 32, False),
    "flyer": ("dark winged drone creature, bat-like energy wings spread wide, hovering, red glowing eyes, aerial enemy", 32, 32, False),
    "turret": ("stationary gun turret, mounted cannon barrel, targeting laser, military defense platform, compact weapon", 32, 32, False),
    "charger": ("large horned beast, glowing orange horns, muscular charging pose, heavy armored front, aggressive bull creature", 32, 32, False),
    "phaser": ("ghostly translucent specter, ethereal flowing form, phasing between dimensions, purple glow, transparent edges", 32, 32, True),
    "exploder": ("round bloated green creature, toxic volatile body, unstable glowing cracks, about to burst, spherical enemy", 32, 32, True),
    "shielder": ("heavy armored knight with large cyan energy shield, defensive stance, bulky armor, tower shield", 32, 32, True),
    "crawler": ("dark spider creature, eight mechanical legs, chitin shell, green bioluminescent spots, low profile", 32, 24, False),
    "summoner": ("floating dark mage in robes, casting purple magic from hands, mystical hovering sorcerer, glowing staff", 32, 32, True),
    "sniper": ("thin sleek sniper unit, long rifle barrel, telescopic red eye, crouching pose, dark matte metal body", 32, 32, False),
    "teleporter": ("glitching digital creature, electric blue distortion, unstable holographic form, teleport sparks", 32, 32, True),
    "reflector": ("angular chrome mirror creature, prismatic reflective surface, geometric shape, light reflections", 32, 32, True),
    "leech": ("dark parasitic slug, tendril appendages reaching out, draining red energy veins, slimy purple body", 32, 32, False),
    "swarmer": ("tiny insect creature, dark body with yellow warning stripes, small bee-like, compact", 16, 16, False),
    "gravitywell": ("floating dark orb, gravitational distortion rings, warping space around it, deep black sphere, energy halo", 32, 32, False),
    "mimic": ("treasure chest with teeth and eyes, wooden chest opening revealing monster mouth, ambush predator, hinged lid", 32, 32, True),
    "minion": ("small floating purple sprite, simple magical orb creature, glowing purple wisp, summoned servant", 32, 32, True),
}

ENEMY_ANIMS = {
    "idle": ("standing still, front facing, idle pose, relaxed", 4),
    "walk": ("moving forward, walking pose, in motion, side stepping", 6),
    "attack": ("attacking aggressively, striking forward, combat pose, hostile action", 4),
    "hurt": ("hit and recoiling, taking damage, pain reaction, staggering back", 2),
    "dead": ("defeated and falling, collapsing, destroyed, dying", 4),
}

# === PLAYER ===
PLAYER_DESC = "sci-fi void warrior, dark purple armored suit, glowing blue visor, sleek futuristic design, athletic build"
PLAYER_ANIMS = {
    "idle": ("standing ready pose, front facing, arms at sides, balanced stance", 6),
    "run": ("running forward, legs mid-stride, arms pumping, dynamic movement", 8),
    "jump": ("jumping upward, legs tucked, arms raised, airborne pose", 3),
    "fall": ("falling downward, arms spread, legs dangling, descending", 3),
    "dash": ("dashing forward fast, blurred motion, horizontal lunge, speed lines", 4),
    "wallslide": ("sliding down wall, back against surface, one hand on wall, slow descent", 3),
    "attack": ("sword slash attack, weapon swinging, aggressive strike, combat action", 6),
    "hurt": ("hit and recoiling backward, pain reaction, flinching, stagger", 3),
    "dead": ("collapsed on ground, defeated, lying down, unconscious", 5),
}

# === BOSS DEFINITIONS ===
BOSSES = {
    "rift_guardian": {
        "desc": "massive armored guardian knight, huge glowing purple sword, heavy plate armor with rift energy cracks, imposing stance",
        "size": 64, "sheet": (512, 576),
    },
    "void_wyrm": {
        "desc": "enormous coiling void serpent dragon, body made of stars and void, cosmic scales, purple breath",
        "size": 64, "sheet": (512, 576),
    },
    "dimensional_architect": {
        "desc": "floating geometric entity, impossible shapes, mathematical horror, blue white energy patterns, abstract form",
        "size": 64, "sheet": (512, 576),
    },
    "temporal_weaver": {
        "desc": "spider-like clockwork creature, bronze mechanical body, golden temporal webs, clock face eyes",
        "size": 64, "sheet": (512, 576),
    },
    "void_sovereign": {
        "desc": "colossal dark void king, massive shadowy royal figure, crown of purple dark flames, commanding presence",
        "size": 96, "sheet": (768, 864),
    },
    "entropy_incarnate": {
        "desc": "chaos entity, reality fragmenting around it, distorted colors, pure entropy given form, glitch effect",
        "size": 64, "sheet": (512, 576),
    },
}

BOSS_ANIMS = {
    "idle": ("menacing idle stance, breathing, dark presence", 6),
    "move": ("advancing forward, moving menacingly", 6),
    "attack1": ("primary melee attack, powerful strike", 6),
    "attack2": ("ranged energy blast attack", 6),
    "attack3": ("area attack, ground slam shockwave", 6),
    "phase_transition": ("transforming, powering up, energy surge", 8),
    "hurt": ("taking damage, flinching in pain", 3),
    "enrage": ("enraged berserk mode, glowing intensely", 6),
    "dead": ("defeated, dissolving into particles", 8),
}

# Boss animation transform functions (similar but with more dramatic effects)
def boss_anim_idle(src, fi, nf, fw, fh):
    return anim_idle(src, fi, nf, fw, fh)

def boss_anim_move(src, fi, nf, fw, fh):
    return anim_walk(src, fi, nf, fw, fh)

def boss_anim_attack(src, fi, nf, fw, fh):
    """Boss attack: more dramatic stretch/lunge."""
    progress = fi / max(1, nf - 1)
    if progress < 0.25:
        shift_x = int(-5 * (progress / 0.25))
        scale_x = 0.92
    elif progress < 0.5:
        strike = (progress - 0.25) / 0.25
        shift_x = int(6 * strike)
        scale_x = 1.0 + 0.18 * strike
    else:
        recovery = (progress - 0.5) / 0.5
        shift_x = int(6 * (1 - recovery))
        scale_x = 1.0 + 0.18 * (1 - recovery)
    nw = max(1, int(fw * scale_x))
    frame = src.resize((nw, fh), Image.LANCZOS)
    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = max(-fw//3, shift_x + (fw - nw) // 2)
    result.paste(frame, (ox, 0), frame)
    return result

def boss_anim_phase(src, fi, nf, fw, fh):
    """Phase transition: pulsing glow + scale oscillation."""
    t = (fi / nf) * 2 * math.pi
    scale = 1.0 + 0.08 * math.sin(t * 2)
    nw = max(1, int(fw * scale))
    nh = max(1, int(fh * scale))
    frame = src.resize((nw, nh), Image.LANCZOS)
    # Brightness pulse
    bright = ImageEnhance.Brightness(frame)
    frame = bright.enhance(1.0 + 0.3 * abs(math.sin(t)))
    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    result.paste(frame, ((fw - nw) // 2, (fh - nh) // 2), frame)
    return result

def boss_anim_enrage(src, fi, nf, fw, fh):
    """Enrage: shake + red tint + scale up."""
    t = (fi / nf) * 2 * math.pi
    shake_x = int(3 * math.sin(t * 4))
    shake_y = int(2 * math.cos(t * 3))
    scale = 1.0 + 0.05 * (fi / nf)
    nw = max(1, int(fw * scale))
    nh = max(1, int(fh * scale))
    frame = src.resize((nw, nh), Image.LANCZOS)
    # Red tint for rage
    r, g, b, a = frame.split()
    r = r.point(lambda p: min(255, int(p * 1.2)))
    g = g.point(lambda p: int(p * 0.85))
    b = b.point(lambda p: int(p * 0.85))
    frame = Image.merge('RGBA', (r, g, b, a))
    result = Image.new('RGBA', (fw, fh), (0, 0, 0, 0))
    ox = max(0, min(fw - nw, (fw - nw) // 2 + shake_x))
    oy = max(0, min(fh - nh, (fh - nh) // 2 + shake_y))
    result.paste(frame, (ox, oy), frame)
    return result

BOSS_ANIM_FUNCS = {
    "idle": boss_anim_idle,
    "move": boss_anim_move,
    "attack1": boss_anim_attack,
    "attack2": boss_anim_attack,
    "attack3": boss_anim_attack,
    "phase_transition": boss_anim_phase,
    "hurt": anim_hurt,
    "enrage": boss_anim_enrage,
    "dead": anim_dead,
}

# === GENERATION FUNCTIONS ===

def gen_single_pose(desc, pose_desc, output_path, gen_size=512, steps=20, cfg=7.0):
    """Generate a single pose image via ComfyUI."""
    prompt = f"{S}{desc}, {pose_desc}{E}"
    seed = random.randint(1, 2**31)
    return generate_image(prompt, output_path, width=gen_size, height=gen_size,
                         steps=steps, cfg=cfg, seed=seed)

def extract_character(img, margin_pct=0.10):
    """Extract the character from center of generated image, removing excess background."""
    w, h = img.size
    margin = int(min(w, h) * margin_pct)
    cropped = img.crop((margin, margin, w - margin, h - margin))
    return cropped

def assemble_enemy_sheet(name, desc, fw, fh, remove_bg=False):
    """Generate and assemble one enemy sprite sheet with proper animations."""
    print(f"\n{'='*40}")
    print(f"ENEMY v2: {name} ({fw}x{fh}, bg_remove={remove_bg})")
    print(f"{'='*40}")

    edir = f'{TEMP}/enemy_{name}'
    os.makedirs(edir, exist_ok=True)

    COLS = 6
    rows = list(ENEMY_ANIMS.keys())
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new('RGBA', (sheet_w, sheet_h), (0, 0, 0, 0))

    for row_idx, anim_name in enumerate(rows):
        anim_desc, num_frames = ENEMY_ANIMS[anim_name]
        img_path = f'{edir}/{anim_name}.png'

        # Generate the base pose image
        if not os.path.exists(img_path):
            gen_single_pose(desc, anim_desc, img_path, gen_size=512, steps=20, cfg=7.0)

        if not os.path.exists(img_path):
            print(f"  SKIP: {img_path} not generated")
            continue

        # Load and extract character
        src = Image.open(img_path).convert('RGBA')
        char = extract_character(src, 0.10)

        # Process the base sprite
        base_sprite = process_sprite(char, fw, fh, remove_bg=remove_bg)

        # Generate animation frames using transform functions
        anim_func = ANIM_FUNCS[anim_name]
        for frame_idx in range(num_frames):
            frame = anim_func(base_sprite, frame_idx, num_frames, fw, fh)
            sheet.paste(frame, (frame_idx * fw, row_idx * fh), frame)

    output = f'assets/textures/enemies/{name}.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    fsize = os.path.getsize(output)
    print(f"  Sheet: {output} ({sheet_w}x{sheet_h}, {fsize} bytes)")
    return output

def assemble_player_sheet():
    """Generate and assemble the player sprite sheet."""
    print(f"\n{'='*50}")
    print(f"PLAYER v2 (32x48)")
    print(f"{'='*50}")

    pdir = f'{TEMP}/player'
    os.makedirs(pdir, exist_ok=True)

    fw, fh = 32, 48
    COLS = 8
    rows = list(PLAYER_ANIMS.keys())
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new('RGBA', (sheet_w, sheet_h), (0, 0, 0, 0))

    # Map player anims to transform functions
    player_anim_funcs = {
        "idle": anim_idle,
        "run": anim_walk,
        "jump": anim_idle,  # slight float
        "fall": anim_idle,
        "dash": anim_attack,  # forward lunge
        "wallslide": anim_idle,
        "attack": anim_attack,
        "hurt": anim_hurt,
        "dead": anim_dead,
    }

    for row_idx, anim_name in enumerate(rows):
        anim_desc, num_frames = PLAYER_ANIMS[anim_name]
        img_path = f'{pdir}/{anim_name}.png'

        if not os.path.exists(img_path):
            gen_single_pose(PLAYER_DESC, anim_desc, img_path, gen_size=512, steps=25, cfg=7.0)

        if not os.path.exists(img_path):
            print(f"  SKIP: {img_path}")
            continue

        src = Image.open(img_path).convert('RGBA')
        char = extract_character(src, 0.08)
        base_sprite = process_sprite(char, fw, fh, remove_bg=True)

        anim_func = player_anim_funcs[anim_name]
        for frame_idx in range(num_frames):
            frame = anim_func(base_sprite, frame_idx, num_frames, fw, fh)
            sheet.paste(frame, (frame_idx * fw, row_idx * fh), frame)

    output = 'assets/textures/player/player.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    fsize = os.path.getsize(output)
    print(f"  Sheet: {output} ({sheet_w}x{sheet_h}, {fsize} bytes)")
    return output

def assemble_boss_sheet(name, info):
    """Generate and assemble one boss sprite sheet."""
    desc = info["desc"]
    fsize = info["size"]
    sheet_w, sheet_h = info["sheet"]

    print(f"\n{'='*50}")
    print(f"BOSS v2: {name} ({fsize}x{fsize})")
    print(f"{'='*50}")

    bdir = f'{TEMP}/boss_{name}'
    os.makedirs(bdir, exist_ok=True)

    COLS = 8
    rows = list(BOSS_ANIMS.keys())
    sheet = Image.new('RGBA', (sheet_w, sheet_h), (0, 0, 0, 0))

    gen_size = 768 if fsize == 96 else 512

    for row_idx, anim_name in enumerate(rows):
        anim_desc, num_frames = BOSS_ANIMS[anim_name]
        img_path = f'{bdir}/{anim_name}.png'

        if not os.path.exists(img_path):
            gen_single_pose(desc, anim_desc, img_path, gen_size=gen_size, steps=25, cfg=7.0)

        if not os.path.exists(img_path):
            print(f"  SKIP: {img_path}")
            continue

        src = Image.open(img_path).convert('RGBA')
        char = extract_character(src, 0.08)
        base_sprite = process_sprite(char, fsize, fsize)

        anim_func = BOSS_ANIM_FUNCS[anim_name]
        for frame_idx in range(num_frames):
            frame = anim_func(base_sprite, frame_idx, num_frames, fsize, fsize)
            sheet.paste(frame, (frame_idx * fsize, row_idx * fsize), frame)

    output = f'assets/textures/bosses/{name}.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    file_size = os.path.getsize(output)
    print(f"  Sheet: {output} ({sheet_w}x{sheet_h}, {file_size} bytes)")
    return output

def gen_tileset():
    """Generate a clean, readable tileset."""
    print(f"\n{'='*50}")
    print(f"TILESET v2")
    print(f"{'='*50}")

    tdir = f'{TEMP}/tiles'
    os.makedirs(tdir, exist_ok=True)

    # Generate tile types individually for clarity
    tile_types = {
        "floor": "dark metal floor tile, seamless game texture, industrial grate, subtle blue light, top-down view, simple clean design",
        "wall": "solid dark wall tile, heavy metal plate, rivets and bolts, industrial, top-down game texture, simple blocky",
        "platform": "floating platform tile, glowing blue edge, dark metal surface, sci-fi platform, side view game tile",
        "hazard": "danger spike tile, sharp red spikes, warning stripes, hazardous floor, game trap tile",
        "door": "sci-fi door tile, sliding metal door, green access light, portal frame, game door sprite",
        "deco1": "dark tech decoration tile, cable conduit, wall panel, sci-fi detail, game decoration",
    }

    # Universal tileset: 32 cols x 12 rows of 16x16 tiles
    tile_w, tile_h = 16, 16
    sheet_cols, sheet_rows = 32, 12
    sheet = Image.new('RGBA', (sheet_cols * tile_w, sheet_rows * tile_h), (0, 0, 0, 0))

    generated_tiles = {}
    for tname, tdesc in tile_types.items():
        img_path = f'{tdir}/{tname}.png'
        if not os.path.exists(img_path):
            prompt = f"game texture tile, {tdesc}, seamless tileable, dark sci-fi, sharp edges, clean design"
            generate_image(prompt, img_path, width=512, height=512, steps=20, cfg=7.0,
                          seed=random.randint(1, 2**31))
        if os.path.exists(img_path):
            generated_tiles[tname] = Image.open(img_path).convert('RGBA')

    # Fill tileset sheet by slicing generated textures into 16x16 tiles
    tile_idx = 0
    for tname, src in generated_tiles.items():
        w, h = src.size
        # Extract multiple 16x16-sized regions from the source
        for gy in range(0, min(h, 256), 32):
            for gx in range(0, min(w, 256), 32):
                if tile_idx >= sheet_cols * sheet_rows:
                    break
                region = src.crop((gx, gy, gx + 32, gy + 32))
                tile = region.resize((tile_w, tile_h), Image.LANCZOS)
                # Boost contrast for small tiles
                enhancer = ImageEnhance.Contrast(tile.convert('RGB'))
                tile_rgb = enhancer.enhance(1.4)
                tile_final = tile_rgb.convert('RGBA')
                tile_final.putalpha(tile.split()[3] if tile.mode == 'RGBA' else Image.new('L', tile.size, 255))

                col = tile_idx % sheet_cols
                row = tile_idx // sheet_cols
                sheet.paste(tile_final, (col * tile_w, row * tile_h))
                tile_idx += 1

    # Fill remaining tiles with color variations of floor
    if "floor" in generated_tiles:
        floor_src = generated_tiles["floor"]
        while tile_idx < sheet_cols * sheet_rows:
            gx = random.randint(0, max(0, floor_src.width - 64))
            gy = random.randint(0, max(0, floor_src.height - 64))
            region = floor_src.crop((gx, gy, gx + 64, gy + 64))
            tile = region.resize((tile_w, tile_h), Image.LANCZOS)
            col = tile_idx % sheet_cols
            row = tile_idx // sheet_cols
            sheet.paste(tile, (col * tile_w, row * tile_h))
            tile_idx += 1

    output = 'assets/textures/tiles/tileset_universal.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    print(f"  Tileset: {output} ({sheet.width}x{sheet.height}, {os.path.getsize(output)} bytes)")

    # Dimension A tileset (blue-tinted)
    dim_a = sheet.copy()
    r, g, b, a = dim_a.split()
    b = b.point(lambda p: min(255, int(p * 1.3)))
    dim_a = Image.merge('RGBA', (r, g, b, a))
    dim_a.save('assets/textures/tiles/generated_tileset.png', 'PNG')
    print(f"  Dim A tileset saved")

    # Dimension B tileset (purple-tinted)
    dim_b = sheet.copy()
    r, g, b, a = dim_b.split()
    r = r.point(lambda p: min(255, int(p * 1.2)))
    b = b.point(lambda p: min(255, int(p * 1.2)))
    g = g.point(lambda p: int(p * 0.85))
    dim_b = Image.merge('RGBA', (r, g, b, a))
    dim_b.save('assets/textures/tiles/generated_tileset_dimB.png', 'PNG')
    print(f"  Dim B tileset saved")

    return output

def gen_pickups_and_projectiles():
    """Generate pickup and projectile sprite sheets."""
    print(f"\n{'='*50}")
    print(f"PICKUPS & PROJECTILES v2")
    print(f"{'='*50}")

    pdir = f'{TEMP}/pickups_v2'
    os.makedirs(pdir, exist_ok=True)

    pickups = {
        "health": "glowing green health crystal, healing item, bright emerald glow, game pickup",
        "shield": "glowing blue shield orb, protective barrier item, cyan energy sphere, game pickup",
        "rift_shard": "glowing purple rift crystal shard, dimensional energy, violet gem, game pickup",
        "speed": "glowing yellow speed bolt, lightning fast item, golden streak, game pickup",
        "xp": "glowing white experience star, wisdom orb, bright white sparkle, game pickup",
        "damage": "glowing red damage crystal, attack power item, crimson shard, game pickup",
        "multishot": "glowing orange triple arrow, multi-projectile item, spread shot, game pickup",
        "magnet": "glowing cyan magnet, attraction force item, magnetic pull, game pickup",
    }

    # 8 pickups in 2 rows of 4, each 16x16 in final sheet
    pickup_size = 16
    sheet = Image.new('RGBA', (pickup_size * 4, pickup_size * 2 + pickup_size * 3), (0, 0, 0, 0))
    # Actually: pickups.png format should be pickup_size * cols x pickup_size * rows
    # Current format: 8 columns x 1 row? Let me check...
    # From gen_placeholders.py: rift_icon = img.crop((32, 0, 48, 16)) — 3rd column
    # So pickups.png is arranged in columns of 16px width
    # 8 pickups * 16 = 128 wide, but original was 64x80
    # Let me use 8 cols x 1 row = 128x16, or match original format

    # Original was 64x80 = 4 cols x 5 rows of 16x16
    cols, rows = 4, 5
    sheet = Image.new('RGBA', (cols * pickup_size, rows * pickup_size), (0, 0, 0, 0))

    for idx, (name, desc) in enumerate(pickups.items()):
        img_path = f'{pdir}/{name}.png'
        if not os.path.exists(img_path):
            prompt = f"game item icon, {desc}, dark background, centered, simple clean design, sharp"
            generate_image(prompt, img_path, width=512, height=512, steps=15, cfg=7.0,
                          seed=random.randint(1, 2**31))
        if os.path.exists(img_path):
            src = Image.open(img_path).convert('RGBA')
            # Center crop
            w, h = src.size
            margin = int(w * 0.15)
            cropped = src.crop((margin, margin, w - margin, h - margin))
            item = cropped.resize((pickup_size, pickup_size), Image.LANCZOS)
            item = remove_background(item, threshold=30)

            col = idx % cols
            row = idx // cols
            sheet.paste(item, (col * pickup_size, row * pickup_size), item)

    output = 'assets/textures/pickups/pickups.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    print(f"  Pickups: {output} ({sheet.width}x{sheet.height})")

    # Projectiles: 4 types, each 16x16, in a single row
    proj_descs = [
        "purple energy bolt projectile, glowing void blast, game bullet",
        "blue laser beam projectile, thin energy ray, game bullet",
        "green acid glob projectile, toxic spit, game bullet",
        "red fire bolt projectile, flaming shot, game bullet",
    ]

    proj_sheet = Image.new('RGBA', (16 * 4, 16 * 3), (0, 0, 0, 0))
    for idx, desc in enumerate(proj_descs):
        img_path = f'{pdir}/proj_{idx}.png'
        if not os.path.exists(img_path):
            prompt = f"game projectile sprite, {desc}, dark background, centered, simple, glowing"
            generate_image(prompt, img_path, width=512, height=512, steps=15, cfg=7.0,
                          seed=random.randint(1, 2**31))
        if os.path.exists(img_path):
            src = Image.open(img_path).convert('RGBA')
            w, h = src.size
            margin = int(w * 0.2)
            cropped = src.crop((margin, margin, w - margin, h - margin))
            proj = cropped.resize((16, 16), Image.LANCZOS)
            proj = remove_background(proj, threshold=30)
            # Place in rows (3 rows for projectile animation: straight, angled, impact)
            for row in range(3):
                proj_sheet.paste(proj, (idx * 16, row * 16), proj)

    proj_output = 'assets/textures/projectiles/projectiles.png'
    os.makedirs(os.path.dirname(proj_output), exist_ok=True)
    proj_sheet.save(proj_output, 'PNG')
    print(f"  Projectiles: {proj_output} ({proj_sheet.width}x{proj_sheet.height})")

def gen_placeholders():
    """Regenerate all placeholders from the new sprite sheets."""
    print(f"\n{'='*50}")
    print(f"PLACEHOLDERS v2")
    print(f"{'='*50}")

    ph_dir = 'assets/textures/placeholders'
    os.makedirs(ph_dir, exist_ok=True)

    def extract_first(sheet_path, fw, fh, out_path):
        if not os.path.exists(sheet_path):
            print(f"  SKIP: {sheet_path}")
            return
        img = Image.open(sheet_path).convert('RGBA')
        frame = img.crop((0, 0, fw, fh))
        frame.save(out_path, 'PNG')
        print(f"  OK: {out_path}")

    # Player
    extract_first('assets/textures/player/player.png', 32, 48, f'{ph_dir}/player.png')

    # Enemies
    for name, (_, fw, fh, _) in ENEMIES.items():
        extract_first(f'assets/textures/enemies/{name}.png', fw, fh, f'{ph_dir}/{name}.png')

    # Bosses
    for name, info in BOSSES.items():
        s = info["size"]
        extract_first(f'assets/textures/bosses/{name}.png', s, s, f'{ph_dir}/{name}.png')

    # Tiles
    extract_first('assets/textures/tiles/tileset_universal.png', 16, 16, f'{ph_dir}/tile_solid.png')

    print("  All placeholders regenerated")


# === MAIN ===

if __name__ == "__main__":
    if not check_server():
        print("ERROR: ComfyUI not running!")
        sys.exit(1)

    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--only', help='Generate only: player, enemies, bosses, tiles, pickups, placeholders')
    parser.add_argument('--enemy', help='Generate only a specific enemy by name')
    parser.add_argument('--boss', help='Generate only a specific boss by name')
    parser.add_argument('--skip-gen', action='store_true', help='Skip ComfyUI generation, only reassemble from cached images')
    args = parser.parse_args()

    target = args.only

    if target is None or target == 'player':
        assemble_player_sheet()

    if target is None or target == 'enemies':
        if args.enemy:
            name = args.enemy
            if name in ENEMIES:
                desc, fw, fh, rbg = ENEMIES[name]
                assemble_enemy_sheet(name, desc, fw, fh, remove_bg=rbg)
        else:
            for name, (desc, fw, fh, rbg) in ENEMIES.items():
                assemble_enemy_sheet(name, desc, fw, fh, remove_bg=rbg)

    if target is None or target == 'bosses':
        if args.boss:
            name = args.boss
            if name in BOSSES:
                assemble_boss_sheet(name, BOSSES[name])
        else:
            for name, info in BOSSES.items():
                assemble_boss_sheet(name, info)

    if target is None or target == 'tiles':
        gen_tileset()

    if target is None or target == 'pickups':
        gen_pickups_and_projectiles()

    if target is None or target == 'placeholders':
        gen_placeholders()

    print(f"\n{'='*50}")
    print("ALL ASSETS v2 GENERATION COMPLETE!")
    print(f"{'='*50}")
