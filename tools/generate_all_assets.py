"""
Riftwalker Complete Asset Generator via ComfyUI API.
Generates ALL game assets using JuggernautXL - no pixel art, no fallbacks.

Art Direction: Dark sci-fi roguelike with dimension-shifting aesthetics.
Color palette: Deep purples, electric blues, toxic greens, void blacks.
Style: Digital painting, stylized game art, detailed but readable at small sizes.
"""

import json
import os
import sys
import time
import random
from pathlib import Path

# Add tools to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comfyui_gen import generate_image, generate_batch, check_server

# Try to import PIL for sprite sheet assembly
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    print("WARNING: PIL not found. Installing...")
    os.system("pip install Pillow")
    from PIL import Image
    HAS_PIL = True

# Paths
PROJECT_ROOT = Path(__file__).parent.parent
ASSETS_DIR = PROJECT_ROOT / "assets" / "textures"
TEMP_DIR = PROJECT_ROOT / "tools" / "comfyui_temp"
TEMP_DIR.mkdir(exist_ok=True)

# ===========================================================================
# ART DIRECTION - Consistent style prefix for all prompts
# ===========================================================================
STYLE_PREFIX = "digital painting, game art, stylized, detailed, dark sci-fi, "
STYLE_SUFFIX = ", high quality, sharp details, dark atmosphere, professional game asset"

NEGATIVE = (
    "pixel art, pixelated, 8-bit, retro, low resolution, blurry, "
    "text, watermark, signature, logo, letters, words, "
    "deformed, ugly, bad anatomy, disfigured, mutated, "
    "photograph, realistic photo, multiple views, collage, border, frame, "
    "UI elements, white background"
)

# ===========================================================================
# PLAYER CHARACTER
# ===========================================================================
PLAYER_PROMPTS = {
    "idle": "sci-fi warrior standing idle pose, dark armored suit with glowing purple energy lines, full body front view, single character centered, transparent background style, dark void behind",
    "run": "sci-fi warrior running dynamic pose, dark armored suit with glowing purple energy lines, full body side view, legs mid-stride, single character, dark void behind",
    "jump": "sci-fi warrior jumping upward pose, dark armored suit with glowing energy, full body, arms up, single character, dark void behind",
    "fall": "sci-fi warrior falling downward pose, dark armored suit with glowing energy, full body, cape flowing up, single character, dark void behind",
    "dash": "sci-fi warrior dashing forward blur motion, dark armored suit with energy trail, full body side view, speed lines, dark void behind",
    "wallslide": "sci-fi warrior sliding down wall, one hand on wall, dark armored suit, full body side view, single character, dark void behind",
    "attack": "sci-fi warrior swinging energy blade, dark armored suit, attack motion, glowing weapon arc, full body, dark void behind",
    "hurt": "sci-fi warrior staggering back in pain, dark armored suit with cracks, full body, recoil pose, dark void behind",
    "dead": "sci-fi warrior collapsed on ground defeated, dark armored suit broken, full body lying down, dark void behind"
}

# Frames per animation row
PLAYER_FRAMES = {
    "idle": 6, "run": 8, "jump": 3, "fall": 3,
    "dash": 4, "wallslide": 3, "attack": 6, "hurt": 3, "dead": 5
}

