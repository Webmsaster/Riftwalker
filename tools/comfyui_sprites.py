#!/usr/bin/env python3
"""
Generate pixel art concept sprites via ComfyUI API.
Uses JuggernautXL v9 + pixel-art-xl LoRA for consistent pixel art style.
Generates character concept images for all entity types.
"""
import json
import sys
import time
import uuid
from pathlib import Path
from urllib import parse, request

COMFY_URL = "http://127.0.0.1:8188"
CLIENT_ID = str(uuid.uuid4())
OUTPUT_DIR = Path(__file__).parent.parent / "assets" / "comfyui_concepts"

# Workflow template: CheckpointLoader -> LoRA -> CLIP+ -> CLIP- -> Sampler -> VAE -> Save
WORKFLOW_TEMPLATE = {
    "1": {
        "inputs": {"ckpt_name": "juggernautXL_v9.safetensors"},
        "class_type": "CheckpointLoaderSimple"
    },
    "2": {
        "inputs": {
            "lora_name": "pixel-art-xl.safetensors",
            "strength_model": 0.85,
            "strength_clip": 0.85,
            "model": ["1", 0],
            "clip": ["1", 1]
        },
        "class_type": "LoraLoader"
    },
    "3": {
        "inputs": {"text": "", "clip": ["2", 1]},
        "class_type": "CLIPTextEncode"
    },
    "4": {
        "inputs": {"text": "", "clip": ["2", 1]},
        "class_type": "CLIPTextEncode"
    },
    "5": {
        "inputs": {"width": 512, "height": 512, "batch_size": 1},
        "class_type": "EmptyLatentImage"
    },
    "6": {
        "inputs": {
            "seed": 42,
            "steps": 20,
            "cfg": 7.0,
            "sampler_name": "dpmpp_2m",
            "scheduler": "karras",
            "denoise": 1.0,
            "model": ["2", 0],
            "positive": ["3", 0],
            "negative": ["4", 0],
            "latent_image": ["5", 0]
        },
        "class_type": "KSampler"
    },
    "7": {
        "inputs": {"samples": ["6", 0], "vae": ["1", 2]},
        "class_type": "VAEDecode"
    },
    "8": {
        "inputs": {"filename_prefix": "riftwalker/concept", "images": ["7", 0]},
        "class_type": "SaveImage"
    }
}

NEGATIVE_PROMPT = (
    "blurry, low quality, text, watermark, signature, deformed, ugly, "
    "3d render, photograph, realistic, multiple characters, busy background, "
    "jpeg artifacts, cropped"
)

# Entity definitions: (name, prompt, seed, size)
ENTITIES = [
    # Player
    ("player", "pixel art game character, sci-fi space suit, blue armor, glowing cyan visor, side view, full body, action pose, dark background, 16-bit style, clean pixel art, game sprite", 100, 512),

    # Standard enemies
    ("walker", "pixel art robot enemy, red mechanical body, single glowing yellow eye, boxy design, two legs, side view, full body, dark background, 16-bit game sprite", 200, 512),
    ("flyer", "pixel art flying bat creature, purple wings, small round body, glowing pink eyes, hovering, side view, dark background, 16-bit game sprite", 201, 512),
    ("turret", "pixel art stationary turret, yellow-grey metal, mounted gun barrel, red targeting eye, mechanical, side view, dark background, 16-bit game sprite", 202, 512),
    ("charger", "pixel art charging beast, orange armored bull-like creature, horn, aggressive stance, side view, full body, dark background, 16-bit game sprite", 203, 512),
    ("phaser", "pixel art ghost enemy, deep purple ethereal being, tall and slim, glowing purple eyes, partially transparent, side view, dark background, 16-bit game sprite", 204, 512),
    ("exploder", "pixel art explosive enemy, orange-red round body, glowing yellow core, fuse on top, unstable appearance, side view, dark background, 16-bit game sprite", 205, 512),
    ("shielder", "pixel art shielded enemy, cyan armored warrior, energy shield, bulky protective stance, glowing blue, side view, dark background, 16-bit game sprite", 206, 512),
    ("crawler", "pixel art crawler bug, green insect creature, low flat body, multiple legs, red eyes, side view, dark background, 16-bit game sprite", 207, 512),
    ("summoner", "pixel art summoner mage, magenta robed figure, floating orb, magical aura, staff, side view, full body, dark background, 16-bit game sprite", 208, 512),
    ("sniper", "pixel art sniper robot, grey-yellow slim body, long rifle, red scope eye, precise stance, side view, dark background, 16-bit game sprite", 209, 512),

    # New enemies
    ("teleporter", "pixel art teleporter enemy, blue glowing entity, portal rings around body, ethereal warp effects, cyan eyes, side view, dark background, 16-bit game sprite", 210, 512),
    ("reflector", "pixel art mirror shield enemy, silver chrome armor, large reflective shield, yellow eyes, defensive stance, side view, dark background, 16-bit game sprite", 211, 512),
    ("leech", "pixel art leech worm creature, dark red segmented body, open mouth with fangs, parasitic, side view, dark background, 16-bit game sprite", 212, 512),
    ("swarmer", "pixel art tiny insect swarm creature, yellow-golden small bug, tiny wings buzzing, stinger tail, side view, dark background, 16-bit game sprite", 213, 512),
    ("gravitywell", "pixel art gravity orb enemy, dark purple pulsating sphere, gravity distortion rings, glowing core, floating, dark background, 16-bit game sprite", 214, 512),
    ("mimic", "pixel art mimic treasure chest, wooden chest with hidden teeth and red eyes, partially open lid revealing fangs, side view, dark background, 16-bit game sprite", 215, 512),

    # Bosses
    ("boss_rift_guardian", "pixel art boss monster, large violet crystal guardian, imposing armored body, glowing red eyes, energy core chest, massive, side view, dark background, 16-bit game sprite", 300, 768),
    ("boss_void_wyrm", "pixel art boss dragon, large teal serpent wyrm, venomous green dripping fangs, elongated body, yellow eyes, massive, side view, dark background, 16-bit game sprite", 301, 768),
    ("boss_dimensional_architect", "pixel art boss mage, large blue-purple architect entity, floating geometric shapes, rift portals, cyan eyes, massive, side view, dark background, 16-bit game sprite", 302, 768),
    ("boss_temporal_weaver", "pixel art boss time wizard, large golden clockwork spider, clock gears and hands, temporal distortion, blue eyes, massive, side view, dark background, 16-bit game sprite", 303, 768),
    ("boss_void_sovereign", "pixel art final boss, massive dark purple emperor, void energy crown, red glowing eyes, commanding pose, enormous, side view, dark background, 16-bit game sprite", 304, 768),
    ("boss_entropy_incarnate", "pixel art chaos boss, large crimson entity of pure entropy, fragmented form, reality-breaking effects, unstable energy, massive, side view, dark background, 16-bit game sprite", 305, 768),
]


