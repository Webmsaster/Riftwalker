"""
ComfyUI API client for Riftwalker asset generation.
Generates game sprites/backgrounds via ComfyUI's REST API.
Uses JuggernautXL for high-quality stylized game art.
"""

import json
import uuid
import urllib.request
import urllib.parse
import time
import os
import sys
from pathlib import Path

SERVER = "127.0.0.1:8188"
CLIENT_ID = str(uuid.uuid4())
MODEL = "juggernautXL_v9.safetensors"

# Negative prompt for all generations - game art specific
NEGATIVE_PROMPT = (
    "pixel art, pixelated, 8-bit, retro, low resolution, blurry, "
    "text, watermark, signature, logo, letters, words, "
    "deformed, ugly, bad anatomy, disfigured, mutated, "
    "photo, photograph, realistic photo, 3d render, "
    "multiple views, collage, border, frame, UI elements"
)


def build_workflow(positive_prompt, negative_prompt=NEGATIVE_PROMPT,
                   width=512, height=512, seed=None, steps=25, cfg=7.5,
                   filename_prefix="riftwalker", batch_size=1):
    """Build a ComfyUI workflow JSON for text-to-image generation."""
    if seed is None:
        seed = int.from_bytes(os.urandom(4), 'big')

    workflow = {
        "4": {
            "class_type": "CheckpointLoaderSimple",
            "inputs": {
                "ckpt_name": MODEL
            }
        },
        "5": {
            "class_type": "EmptyLatentImage",
            "inputs": {
                "width": width,
                "height": height,
                "batch_size": batch_size
            }
        },
        "6": {
            "class_type": "CLIPTextEncode",
            "inputs": {
                "clip": ["4", 1],
                "text": positive_prompt
            }
        },
        "7": {
            "class_type": "CLIPTextEncode",
            "inputs": {
                "clip": ["4", 1],
                "text": negative_prompt
            }
        },
        "3": {
            "class_type": "KSampler",
            "inputs": {
                "model": ["4", 0],
                "positive": ["6", 0],
                "negative": ["7", 0],
                "latent_image": ["5", 0],
                "seed": seed,
                "steps": steps,
                "cfg": cfg,
                "sampler_name": "dpmpp_2m",
                "scheduler": "karras",
                "denoise": 1.0
            }
        },
        "8": {
            "class_type": "VAEDecode",
            "inputs": {
                "samples": ["3", 0],
                "vae": ["4", 2]
            }
        },
        "9": {
            "class_type": "SaveImage",
            "inputs": {
                "filename_prefix": filename_prefix,
                "images": ["8", 0]
            }
        }
    }
    return workflow


def queue_prompt(prompt):
    """Queue a prompt and return the prompt_id."""
    prompt_id = str(uuid.uuid4())
    p = {"prompt": prompt, "client_id": CLIENT_ID, "prompt_id": prompt_id}
    data = json.dumps(p).encode('utf-8')
    req = urllib.request.Request(f"http://{SERVER}/prompt", data=data)
    resp = urllib.request.urlopen(req).read()
    return prompt_id


def get_history(prompt_id):
    """Get execution history for a prompt."""
    with urllib.request.urlopen(f"http://{SERVER}/history/{prompt_id}") as response:
        return json.loads(response.read())


def get_image(filename, subfolder, folder_type):
    """Download a generated image from ComfyUI."""
    data = {"filename": filename, "subfolder": subfolder, "type": folder_type}
    url_values = urllib.parse.urlencode(data)
    with urllib.request.urlopen(f"http://{SERVER}/view?{url_values}") as response:
        return response.read()


def wait_for_completion(prompt_id, timeout=600, poll_interval=2):
    """Poll until the prompt is done or timeout."""
    start = time.time()
    while time.time() - start < timeout:
        try:
            history = get_history(prompt_id)
            if prompt_id in history:
                status = history[prompt_id].get("status", {})
                if status.get("completed", False) or "outputs" in history[prompt_id]:
                    return history[prompt_id]
                if status.get("status_str") == "error":
                    print(f"ERROR: Prompt {prompt_id} failed: {status}")
                    return None
        except Exception:
            pass
        time.sleep(poll_interval)
    print(f"TIMEOUT: Prompt {prompt_id} did not complete in {timeout}s")
    return None


def generate_image(positive_prompt, output_path, width=512, height=512,
                   seed=None, steps=25, cfg=7.5, timeout=600):
    """Generate a single image and save to output_path. Returns True on success."""
    print(f"  Generating: {output_path}")
    print(f"  Prompt: {positive_prompt[:80]}...")
    print(f"  Size: {width}x{height}, Steps: {steps}, CFG: {cfg}")

    workflow = build_workflow(
        positive_prompt=positive_prompt,
        width=width, height=height,
        seed=seed, steps=steps, cfg=cfg,
        filename_prefix="riftwalker_gen"
    )

    prompt_id = queue_prompt(workflow)
    print(f"  Queued: {prompt_id}")

    result = wait_for_completion(prompt_id, timeout=timeout)
    if result is None:
        print(f"  FAILED: No result for {prompt_id}")
        return False

    # Extract images from result
    for node_id, node_output in result.get("outputs", {}).items():
        if "images" in node_output:
            for img_info in node_output["images"]:
                img_data = get_image(
                    img_info["filename"],
                    img_info.get("subfolder", ""),
                    img_info.get("type", "output")
                )
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                with open(output_path, "wb") as f:
                    f.write(img_data)
                print(f"  Saved: {output_path} ({len(img_data)} bytes)")
                return True

    print(f"  FAILED: No images in output")
    return False


def generate_batch(jobs, timeout_per_image=600):
    """Generate multiple images sequentially.
    jobs: list of dicts with keys: prompt, output_path, width, height, seed, steps, cfg
    """
    results = []
    total = len(jobs)
    for i, job in enumerate(jobs):
        print(f"\n[{i+1}/{total}] Generating...")
        success = generate_image(
            positive_prompt=job["prompt"],
            output_path=job["output_path"],
            width=job.get("width", 512),
            height=job.get("height", 512),
            seed=job.get("seed"),
            steps=job.get("steps", 25),
            cfg=job.get("cfg", 7.5),
            timeout=timeout_per_image
        )
        results.append({"job": job, "success": success})
        if success:
            print(f"  OK")
        else:
            print(f"  FAILED - will retry later")
    return results


def check_server():
    """Check if ComfyUI is running."""
    try:
        with urllib.request.urlopen(f"http://{SERVER}/system_stats") as resp:
            data = json.loads(resp.read())
            print(f"ComfyUI v{data['system']['comfyui_version']} running")
            for dev in data.get("devices", []):
                print(f"  Device: {dev['name']}, VRAM: {dev['vram_total']//1024//1024}MB")
            return True
    except Exception as e:
        print(f"ComfyUI not available: {e}")
        return False


if __name__ == "__main__":
    if not check_server():
        sys.exit(1)
    # Quick test
    generate_image(
        "dark sci-fi corridor, dimension rift, glowing purple energy, game background, digital painting, detailed",
        "tools/test_gen.png",
        width=512, height=512, steps=20
    )