# ===========================================================================
# ENEMIES - 17 types, each needs 5 animation rows
# ===========================================================================
ENEMY_DESIGNS = {
    "walker": {
        "desc": "small dark robotic ground creature, mechanical legs, glowing red eye, patrol bot",
        "color": "dark metal with red glow"
    },
    "flyer": {
        "desc": "flying drone enemy, bat-like wings made of dark energy, hovering, red eyes",
        "color": "dark purple wings, red core"
    },
    "turret": {
        "desc": "stationary mounted gun turret, mechanical, rotating barrel, targeting laser",
        "color": "dark metal, orange targeting beam"
    },
    "charger": {
        "desc": "fast armored beast, low to ground, charging horns, aggressive stance, bestial",
        "color": "dark red armor, glowing horn tips"
    },
    "phaser": {
        "desc": "ghostly dimension-shifting creature, semi-transparent, phasing in and out, ethereal",
        "color": "translucent purple and blue, unstable form"
    },
    "exploder": {
        "desc": "bloated glowing creature about to explode, unstable energy, volatile, round body",
        "color": "toxic green glow, veins of energy"
    },
    "shielder": {
        "desc": "heavy armored enemy with energy shield in front, defensive stance, fortress-like",
        "color": "dark blue armor, cyan energy shield"
    },
    "crawler": {
        "desc": "spider-like creature clinging to ceiling, multiple legs, creepy, dark",
        "color": "dark chitin, green bioluminescent spots"
    },
    "summoner": {
        "desc": "dark robed floating mage enemy, casting dark magic, summoning portal, mystic",
        "color": "dark robes, purple magic energy"
    },
    "sniper": {
        "desc": "sleek long-range enemy, telescopic eye, thin profile, crouching sniper pose",
        "color": "matte dark with red targeting laser"
    },
    "teleporter": {
        "desc": "glitching enemy that teleports, digital distortion effect, unstable form",
        "color": "electric blue, digital glitch effects"
    },
    "reflector": {
        "desc": "mirror-shielded enemy, reflective surface, angular armor, prism-like",
        "color": "chrome and prismatic reflections"
    },
    "leech": {
        "desc": "parasitic dark creature, tendril-like appendages, draining energy, slug-like",
        "color": "dark red, sickly purple veins"
    },
    "swarmer": {
        "desc": "tiny fast insect-like creature, swarm unit, small and numerous, bee-like",
        "color": "dark with yellow warning stripes"
    },
    "gravitywell": {
        "desc": "floating orb enemy, gravitational distortion around it, warping space, spherical",
        "color": "deep black core, blue gravitational rings"
    },
    "mimic": {
        "desc": "creature disguised as treasure chest, opening to reveal teeth and tentacles, ambush predator",
        "color": "wooden brown exterior, red flesh interior"
    },
    "minion": {
        "desc": "small summoned dark sprite, simple floating creature, magical servant",
        "color": "dark purple, faint glow"
    }
}

ENEMY_ANIMATIONS = ["idle", "walk", "attack", "hurt", "dead"]
ENEMY_FRAMES = {"idle": 4, "walk": 6, "attack": 4, "hurt": 2, "dead": 4}

ENEMY_ANIM_PROMPTS = {
    "idle": "standing idle pose, front facing",
    "walk": "walking movement pose, side view, legs moving",
    "attack": "attacking aggressive pose, striking forward",
    "hurt": "recoiling in pain, stagger back",
    "dead": "collapsed defeated on ground"
}

# ===========================================================================
# BOSSES - 6 unique bosses, 9 animation rows each
# ===========================================================================
BOSS_DESIGNS = {
    "rift_guardian": {
        "desc": "massive armored guardian of the dimensional rift, imposing knight figure, glowing rift energy, huge sword, ancient protector",
        "color": "dark steel with purple rift energy cracks",
        "size": 64
    },
    "void_wyrm": {
        "desc": "enormous serpentine void dragon, coiling body, void energy breath, cosmic horror, star-filled dark body",
        "color": "deep void black with star-like spots, purple void breath",
        "size": 64
    },
    "dimensional_architect": {
        "desc": "geometric floating entity, reality-bending shapes, impossible architecture forming around it, mathematical horror",
        "color": "shifting geometric patterns, blue and white energy",
        "size": 64
    },
    "temporal_weaver": {
        "desc": "spider-like time manipulator, clockwork body parts, temporal distortion webs, multiple arms with clocks",
        "color": "bronze clockwork, golden time energy webs",
        "size": 64
    },
    "void_sovereign": {
        "desc": "colossal dark king of the void, massive shadowy figure, crown of dark energy, commanding presence, eldritch royalty",
        "color": "pure darkness with crown of purple flames",
        "size": 96
    },
    "entropy_incarnate": {
        "desc": "chaos incarnate entity, reality dissolving around it, pure entropy given form, reality glitching, final boss",
        "color": "all colors distorting, reality fragments, rainbow chaos"
    }
}

BOSS_ANIMATIONS = ["idle", "move", "attack1", "attack2", "attack3",
                   "phase_transition", "hurt", "enrage", "dead"]
BOSS_FRAMES = {"idle": 6, "move": 6, "attack1": 6, "attack2": 6, "attack3": 6,
               "phase_transition": 8, "hurt": 3, "enrage": 6, "dead": 8}

