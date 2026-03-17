"""
Riftwalker Sprite Generator via ComfyUI API
Generates all game sprites using SDXL + Pixel Art LoRA
"""

import json
import urllib.request
import urllib.error
import time
import uuid
import os
import sys
import random
from PIL import Image, ImageFilter, ImageEnhance

COMFYUI_URL = "http://127.0.0.1:8188"
OUTPUT_BASE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "textures")

# Sprite sheet specifications
# (folder, filename, frame_w, frame_h, cols, rows, description, prompt_detail)
SPRITE_SPECS = {
    "player": ("player", "player.png", 32, 48, 8, 9,
        "Player character",
        "single pixel art character, blue armored sci-fi void walker, glowing blue visor, dark suit with blue energy lines, front-facing, centered, full body"),

    "walker": ("enemies", "walker.png", 32, 32, 6, 5,
        "Walker enemy",
        "single pixel art enemy creature, red hostile robot, menacing red eyes, angular metallic body, front-facing, centered, full body"),
    "flyer": ("enemies", "flyer.png", 32, 32, 6, 5,
        "Flyer enemy",
        "single pixel art enemy creature, yellow flying bat-winged creature, glowing wings, hovering, front-facing, centered"),
    "charger": ("enemies", "charger.png", 32, 32, 6, 5,
        "Charger enemy",
        "single pixel art enemy creature, orange armored charging bull beast, horned head, muscular, front-facing, centered"),
    "turret": ("enemies", "turret.png", 32, 32, 6, 5,
        "Turret enemy",
        "single pixel art enemy, grey mechanical defense turret, barrel cannon, metallic, stationary, centered"),
    "phaser": ("enemies", "phaser.png", 32, 32, 6, 5,
        "Phaser enemy",
        "single pixel art enemy creature, purple ghostly ethereal entity, translucent glowing body, mystic, centered"),
    "exploder": ("enemies", "exploder.png", 32, 32, 6, 5,
        "Exploder enemy",
        "single pixel art enemy creature, orange-red volatile bomb creature, glowing unstable core, about to explode, centered"),
    "shielder": ("enemies", "shielder.png", 32, 32, 6, 5,
        "Shielder enemy",
        "single pixel art enemy creature, cyan armored shield bearer, energy barrier in front, bulky defensive, centered"),
    "crawler": ("enemies", "crawler.png", 32, 32, 6, 5,
        "Crawler enemy",
        "single pixel art enemy creature, green spider-like crawler, multiple legs, low profile insect, centered"),
    "summoner": ("enemies", "summoner.png", 32, 32, 6, 5,
        "Summoner enemy",
        "single pixel art enemy creature, magenta dark robed mage, staff with dark magic aura, summoning pose, centered"),
    "sniper": ("enemies", "sniper.png", 32, 32, 6, 5,
        "Sniper enemy",
        "single pixel art enemy creature, dark blue hooded sniper, long rifle, crouching aiming pose, centered"),
    "minion": ("enemies", "minion.png", 32, 32, 6, 5,
        "Minion enemy",
        "single pixel art enemy creature, small red imp with tiny horns, aggressive little demon, centered"),

    "rift_guardian": ("bosses", "rift_guardian.png", 64, 64, 8, 9,
        "Rift Guardian boss",
        "single pixel art boss monster, large violet dimensional guardian, glowing purple heavy armor, rift energy swirling, imposing, centered"),
    "void_wyrm": ("bosses", "void_wyrm.png", 64, 64, 8, 9,
        "Void Wyrm boss",
        "single pixel art boss monster, large teal void serpent dragon, poisonous scales, menacing head with fangs, centered"),
    "dimensional_architect": ("bosses", "dimensional_architect.png", 64, 64, 8, 9,
        "Dimensional Architect boss",
        "single pixel art boss monster, large blue mechanical construct, geometric floating shapes, analytical robotic, centered"),
    "temporal_weaver": ("bosses", "temporal_weaver.png", 64, 64, 8, 9,
        "Temporal Weaver boss",
        "single pixel art boss monster, large golden clockwork entity, gears and clock hands, time distortion, centered"),
    "void_sovereign": ("bosses", "void_sovereign.png", 96, 96, 8, 9,
        "Void Sovereign boss",
        "single pixel art boss monster, massive dark purple void sovereign, cosmic horror, tentacles of void energy, crown of darkness, centered"),
    "entropy_incarnate": ("bosses", "entropy_incarnate.png", 64, 64, 8, 9,
        "Entropy Incarnate boss",
        "single pixel art boss monster, large crimson chaos entity, fracturing body, entropy decay red-black energy aura, centered"),
}