def submit_job(prompt_text: str, negative_text: str, seed: int, size: int, prefix: str) -> str:
    """Submit a generation job to ComfyUI API."""
    import copy
    workflow = copy.deepcopy(WORKFLOW_TEMPLATE)
    workflow["3"]["inputs"]["text"] = prompt_text
    workflow["4"]["inputs"]["text"] = negative_text
    workflow["5"]["inputs"]["width"] = size
    workflow["5"]["inputs"]["height"] = size
    workflow["6"]["inputs"]["seed"] = seed
    workflow["8"]["inputs"]["filename_prefix"] = f"riftwalker/{prefix}"

    payload = json.dumps({"prompt": workflow, "client_id": CLIENT_ID}).encode()
    req = request.Request(
        f"{COMFY_URL}/prompt",
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    with request.urlopen(req, timeout=10) as resp:
        result = json.loads(resp.read())
    return result["prompt_id"]


def wait_for_job(prompt_id: str, timeout: int = 300) -> dict:
    """Poll ComfyUI history until job completes."""
    url = f"{COMFY_URL}/history/{parse.quote(prompt_id)}"
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            req = request.Request(url, method="GET")
            with request.urlopen(req, timeout=5) as resp:
                history = json.loads(resp.read())
            entry = history.get(prompt_id)
            if entry and entry.get("outputs"):
                return entry
            if entry:
                status = entry.get("status", {})
                if isinstance(status, dict) and status.get("status_str") == "error":
                    raise RuntimeError(f"Job failed: {status}")
        except Exception as e:
            if "failed" in str(e).lower():
                raise
        time.sleep(2)
    raise TimeoutError(f"Job {prompt_id} timed out after {timeout}s")


def download_image(image_info: dict, target: Path) -> None:
    """Download generated image from ComfyUI."""
    query = parse.urlencode({
        "filename": image_info.get("filename", ""),
        "subfolder": image_info.get("subfolder", ""),
        "type": image_info.get("type", "output"),
    })
    req = request.Request(f"{COMFY_URL}/view?{query}", method="GET")
    with request.urlopen(req, timeout=30) as resp:
        data = resp.read()
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Check ComfyUI is running
    try:
        req = request.Request(f"{COMFY_URL}/system_stats", method="GET")
        with request.urlopen(req, timeout=5) as resp:
            stats = json.loads(resp.read())
        print(f"ComfyUI online: {stats['system']['comfyui_version']}")
    except Exception as e:
        print(f"ERROR: ComfyUI not reachable: {e}")
        sys.exit(1)

    total = len(ENTITIES)
    successes = 0
    failures = 0

    for i, (name, prompt, seed, size) in enumerate(ENTITIES, 1):
        target = OUTPUT_DIR / f"{name}.png"
        print(f"[{i}/{total}] Generating {name} ({size}x{size})...", end=" ", flush=True)
        try:
            prompt_id = submit_job(prompt, NEGATIVE_PROMPT, seed, size, name)
            entry = wait_for_job(prompt_id, timeout=600)
            # Find image in outputs
            for node_out in entry.get("outputs", {}).values():
                images = node_out.get("images", [])
                if images:
                    download_image(images[0], target)
                    print(f"OK -> {target}")
                    successes += 1
                    break
            else:
                print("WARN: no image output")
                failures += 1
        except Exception as e:
            print(f"FAILED: {e}")
            failures += 1

    print(f"\nDone: {successes} succeeded, {failures} failed out of {total}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