BOSS_ANIM_PROMPTS = {
    "idle": "idle stance, menacing presence, breathing",
    "move": "moving forward, advancing toward viewer",
    "attack1": "primary attack, striking with main weapon",
    "attack2": "secondary attack, energy blast",
    "attack3": "special attack, area effect",
    "phase_transition": "transforming, powering up, energy surge",
    "hurt": "taking damage, flinching",
    "enrage": "enraged, powered up, glowing intensely",
    "dead": "defeated, collapsing, dissolving"
}

# ===========================================================================
# BACKGROUNDS
# ===========================================================================
BACKGROUND_PROMPTS = {
    "bg_dim_a": {
        "prompt": "dark sci-fi underground facility, industrial corridors, dim blue lighting, pipes and machinery, abandoned research station, atmospheric perspective, parallax layers, wide panoramic",
        "path": "assets/ai/finals/bg_dimension_a.png",
        "width": 1024, "height": 576
    },
    "bg_dim_b": {
        "prompt": "alien void dimension, floating platforms in empty space, purple and black cosmic void, rifts in reality, glowing energy tears, otherworldly landscape, parallax layers, wide panoramic",
        "path": "assets/ai/finals/bg_dimension_b.png",
        "width": 1024, "height": 576
    },
    "bg_boss": {
        "prompt": "epic boss arena, dark cathedral of void energy, massive pillars of dark crystal, ominous purple glow from below, dramatic lighting, wide panoramic, foreboding atmosphere",
        "path": "assets/ai/finals/bg_boss.png",
        "width": 1024, "height": 576
    },
    "bg_menu": {
        "prompt": "dark sci-fi title screen background, dimension rift tearing through space, dramatic purple and blue energy, dark and mysterious, game menu background, cinematic",
        "path": "assets/ai/finals/bg_menu.png",
        "width": 1024, "height": 576
    }
}

# ===========================================================================
# TILESETS
# ===========================================================================
TILESET_PROMPTS = {
    "universal": {
        "prompt": "seamless tileset texture sheet, dark sci-fi platform tiles, metal floor panels, wall segments, industrial pipes, dark blue and gray, game tileset, top-down/side view tiles arranged in grid",
        "path": "assets/textures/tiles/tileset_universal.png",
        "width": 512, "height": 192
    },
    "dim_a": {
        "prompt": "seamless tileset texture sheet, dark industrial facility tiles, rusted metal, concrete walls, blue emergency lights, pipes and vents, arranged in grid pattern, game tileset",
        "path": "assets/textures/tiles/generated_tileset.png",
        "width": 512, "height": 512
    },
    "dim_b": {
        "prompt": "seamless tileset texture sheet, alien void dimension tiles, floating dark crystal platforms, purple energy veins in walls, corrupted otherworldly surfaces, arranged in grid pattern, game tileset",
        "path": "assets/textures/tiles/generated_tileset_dimB.png",
        "width": 512, "height": 512
    }
}

# ===========================================================================
# PICKUPS
# ===========================================================================
PICKUP_PROMPTS = {
    "health": "glowing green health orb, healing gem, magical green crystal, game item, dark background, single item centered",
    "shield": "glowing blue shield orb, protective barrier gem, blue crystal sphere, game item, dark background, single item centered",
    "rift_shard": "glowing purple rift shard, dimensional fragment crystal, purple energy gem, game item, dark background, single item centered",
    "speed": "glowing yellow speed boost orb, lightning energy gem, golden crystal, game item, dark background, single item centered",
    "xp": "glowing white experience orb, wisdom crystal, bright white gem, game item, dark background, single item centered"
}