def remove_background(img, threshold=30):
    """Remove background by making near-corner-color pixels transparent."""
    img = img.convert("RGBA")
    pixels = img.load()
    w, h = img.size

    # Sample corners to determine background color
    corners = [pixels[0,0], pixels[w-1,0], pixels[0,h-1], pixels[w-1,h-1]]
    # Use the most common corner color
    bg = max(set(corners), key=corners.count)

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            # Distance from background color
            dist = abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2])
            if dist < threshold:
                pixels[x, y] = (0, 0, 0, 0)
            elif dist < threshold * 2:
                # Semi-transparent edge
                alpha = int(255 * (dist - threshold) / threshold)
                pixels[x, y] = (r, g, b, min(a, alpha))

    return img


def create_animation_frames(base_frame, num_frames, anim_type="idle"):
    """Create slight variations of a base frame for animation."""
    frames = []
    w, h = base_frame.size

    for i in range(num_frames):
        frame = base_frame.copy()
        t = i / max(1, num_frames - 1)  # 0.0 to 1.0

        if anim_type == "idle":
            # Subtle breathing: slight vertical squash/stretch
            scale = 1.0 + 0.02 * (1.0 if i % 2 == 0 else -1.0) * ((i % 4) / 3.0)
            new_h = int(h * scale)
            frame = frame.resize((w, new_h), Image.NEAREST)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            offset_y = (h - new_h) // 2
            result.paste(frame, (0, max(0, offset_y)))
            frame = result

        elif anim_type == "walk" or anim_type == "run":
            # Bob up and down + slight lean
            offset_y = int(2 * (1 if i % 2 == 0 else -1))
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (0, offset_y))
            frame = result

        elif anim_type == "attack":
            # Forward lean/lunge
            offset_x = int(3 * t * (1 if i < num_frames//2 else -1))
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (offset_x, 0))
            frame = result

        elif anim_type == "hurt":
            # Flash brighter + recoil
            enhancer = ImageEnhance.Brightness(frame)
            frame = enhancer.enhance(1.3 if i == 0 else 1.0)
            offset_x = int(-2 * (1 - t))
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (offset_x, 0))
            frame = result

        elif anim_type == "dead":
            # Fade out + sink down
            offset_y = int(4 * t)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (0, offset_y))
            # Reduce alpha
            pixels = result.load()
            alpha_mult = 1.0 - 0.5 * t
            for py in range(h):
                for px in range(w):
                    r, g, b, a = pixels[px, py]
                    pixels[px, py] = (r, g, b, int(a * alpha_mult))
            frame = result

        elif anim_type == "jump":
            # Stretch vertically
            scale_y = 1.0 + 0.05 * (1.0 - t)
            new_h = int(h * scale_y)
            frame = frame.resize((w, new_h), Image.NEAREST)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (0, h - new_h))
            frame = result

        elif anim_type == "fall":
            # Squash slightly
            scale_y = 1.0 - 0.03 * t
            scale_x = 1.0 + 0.02 * t
            new_w = int(w * scale_x)
            new_h = int(h * scale_y)
            frame = frame.resize((new_w, new_h), Image.NEAREST)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, ((w - new_w) // 2, h - new_h))
            frame = result

        elif anim_type == "dash":
            # Horizontal stretch
            scale_x = 1.0 + 0.1 * (1.0 - abs(2*t - 1))
            new_w = int(w * scale_x)
            frame = frame.resize((new_w, h), Image.NEAREST)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, ((w - new_w) // 2, 0))
            frame = result

        elif anim_type == "phase_transition":
            # Pulsing glow effect
            enhancer = ImageEnhance.Brightness(frame)
            brightness = 1.0 + 0.4 * abs(2*t - 1)
            frame = enhancer.enhance(brightness)

        elif anim_type == "enrage":
            # Shake + brighten
            offset_x = int(2 * (1 if i % 2 == 0 else -1))
            enhancer = ImageEnhance.Brightness(frame)
            frame = enhancer.enhance(1.2)
            result = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            result.paste(frame, (offset_x, 0))
            frame = result

        frames.append(frame)

    return frames


# Animation row definitions per entity type
PLAYER_ANIMS = [
    ("idle", 6), ("run", 8), ("jump", 3), ("fall", 3),
    ("dash", 4), ("idle", 3),  # wallslide = idle variant
    ("attack", 6), ("hurt", 3), ("dead", 5)
]

ENEMY_ANIMS = [
    ("idle", 4), ("walk", 6), ("attack", 4), ("hurt", 2), ("dead", 4)
]

BOSS_ANIMS = [
    ("idle", 6), ("walk", 6), ("attack", 6), ("attack", 6), ("attack", 6),
    ("phase_transition", 8), ("hurt", 3), ("enrage", 6), ("dead", 8)
]


def create_sprite_sheet(single_frame_path, output_path, frame_w, frame_h, cols, rows, entity_type):
    """Create animated sprite sheet from a single generated character image."""
    try:
        img = Image.open(single_frame_path).convert("RGBA")

        # Remove background
        img = remove_background(img, threshold=40)

        # Resize to single frame
        frame = img.resize((frame_w, frame_h), Image.NEAREST)

        # Determine animation rows
        if entity_type == "player":
            anims = PLAYER_ANIMS
        elif entity_type.startswith("boss_"):
            anims = BOSS_ANIMS
        else:
            anims = ENEMY_ANIMS

        sheet_w = frame_w * cols
        sheet_h = frame_h * rows
        sheet = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

        for row_idx in range(min(rows, len(anims))):
            anim_type, num_frames = anims[row_idx]
            frames = create_animation_frames(frame, num_frames, anim_type)

            for col_idx, anim_frame in enumerate(frames):
                if col_idx >= cols:
                    break
                x = col_idx * frame_w
                y = row_idx * frame_h
                sheet.paste(anim_frame, (x, y))

        sheet.save(output_path)
        print(f"  Sheet: {sheet_w}x{sheet_h} ({cols}x{rows} @ {frame_w}x{frame_h}) with animations")
        return True
    except Exception as e:
        print(f"  ERROR creating sheet: {e}")
        import traceback; traceback.print_exc()
        return False


def build_workflow(prompt_text, width, height, seed=None, lora_strength=0.85):
    """Build a ComfyUI workflow for pixel art sprite generation."""
    if seed is None:
        seed = random.randint(0, 2**32 - 1)

    workflow = {
        "1": {
            "class_type": "CheckpointLoaderSimple",
            "inputs": {"ckpt_name": "juggernautXL_v9.safetensors"}
        },
        "2": {
            "class_type": "LoraLoader",
            "inputs": {
                "lora_name": "pixel-art-xl.safetensors",
                "strength_model": lora_strength,
                "strength_clip": lora_strength,
                "model": ["1", 0], "clip": ["1", 1]
            }
        },
        "3": {
            "class_type": "CLIPTextEncode",
            "inputs": {
                "text": f"pixel art, {prompt_text}, game sprite asset, solid color background, 16-bit retro pixel art style, clean crisp pixels, vibrant saturated colors, no anti-aliasing, sharp edges, black background",
                "clip": ["2", 1]
            }
        },
        "4": {
            "class_type": "CLIPTextEncode",
            "inputs": {
                "text": "blurry, 3d render, photo, realistic, smooth gradients, watermark, text, ui, multiple characters, collage, border, frame, white background, grey background, landscape, scene, environment",
                "clip": ["2", 1]
            }
        },
        "5": {
            "class_type": "EmptyLatentImage",
            "inputs": {"width": width, "height": height, "batch_size": 1}
        },
        "6": {
            "class_type": "KSampler",
            "inputs": {
                "model": ["2", 0], "positive": ["3", 0],
                "negative": ["4", 0], "latent_image": ["5", 0],
                "seed": seed, "steps": 28, "cfg": 7.5,
                "sampler_name": "euler_ancestral", "scheduler": "normal",
                "denoise": 1.0
            }
        },
        "7": {
            "class_type": "VAEDecode",
            "inputs": {"samples": ["6", 0], "vae": ["1", 2]}
        },
        "8": {
            "class_type": "SaveImage",
            "inputs": {"images": ["7", 0], "filename_prefix": "riftwalker_sprite"}
        }
    }
    return workflow


def queue_prompt(workflow):
    data = json.dumps({"prompt": workflow, "client_id": str(uuid.uuid4())}).encode("utf-8")
    req = urllib.request.Request(f"{COMFYUI_URL}/prompt", data=data,
                                 headers={"Content-Type": "application/json"})
    try:
        resp = urllib.request.urlopen(req, timeout=30)
        return json.loads(resp.read()).get("prompt_id")
    except Exception as e:
        print(f"  ERROR queuing: {e}")
        return None


def wait_for_completion(prompt_id, timeout=600):
    start = time.time()
    while time.time() - start < timeout:
        try:
            resp = urllib.request.urlopen(f"{COMFYUI_URL}/history/{prompt_id}", timeout=10)
            history = json.loads(resp.read())
            if prompt_id in history:
                outputs = history[prompt_id].get("outputs", {})
                for node_id, node_out in outputs.items():
                    images = node_out.get("images", [])
                    if images:
                        return images
                status = history[prompt_id].get("status", {})
                if status.get("completed", False) or status.get("status_str") == "error":
                    msgs = status.get("messages", [])
                    for msg in msgs:
                        if isinstance(msg, list) and len(msg) > 1 and isinstance(msg[1], dict):
                            if "exception_message" in msg[1]:
                                print(f"  ERROR: {msg[1]['exception_message']}")
                    return None
        except Exception:
            pass
        time.sleep(3)
    print(f"  TIMEOUT")
    return None


def download_image(filename, subfolder, output_path):
    url = f"{COMFYUI_URL}/view?filename={filename}&subfolder={subfolder}&type=output"
    try:
        resp = urllib.request.urlopen(url, timeout=30)
        with open(output_path, "wb") as f:
            f.write(resp.read())
        return True
    except Exception as e:
        print(f"  ERROR downloading: {e}")
        return False


def generate_sprite(name, spec):
    folder, filename, frame_w, frame_h, cols, rows, desc, prompt_detail = spec
    output_dir = os.path.join(OUTPUT_BASE, folder)
    output_path = os.path.join(output_dir, filename)

    # Determine entity type for animation
    if name == "player":
        entity_type = "player"
    elif folder == "bosses":
        entity_type = "boss_" + name
    else:
        entity_type = "enemy_" + name

    print(f"\n{'='*60}")
    print(f"[{name}] {desc} -> {folder}/{filename}")

    # Generate at 512x512 (SDXL sweet spot)
    gen_w, gen_h = 512, 512

    workflow = build_workflow(prompt_detail, gen_w, gen_h)
    prompt_id = queue_prompt(workflow)
    if not prompt_id:
        return False

    print(f"  Queued, generating at {gen_w}x{gen_h}...")
    images = wait_for_completion(prompt_id)
    if not images:
        print(f"  FAILED")
        return False

    temp_path = os.path.join(output_dir, f"_temp_{name}.png")
    if not download_image(images[0]["filename"], images[0].get("subfolder", ""), temp_path):
        return False

    print(f"  Downloaded, creating sprite sheet...")
    success = create_sprite_sheet(temp_path, output_path, frame_w, frame_h, cols, rows, entity_type)

    if os.path.exists(temp_path):
        os.remove(temp_path)

    return success


def main():
    try:
        urllib.request.urlopen(f"{COMFYUI_URL}/system_stats", timeout=5)
    except Exception:
        print("ERROR: ComfyUI not running at", COMFYUI_URL)
        sys.exit(1)

    print("ComfyUI connected!")

    targets = sys.argv[1:] if len(sys.argv) > 1 else list(SPRITE_SPECS.keys())

    results = {}
    for name in targets:
        if name not in SPRITE_SPECS:
            print(f"Unknown: {name}")
            continue
        success = generate_sprite(name, SPRITE_SPECS[name])
        results[name] = success
        if success:
            print(f"  OK!")

    print(f"\n{'='*60}")
    ok = sum(1 for v in results.values() if v)
    print(f"Done: {ok}/{len(results)} sprites generated")
    failed = [k for k,v in results.items() if not v]
    if failed:
        print(f"Failed: {', '.join(failed)}")


if __name__ == "__main__":
    main()
