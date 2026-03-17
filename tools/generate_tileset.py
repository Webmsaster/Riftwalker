"""
Generate a proper tileset for Riftwalker with visual variety.
The tileset is 16x6 grid of 32x32 tiles (512x192).

Row 0: Solid tiles (auto-tile variants 0-15 based on neighbor mask)
Row 1: Hazards (spikes, fire, conveyor, laser)
Row 2: Special (one-way, ice, crumbling, gravity well)
Row 3: Interactables (rift, exit, dim-switch, dim-gate, teleporter)
Row 4: Decorations (background details)
Row 5: Extra
"""

import json
import urllib.request
import time
import uuid
import random
import os
from PIL import Image, ImageDraw, ImageFilter

COMFYUI_URL = "http://127.0.0.1:8188"
OUTPUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      "assets", "textures", "tiles", "tileset_universal.png")

TILE = 32

def queue_and_wait(prompt_text, w, h, seed=None, steps=28):
    """Generate an image via ComfyUI and return PIL Image."""
    if seed is None:
        seed = random.randint(0, 2**32-1)

    workflow = {
        "1": {"class_type": "CheckpointLoaderSimple",
              "inputs": {"ckpt_name": "juggernautXL_v9.safetensors"}},
        "2": {"class_type": "LoraLoader",
              "inputs": {"lora_name": "pixel-art-xl.safetensors",
                         "strength_model": 0.9, "strength_clip": 0.9,
                         "model": ["1", 0], "clip": ["1", 1]}},
        "3": {"class_type": "CLIPTextEncode",
              "inputs": {"text": f"pixel art, {prompt_text}, game tileset asset, 16-bit retro style, clean crisp pixels, vibrant colors, sharp edges, top-down view",
                         "clip": ["2", 1]}},
        "4": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "blurry, 3d, photo, realistic, smooth, watermark, text, character, person, face, animal",
                         "clip": ["2", 1]}},
        "5": {"class_type": "EmptyLatentImage",
              "inputs": {"width": w, "height": h, "batch_size": 1}},
        "6": {"class_type": "KSampler",
              "inputs": {"model": ["2", 0], "positive": ["3", 0],
                         "negative": ["4", 0], "latent_image": ["5", 0],
                         "seed": seed, "steps": steps, "cfg": 7.5,
                         "sampler_name": "euler_ancestral", "scheduler": "normal",
                         "denoise": 1.0}},
        "7": {"class_type": "VAEDecode",
              "inputs": {"samples": ["6", 0], "vae": ["1", 2]}},
        "8": {"class_type": "SaveImage",
              "inputs": {"images": ["7", 0], "filename_prefix": "riftwalker_tile"}}
    }

    data = json.dumps({"prompt": workflow, "client_id": str(uuid.uuid4())}).encode("utf-8")
    req = urllib.request.Request(f"{COMFYUI_URL}/prompt", data=data,
                                 headers={"Content-Type": "application/json"})
    resp = urllib.request.urlopen(req, timeout=30)
    prompt_id = json.loads(resp.read())["prompt_id"]

    # Wait for completion
    for _ in range(200):
        time.sleep(3)
        try:
            resp = urllib.request.urlopen(f"{COMFYUI_URL}/history/{prompt_id}", timeout=10)
            history = json.loads(resp.read())
            if prompt_id in history:
                outputs = history[prompt_id].get("outputs", {})
                for node_out in outputs.values():
                    images = node_out.get("images", [])
                    if images:
                        img_info = images[0]
                        url = f"{COMFYUI_URL}/view?filename={img_info['filename']}&subfolder={img_info.get('subfolder','')}&type=output"
                        resp = urllib.request.urlopen(url, timeout=30)
                        import io
                        return Image.open(io.BytesIO(resp.read())).convert("RGBA")
        except Exception:
            pass
    return None