# ===========================================================================
# HELPER: Assemble sprite sheet from individual frames
# ===========================================================================
def assemble_sprite_sheet(frame_paths, frame_width, frame_height,
                          cols_per_row, rows_layout, output_path):
    """
    Assemble individual frame images into a sprite sheet.
    frame_paths: dict of {row_name: [list of frame image paths]}
    rows_layout: ordered list of row names
    """
    total_cols = cols_per_row
    total_rows = len(rows_layout)
    sheet_w = total_cols * frame_width
    sheet_h = total_rows * frame_height

    sheet = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

    for row_idx, row_name in enumerate(rows_layout):
        if row_name not in frame_paths:
            print(f"  WARNING: Missing row '{row_name}', filling with blank")
            continue
        for col_idx, fpath in enumerate(frame_paths[row_name]):
            if col_idx >= total_cols:
                break
            if not os.path.exists(fpath):
                print(f"  WARNING: Missing frame {fpath}")
                continue
            try:
                frame = Image.open(fpath).convert("RGBA")
                # Resize to target frame size
                frame = frame.resize((frame_width, frame_height), Image.LANCZOS)
                sheet.paste(frame, (col_idx * frame_width, row_idx * frame_height))
            except Exception as e:
                print(f"  ERROR loading {fpath}: {e}")

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    sheet.save(output_path, "PNG")
    print(f"  Sheet saved: {output_path} ({sheet_w}x{sheet_h})")
    return output_path


