"""Generate all 17 enemy sprite sheets via ComfyUI."""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comfyui_gen import generate_image
from PIL import Image

TEMP = 'tools/comfyui_temp'
S = 'digital painting, game creature art, stylized, detailed, dark sci-fi, '
E = ', high quality, sharp details, single creature centered, dark void background, full body visible, no background clutter, game enemy design'

ENEMIES = {
    "walker": ("small dark robotic ground creature, mechanical legs, glowing red eye, patrol bot, quadruped robot", 32, 32),
    "flyer": ("flying drone enemy, bat-like dark energy wings, hovering in air, red glowing eyes, winged creature", 32, 32),
    "turret": ("stationary mounted gun turret, mechanical rotating barrel, targeting laser, military defense turret", 32, 32),
    "charger": ("fast armored beast creature, low to ground, charging horns glowing red, aggressive predator", 32, 32),
    "phaser": ("ghostly dimension-shifting creature, semi-transparent ethereal form, phasing in and out of reality, spectral", 32, 32),
    "exploder": ("bloated glowing volatile creature, unstable toxic green energy, about to explode, round body", 32, 32),
    "shielder": ("heavy armored enemy with cyan energy shield, defensive stance, fortress-like, shield bearer", 32, 32),
    "crawler": ("spider-like creature clinging to surface, multiple legs, dark chitin shell, green bioluminescent spots", 32, 24),
    "summoner": ("dark robed floating mage, casting purple dark magic, summoning portal, mystical sorcerer", 32, 32),
    "sniper": ("sleek long-range sniper enemy, telescopic red eye, thin profile, crouching pose, dark matte finish", 32, 32),
    "teleporter": ("glitching teleporting enemy, digital distortion effect, electric blue, unstable holographic form", 32, 32),
    "reflector": ("mirror-shielded angular enemy, reflective prismatic surface, chrome armor, light reflections", 32, 32),
    "leech": ("parasitic dark slug creature, tendril appendages, draining red energy, sickly purple veins", 32, 32),
    "swarmer": ("tiny fast insect creature, dark body with yellow warning stripes, bee-like swarm unit, small", 16, 16),
    "gravitywell": ("floating dark orb enemy, gravitational distortion rings around it, warping space, spherical, deep black core", 32, 32),
    "mimic": ("treasure chest mimic creature, opening wooden chest revealing teeth and tentacles, ambush predator", 32, 32),
    "minion": ("small dark floating sprite servant, simple magical creature, purple glow, summoned minion", 32, 32),
}

ANIMS = {
    "idle": "standing idle pose, front facing, relaxed",
    "walk": "walking movement pose, side view, legs moving forward",
    "attack": "attacking aggressive pose, striking forward, hostile",
    "hurt": "recoiling in pain, stagger backward, damaged",
    "dead": "collapsed defeated on ground, destroyed, broken"
}
ANIM_FRAMES = {"idle": 4, "walk": 6, "attack": 4, "hurt": 2, "dead": 4}

def gen_enemy(name, desc, fw, fh):
    """Generate one enemy's sprite sheet."""
    print(f"\n{'='*40}")
    print(f"ENEMY: {name} ({fw}x{fh})")
    print(f"{'='*40}")

    edir = f'{TEMP}/enemy_{name}'
    os.makedirs(edir, exist_ok=True)

    # Generate one image per animation
    for anim_name, anim_desc in ANIMS.items():
        prompt = f"{S}{desc}, {anim_desc}{E}"
        img_path = f'{edir}/{anim_name}.png'

        # Use 512x512 for standard, smaller for small enemies
        gen_size = 512
        generate_image(prompt, img_path, width=gen_size, height=gen_size,
                      steps=20, cfg=7.5, seed=random.randint(1, 2**31))

    # Assemble sprite sheet: 6 cols, 5 rows
    COLS = 6
    rows = ["idle", "walk", "attack", "hurt", "dead"]
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new('RGBA', (sheet_w, sheet_h), (0, 0, 0, 0))

    for row_idx, anim_name in enumerate(rows):
        img_path = f'{edir}/{anim_name}.png'
        if not os.path.exists(img_path):
            print(f"  MISSING: {img_path}")
            continue

        src = Image.open(img_path).convert('RGBA')
        w, h = src.size

        # Center crop to focus on creature
        margin = int(w * 0.12)
        char_region = src.crop((margin, margin, w - margin, h - margin))

        num_frames = ANIM_FRAMES[anim_name]
        cw, ch = char_region.size

        for frame_idx in range(num_frames):
            # Slight variation per frame
            shift_x = int((frame_idx - num_frames // 2) * (cw * 0.015))
            shift_y = int(abs(frame_idx - num_frames // 2) * (ch * -0.01))

            cx = cw // 2 + shift_x
            cy = ch // 2 + shift_y
            half = int(min(cw, ch) * 0.42)

            x1 = max(0, cx - half)
            y1 = max(0, cy - half)
            x2 = min(cw, cx + half)
            y2 = min(ch, cy + half)

            frame = char_region.crop((x1, y1, x2, y2))
            frame = frame.resize((fw, fh), Image.LANCZOS)
            sheet.paste(frame, (frame_idx * fw, row_idx * fh))

    output = f'assets/textures/enemies/{name}.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    print(f"  Sheet: {output} ({sheet_w}x{sheet_h}, {os.path.getsize(output)} bytes)")


if __name__ == "__main__":
    for name, (desc, fw, fh) in ENEMIES.items():
        gen_enemy(name, desc, fw, fh)
    print("\nALL 17 ENEMIES DONE!")