def make_solid_tiles(sheet, draw):
    """Row 0: 16 auto-tile solid variants with visible edges and texture."""
    print("  Generating solid tiles...")

    # Generate a textured stone surface via ComfyUI
    stone_img = queue_and_wait(
        "stone brick wall texture, dungeon floor tiles, dark grey-brown, mossy cracks, medieval, seamless pattern",
        512, 512)

    if stone_img:
        stone_img = stone_img.resize((TILE * 4, TILE * 4), Image.NEAREST)

    for i in range(16):
        x, y = i * TILE, 0
        # Base tile from generated texture
        if stone_img:
            sx = (i % 4) * TILE
            sy = (i // 4) * TILE
            tile = stone_img.crop((sx, sy, sx + TILE, sy + TILE))
        else:
            tile = Image.new("RGBA", (TILE, TILE), (80, 70, 60, 255))

        # Draw edge highlights based on auto-tile mask
        tile_draw = ImageDraw.Draw(tile)
        has_top = not (i & 1)
        has_right = not (i & 2)
        has_bottom = not (i & 4)
        has_left = not (i & 8)

        edge_light = (180, 170, 150, 200)
        edge_dark = (30, 25, 20, 200)

        if has_top:
            tile_draw.line([(0, 0), (TILE-1, 0)], fill=edge_light, width=2)
        if has_bottom:
            tile_draw.line([(0, TILE-1), (TILE-1, TILE-1)], fill=edge_dark, width=2)
        if has_left:
            tile_draw.line([(0, 0), (0, TILE-1)], fill=edge_light, width=1)
        if has_right:
            tile_draw.line([(TILE-1, 0), (TILE-1, TILE-1)], fill=edge_dark, width=1)

        sheet.paste(tile, (x, y))


def make_hazard_tiles(sheet, draw):
    """Row 1: Spike, Fire, Conveyor, Laser hazard tiles."""
    print("  Generating hazard tiles...")

    hazards = [
        ("metallic spikes, sharp pointed metal traps, dangerous, silver grey", 4),
        ("fire flames, burning orange-red fire effect, hot lava glow", 4),
        ("conveyor belt, industrial metal moving belt with arrows, mechanical", 4),
        ("laser beam, red energy beam, sci-fi warning light, danger", 4),
    ]

    col = 0
    for prompt, count in hazards:
        img = queue_and_wait(prompt, 512, 512)
        if img:
            img = img.resize((TILE * count, TILE), Image.NEAREST)
            for c in range(count):
                tile = img.crop((c * TILE, 0, (c+1) * TILE, TILE))
                sheet.paste(tile, (col * TILE, TILE))
                col += 1
        else:
            col += count


def make_special_tiles(sheet, draw):
    """Row 2: One-way, Ice, Crumbling, Gravity well."""
    print("  Generating special tiles...")

    specials = [
        ("thin platform, one-way wooden bridge, pixel art, simple", 4),
        ("ice crystal tile, frozen blue surface, slippery, glowing", 4),
        ("crumbling stone, cracked breaking rock, damaged, debris", 4),
        ("gravity well, swirling purple vortex, energy field, cosmic", 4),
    ]

    col = 0
    for prompt, count in specials:
        img = queue_and_wait(prompt, 512, 512)
        if img:
            img = img.resize((TILE * count, TILE), Image.NEAREST)
            for c in range(count):
                tile = img.crop((c * TILE, 0, (c+1) * TILE, TILE))
                sheet.paste(tile, (col * TILE, 2 * TILE))
                col += 1
        else:
            col += count


def make_interactable_tiles(sheet, draw):
    """Row 3: Rift, Exit, DimSwitch, DimGate, Teleporter."""
    print("  Generating interactable tiles...")

    interactables = [
        ("dimensional rift portal, purple swirling energy tear in space, glowing", 3),
        ("exit door, green glowing escape portal, safe haven", 3),
        ("pressure plate switch, diamond shaped button on floor, mechanical", 2),
        ("energy barrier gate, red force field wall, blocking", 2),
        ("teleporter pad, blue sci-fi teleport circle, glowing runes", 2),
        ("collectible crystal shard, glowing gem pickup, valuable", 2),
    ]

    col = 0
    for prompt, count in interactables:
        img = queue_and_wait(prompt, 512, 512)
        if img:
            sz = min(count, 4)
            img = img.resize((TILE * sz, TILE), Image.NEAREST)
            for c in range(min(count, sz)):
                tile = img.crop((c * TILE, 0, (c+1) * TILE, TILE))
                sheet.paste(tile, (col * TILE, 3 * TILE))
                col += 1
        else:
            col += count
        if col >= 16:
            break


def make_decoration_tiles(sheet, draw):
    """Row 4-5: Background decorations and extras."""
    print("  Generating decoration tiles...")

    img = queue_and_wait(
        "dungeon decoration tiles, chains, torches, broken pillars, moss, cobwebs, skulls, pixel art tileset, dark fantasy",
        512, 512)

    if img:
        # Fill rows 4-5 with decoration tiles
        img = img.resize((TILE * 16, TILE * 2), Image.NEAREST)
        for row in range(2):
            for col in range(16):
                tile = img.crop((col * TILE, row * TILE, (col+1) * TILE, (row+1) * TILE))
                sheet.paste(tile, (col * TILE, (4 + row) * TILE))


def main():
    try:
        urllib.request.urlopen(f"{COMFYUI_URL}/system_stats", timeout=5)
    except Exception:
        print("ERROR: ComfyUI not running")
        return

    print("Generating tileset...")

    # Create blank sheet
    sheet = Image.new("RGBA", (16 * TILE, 6 * TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)

    make_solid_tiles(sheet, draw)
    make_hazard_tiles(sheet, draw)
    make_special_tiles(sheet, draw)
    make_interactable_tiles(sheet, draw)
    make_decoration_tiles(sheet, draw)

    sheet.save(OUTPUT)
    print(f"Tileset saved: {OUTPUT} ({sheet.size[0]}x{sheet.size[1]})")


if __name__ == "__main__":
    main()