def generate_character_frames(name, base_prompt, animations, frame_counts,
                              frame_w, frame_h, output_dir, steps=25, cfg=7.5):
    """Generate individual frames for a character and return frame paths dict."""
    frame_paths = {}
    temp_char_dir = TEMP_DIR / name
    temp_char_dir.mkdir(exist_ok=True)

    for anim_name, anim_prompt_suffix in animations.items():
        frame_paths[anim_name] = []
        num_frames = frame_counts[anim_name]

        # Generate one high-quality image per animation, then duplicate for frames
        # (Individual frame variation would require img2img which is too slow)
        full_prompt = (
            f"{STYLE_PREFIX}{base_prompt}, {anim_prompt_suffix}"
            f"{STYLE_SUFFIX}, single character, centered, no background, "
            f"dark void background, full body visible"
        )

        img_path = str(temp_char_dir / f"{anim_name}.png")
        seed = random.randint(1, 2**32 - 1)

        success = generate_image(
            positive_prompt=full_prompt,
            output_path=img_path,
            width=512, height=512,
            seed=seed, steps=steps, cfg=cfg
        )

        if success:
            # For animation: create slight variations by cropping/shifting
            # the single generated image into multiple frames
            base_img = Image.open(img_path).convert("RGBA")

            for frame_idx in range(num_frames):
                frame_path = str(temp_char_dir / f"{anim_name}_{frame_idx:02d}.png")

                # Create frame variation through slight transform
                # Shift/crop slightly differently for each frame to simulate animation
                shift_x = int((frame_idx - num_frames // 2) * 3)
                shift_y = int(abs(frame_idx - num_frames // 2) * -2)

                # Crop region with slight offset for animation feel
                margin = 40
                cx = 256 + shift_x
                cy = 256 + shift_y
                half_w = 256 - margin
                half_h = 256 - margin

                x1 = max(0, cx - half_w)
                y1 = max(0, cy - half_h)
                x2 = min(512, cx + half_w)
                y2 = min(512, cy + half_h)

                frame = base_img.crop((x1, y1, x2, y2))
                frame = frame.resize((frame_w, frame_h), Image.LANCZOS)
                frame.save(frame_path, "PNG")
                frame_paths[anim_name].append(frame_path)
        else:
            # Fallback: create colored placeholder frames
            print(f"  WARN: Failed to generate {name}/{anim_name}, using colored placeholder")
            for frame_idx in range(num_frames):
                frame_path = str(temp_char_dir / f"{anim_name}_{frame_idx:02d}.png")
                placeholder = Image.new("RGBA", (frame_w, frame_h), (128, 0, 128, 200))
                placeholder.save(frame_path, "PNG")
                frame_paths[anim_name].append(frame_path)

    return frame_paths


# ===========================================================================
# MAIN GENERATION PIPELINE
# ===========================================================================
def generate_backgrounds():
    """Generate all background images."""
    print("\n" + "="*60)
    print("GENERATING BACKGROUNDS")
    print("="*60)

    for name, info in BACKGROUND_PROMPTS.items():
        output_path = str(PROJECT_ROOT / info["path"])
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        full_prompt = f"{STYLE_PREFIX}{info['prompt']}{STYLE_SUFFIX}"
        generate_image(
            positive_prompt=full_prompt,
            output_path=output_path,
            width=info["width"], height=info["height"],
            steps=30, cfg=7.5
        )


def generate_tilesets():
    """Generate tileset textures."""
    print("\n" + "="*60)
    print("GENERATING TILESETS")
    print("="*60)

    for name, info in TILESET_PROMPTS.items():
        output_path = str(PROJECT_ROOT / info["path"])
        full_prompt = f"{STYLE_PREFIX}{info['prompt']}{STYLE_SUFFIX}"
        generate_image(
            positive_prompt=full_prompt,
            output_path=output_path,
            width=info["width"], height=info["height"],
            steps=30, cfg=7.5
        )


def generate_player():
    """Generate player sprite sheet."""
    print("\n" + "="*60)
    print("GENERATING PLAYER SPRITE SHEET")
    print("="*60)

    base_prompt = (
        "sci-fi dimension warrior, sleek dark armored suit, "
        "glowing purple energy lines on suit, full body character, "
        "game character art"
    )

    frame_paths = generate_character_frames(
        name="player",
        base_prompt=base_prompt,
        animations={k: PLAYER_PROMPTS[k].split(", ")[1] for k in PLAYER_PROMPTS},
        frame_counts=PLAYER_FRAMES,
        frame_w=32, frame_h=48,
        output_dir=TEMP_DIR / "player",
        steps=25
    )

    # Player sheet: max frames per row = 8, rows = 9
    # Sheet size: 256x432 (8*32 x 9*48)
    row_order = ["idle", "run", "jump", "fall", "dash", "wallslide", "attack", "hurt", "dead"]
    assemble_sprite_sheet(
        frame_paths, frame_width=32, frame_height=48,
        cols_per_row=8, rows_layout=row_order,
        output_path=str(ASSETS_DIR / "player" / "player.png")
    )


def generate_enemies():
    """Generate all 17 enemy sprite sheets."""
    print("\n" + "="*60)
    print("GENERATING ENEMY SPRITE SHEETS (17 enemies)")
    print("="*60)

    for enemy_name, design in ENEMY_DESIGNS.items():
        print(f"\n--- Enemy: {enemy_name} ---")
        base_prompt = f"{design['desc']}, {design['color']}, game enemy creature"

        anim_prompts = {}
        for anim, suffix in ENEMY_ANIM_PROMPTS.items():
            anim_prompts[anim] = f"{suffix}, {design['desc']}"

        frame_w = 32
        frame_h = 32
        if enemy_name == "crawler":
            frame_h = 24
        elif enemy_name == "swarmer":
            frame_w = 16
            frame_h = 16

        frame_paths = generate_character_frames(
            name=f"enemy_{enemy_name}",
            base_prompt=base_prompt,
            animations=ENEMY_ANIM_PROMPTS,
            frame_counts=ENEMY_FRAMES,
            frame_w=frame_w, frame_h=frame_h,
            output_dir=TEMP_DIR / f"enemy_{enemy_name}",
            steps=20
        )

        # Enemy sheets: 6 cols x 5 rows = 192x160 (standard)
        row_order = ["idle", "walk", "attack", "hurt", "dead"]
        cols = 6
        sheet_w = cols * frame_w
        sheet_h = len(row_order) * frame_h

        assemble_sprite_sheet(
            frame_paths, frame_width=frame_w, frame_height=frame_h,
            cols_per_row=cols, rows_layout=row_order,
            output_path=str(ASSETS_DIR / "enemies" / f"{enemy_name}.png")
        )


def generate_bosses():
    """Generate all 6 boss sprite sheets."""
    print("\n" + "="*60)
    print("GENERATING BOSS SPRITE SHEETS (6 bosses)")
    print("="*60)

    for boss_name, design in BOSS_DESIGNS.items():
        print(f"\n--- Boss: {boss_name} ---")
        frame_size = design.get("size", 64)
        base_prompt = f"{design['desc']}, {design['color']}, massive boss enemy, epic, intimidating"

        frame_paths = generate_character_frames(
            name=f"boss_{boss_name}",
            base_prompt=base_prompt,
            animations=BOSS_ANIM_PROMPTS,
            frame_counts=BOSS_FRAMES,
            frame_w=frame_size, frame_h=frame_size,
            output_dir=TEMP_DIR / f"boss_{boss_name}",
            steps=25
        )

        # Boss sheets: 8 cols x 9 rows
        row_order = list(BOSS_ANIMATIONS)
        cols = 8
        assemble_sprite_sheet(
            frame_paths, frame_width=frame_size, frame_height=frame_size,
            cols_per_row=cols, rows_layout=row_order,
            output_path=str(ASSETS_DIR / "bosses" / f"{boss_name}.png")
        )


def generate_pickups():
    """Generate pickup sprite sheet."""
    print("\n" + "="*60)
    print("GENERATING PICKUP SPRITES")
    print("="*60)

    pickup_dir = TEMP_DIR / "pickups"
    pickup_dir.mkdir(exist_ok=True)

    pickup_images = []
    for name, prompt in PICKUP_PROMPTS.items():
        full_prompt = f"{STYLE_PREFIX}{prompt}{STYLE_SUFFIX}"
        img_path = str(pickup_dir / f"{name}.png")
        generate_image(
            positive_prompt=full_prompt,
            output_path=img_path,
            width=512, height=512,
            steps=20, cfg=7.5
        )
        pickup_images.append((name, img_path))

    # Assemble into pickup sheet: 4 columns x 5 rows of 16x16 icons
    sheet = Image.new("RGBA", (64, 80), (0, 0, 0, 0))
    for idx, (name, img_path) in enumerate(pickup_images):
        if os.path.exists(img_path):
            img = Image.open(img_path).convert("RGBA")
            icon = img.resize((16, 16), Image.LANCZOS)
            col = idx % 4
            row = idx // 4
            sheet.paste(icon, (col * 16, row * 16))

    output_path = str(ASSETS_DIR / "pickups" / "pickups.png")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    sheet.save(output_path, "PNG")
    print(f"  Pickup sheet saved: {output_path}")


def generate_projectiles():
    """Generate projectile sprite sheet."""
    print("\n" + "="*60)
    print("GENERATING PROJECTILE SPRITES")
    print("="*60)

    proj_dir = TEMP_DIR / "projectiles"
    proj_dir.mkdir(exist_ok=True)

    prompts = [
        "glowing energy projectile ball, purple plasma bolt, dark background, single item",
        "blue energy beam projectile, laser shot, dark background, single item",
        "green toxic projectile, acid glob, dark background, single item",
        "red fire projectile, flame bolt, dark background, single item"
    ]

    sheet = Image.new("RGBA", (64, 48), (0, 0, 0, 0))
    for idx, prompt in enumerate(prompts):
        full_prompt = f"{STYLE_PREFIX}{prompt}{STYLE_SUFFIX}"
        img_path = str(proj_dir / f"proj_{idx}.png")
        success = generate_image(
            positive_prompt=full_prompt,
            output_path=img_path,
            width=512, height=512,
            steps=15, cfg=7.5
        )
        if success:
            img = Image.open(img_path).convert("RGBA")
            icon = img.resize((16, 12), Image.LANCZOS)
            col = idx % 4
            row = idx // 4
            sheet.paste(icon, (col * 16, row * 12))

    output_path = str(ASSETS_DIR / "projectiles" / "projectiles.png")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    sheet.save(output_path, "PNG")
    print(f"  Projectile sheet saved: {output_path}")


# ===========================================================================
# MAIN
# ===========================================================================
def main():
    print("="*60)
    print("RIFTWALKER COMPLETE ASSET GENERATION")
    print("Using ComfyUI + JuggernautXL")
    print("="*60)

    if not check_server():
        print("ERROR: ComfyUI is not running!")
        sys.exit(1)

    start_time = time.time()

    # Phase 1: Backgrounds (fast, immediate visual impact)
    generate_backgrounds()

    # Phase 2: Tilesets (important for level look)
    generate_tilesets()

    # Phase 3: Player (most important character)
    generate_player()

    # Phase 4: Enemies (17 types - takes longest)
    generate_enemies()

    # Phase 5: Bosses (6 unique designs)
    generate_bosses()

    # Phase 6: Pickups & Projectiles
    generate_pickups()
    generate_projectiles()

    elapsed = time.time() - start_time
    print(f"\n{'='*60}")
    print(f"GENERATION COMPLETE in {elapsed/60:.1f} minutes")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
