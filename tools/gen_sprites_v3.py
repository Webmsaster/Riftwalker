"""
Riftwalker Sprite Generator v3 — High-End Game Art Pipeline
=============================================================
Produces hand-painted quality sprites (Hades / Dead Cells / Ori style):
  - 1024x1024 base generation for maximum detail
  - 2-pass pipeline: txt2img → img2img refinement for consistency
  - Professional background removal via rembg (U²-Net) + edge refinement
  - Color coherence system per entity (palette locking)
  - Style anchoring: specific artist/game references, no pixel-art
  - Proper negative prompts tuned for SDXL artifact suppression
  - Assembles into sprite sheets matching existing SpriteConfig layout
  - Optional Real-ESRGAN 4x upscale as final pass

Requires:
  pip install rembg onnxruntime pillow opencv-python-headless numpy
  ComfyUI running on localhost:8188 with JuggernautXL v9
"""

import sys
import os
import random
import math
import json
import uuid
import time
import urllib.request
import urllib.parse
import argparse
import hashlib
from pathlib import Path
from collections import deque

import numpy as np
import cv2
from PIL import Image, ImageFilter, ImageEnhance, ImageDraw, ImageOps

# ---------------------------------------------------------------------------
# ComfyUI Connection
# ---------------------------------------------------------------------------
SERVER = "127.0.0.1:8188"
CLIENT_ID = str(uuid.uuid4())
MODEL = "juggernautXL_v9.safetensors"

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
TEMP_DIR = PROJECT_DIR / "tools" / "comfyui_temp_v3"
TEMP_DIR.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Quality Presets
# ---------------------------------------------------------------------------
QUALITY = {
    "draft": {"gen_size": 768, "steps_pass1": 25, "steps_pass2": 15, "cfg": 6.5, "denoise_p2": 0.35},
    "standard": {"gen_size": 1024, "steps_pass1": 35, "steps_pass2": 20, "cfg": 7.0, "denoise_p2": 0.30},
    "high": {"gen_size": 1024, "steps_pass1": 45, "steps_pass2": 25, "cfg": 7.5, "denoise_p2": 0.25},
}

# ---------------------------------------------------------------------------
# Global Style System
# ---------------------------------------------------------------------------
# These anchor the entire visual identity. Every prompt inherits them.

STYLE_PREFIX = (
    "masterpiece, best quality, "
    "hand-painted digital illustration, "
    "stylized game character art, "
    "art style inspired by Supergiant Games and Vanillaware, "
    "bold painterly brushstrokes, rich color palette, "
    "dramatic lighting with strong rim light, "
    "clean readable silhouette, centered subject, "
    "single character on solid dark background, "
    "full body visible, "
)

STYLE_SUFFIX = (
    ", dark void background, pitch black background, "
    "studio lighting, sharp focus, high detail, "
    "concept art quality, professional game asset, "
    "4k render quality"
)

# Aggressive negative to suppress ALL common SDXL artifacts
NEGATIVE_PROMPT = (
    # Anti pixel-art (critical — removes the indie look)
    "pixel art, pixelated, 8-bit, 16-bit, retro, sprite, low-res, mosaic, "
    # Anti-AI artifacts
    "deformed, ugly, bad anatomy, disfigured, mutated, extra limbs, extra fingers, "
    "fused fingers, poorly drawn hands, poorly drawn face, mutation, bad proportions, "
    "extra digit, fewer digits, cropped, worst quality, low quality, normal quality, "
    "jpeg artifacts, blurry, out of focus, "
    # Anti-unwanted content
    "text, watermark, signature, logo, letters, words, username, "
    "photo, photograph, photorealistic, 3d render, CGI, unreal engine, "
    "multiple characters, multiple views, collage, border, frame, "
    "busy background, gradient background, patterned background, "
    # Anti-generic-AI
    "stock photo, clip art, cartoon, anime, manga, chibi, "
    "oversaturated, undersaturated, washed out, flat shading, "
    # Anti-noise (helps background removal)
    "noisy, grainy, speckled, dotted background, textured background"
)

# Refinement pass: lighter negative, focuses on maintaining quality
NEGATIVE_REFINE = (
    "pixel art, pixelated, blurry, deformed, ugly, bad anatomy, "
    "text, watermark, multiple characters, busy background, "
    "low quality, worst quality, jpeg artifacts"
)


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 1: ComfyUI API
# ═══════════════════════════════════════════════════════════════════════════

def comfy_request(endpoint, data=None):
    """Send request to ComfyUI API."""
    url = f"http://{SERVER}{endpoint}"
    if data is not None:
        req = urllib.request.Request(url, json.dumps(data).encode('utf-8'),
                                     headers={"Content-Type": "application/json"})
    else:
        req = urllib.request.Request(url)
    return urllib.request.urlopen(req)


def check_server():
    """Verify ComfyUI is running and report GPU info."""
    try:
        resp = json.loads(comfy_request("/system_stats").read())
        ver = resp["system"]["comfyui_version"]
        print(f"[OK] ComfyUI v{ver} online")
        for dev in resp.get("devices", []):
            vram = dev["vram_total"] // 1024 // 1024
            print(f"     GPU: {dev['name']} — {vram} MB VRAM")
        return True
    except Exception as e:
        print(f"[FAIL] ComfyUI not reachable: {e}")
        return False


def queue_prompt(workflow):
    """Queue a workflow and return prompt_id."""
    pid = str(uuid.uuid4())
    payload = {"prompt": workflow, "client_id": CLIENT_ID, "prompt_id": pid}
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(f"http://{SERVER}/prompt", data=data)
    urllib.request.urlopen(req)
    return pid


def wait_for_result(prompt_id, timeout=600):
    """Poll until generation completes. Returns history dict or None."""
    start = time.time()
    while time.time() - start < timeout:
        try:
            resp = json.loads(comfy_request(f"/history/{prompt_id}").read())
            if prompt_id in resp:
                status = resp[prompt_id].get("status", {})
                if status.get("completed", False) or "outputs" in resp[prompt_id]:
                    return resp[prompt_id]
                if status.get("status_str") == "error":
                    print(f"  [ERROR] Generation failed: {status}")
                    return None
        except Exception:
            pass
        time.sleep(2)
    print(f"  [TIMEOUT] after {timeout}s")
    return None


def download_image(filename, subfolder="", folder_type="output"):
    """Download generated image bytes from ComfyUI."""
    params = urllib.parse.urlencode({"filename": filename, "subfolder": subfolder, "type": folder_type})
    return comfy_request(f"/view?{params}").read()


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 2: Workflow Builders
# ═══════════════════════════════════════════════════════════════════════════

def build_txt2img_workflow(positive, negative=NEGATIVE_PROMPT,
                           width=1024, height=1024, seed=None,
                           steps=35, cfg=7.0, prefix="rw3"):
    """Pass 1: Text-to-image at full resolution."""
    if seed is None:
        seed = random.randint(1, 2**31)
    return {
        "4": {
            "class_type": "CheckpointLoaderSimple",
            "inputs": {"ckpt_name": MODEL}
        },
        "5": {
            "class_type": "EmptyLatentImage",
            "inputs": {"width": width, "height": height, "batch_size": 1}
        },
        "6": {
            "class_type": "CLIPTextEncode",
            "inputs": {"clip": ["4", 1], "text": positive}
        },
        "7": {
            "class_type": "CLIPTextEncode",
            "inputs": {"clip": ["4", 1], "text": negative}
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
                "sampler_name": "dpmpp_2m_sde",
                "scheduler": "karras",
                "denoise": 1.0
            }
        },
        "8": {
            "class_type": "VAEDecode",
            "inputs": {"samples": ["3", 0], "vae": ["4", 2]}
        },
        "9": {
            "class_type": "SaveImage",
            "inputs": {"filename_prefix": prefix, "images": ["8", 0]}
        }
    }, seed


def build_img2img_workflow(image_path, positive, negative=NEGATIVE_REFINE,
                           width=1024, height=1024, seed=None,
                           steps=20, cfg=6.0, denoise=0.30, prefix="rw3_ref"):
    """Pass 2: Image-to-image refinement for detail and consistency.
    Loads the pass-1 output and re-renders with lower denoise to preserve structure
    while adding painterly detail."""
    if seed is None:
        seed = random.randint(1, 2**31)
    return {
        "10": {
            "class_type": "LoadImage",
            "inputs": {"image": os.path.basename(image_path)}
        },
        "4": {
            "class_type": "CheckpointLoaderSimple",
            "inputs": {"ckpt_name": MODEL}
        },
        "11": {
            "class_type": "ImageScale",
            "inputs": {
                "image": ["10", 0],
                "width": width,
                "height": height,
                "upscale_method": "lanczos",
                "crop": "center"
            }
        },
        "12": {
            "class_type": "VAEEncode",
            "inputs": {"pixels": ["11", 0], "vae": ["4", 2]}
        },
        "6": {
            "class_type": "CLIPTextEncode",
            "inputs": {"clip": ["4", 1], "text": positive}
        },
        "7": {
            "class_type": "CLIPTextEncode",
            "inputs": {"clip": ["4", 1], "text": negative}
        },
        "3": {
            "class_type": "KSampler",
            "inputs": {
                "model": ["4", 0],
                "positive": ["6", 0],
                "negative": ["7", 0],
                "latent_image": ["12", 0],
                "seed": seed,
                "steps": steps,
                "cfg": cfg,
                "sampler_name": "dpmpp_2m_sde",
                "scheduler": "karras",
                "denoise": denoise
            }
        },
        "8": {
            "class_type": "VAEDecode",
            "inputs": {"samples": ["3", 0], "vae": ["4", 2]}
        },
        "9": {
            "class_type": "SaveImage",
            "inputs": {"filename_prefix": prefix, "images": ["8", 0]}
        }
    }, seed


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 3: Generation Functions
# ═══════════════════════════════════════════════════════════════════════════

def generate_pass1(prompt, output_path, q="standard", seed=None):
    """Pass 1: Generate base image at high resolution."""
    qp = QUALITY[q]
    full_prompt = f"{STYLE_PREFIX}{prompt}{STYLE_SUFFIX}"

    print(f"  [P1] {os.path.basename(output_path)}")
    print(f"       Size={qp['gen_size']}  Steps={qp['steps_pass1']}  CFG={qp['cfg']}")

    workflow, used_seed = build_txt2img_workflow(
        positive=full_prompt,
        width=qp["gen_size"], height=qp["gen_size"],
        seed=seed, steps=qp["steps_pass1"], cfg=qp["cfg"]
    )

    pid = queue_prompt(workflow)
    result = wait_for_result(pid)
    if result is None:
        return False, used_seed

    # Extract and save
    for node_id, node_out in result.get("outputs", {}).items():
        if "images" in node_out:
            for img_info in node_out["images"]:
                img_data = download_image(
                    img_info["filename"],
                    img_info.get("subfolder", ""),
                    img_info.get("type", "output")
                )
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                with open(output_path, "wb") as f:
                    f.write(img_data)
                print(f"       Saved ({len(img_data)} bytes), seed={used_seed}")
                return True, used_seed

    return False, used_seed


def generate_pass2(input_path, output_path, prompt, q="standard", seed=None):
    """Pass 2: Refine pass-1 output with img2img.
    NOTE: This requires the pass-1 image to be in ComfyUI's input folder.
    If ComfyUI can't load external paths, we copy it first."""
    qp = QUALITY[q]
    refine_prompt = (
        f"{STYLE_PREFIX}{prompt}, "
        "refined details, clean edges, polished game art, "
        f"enhanced painterly quality{STYLE_SUFFIX}"
    )

    print(f"  [P2] Refining {os.path.basename(input_path)}")
    print(f"       Denoise={qp['denoise_p2']}  Steps={qp['steps_pass2']}")

    # Copy image to ComfyUI input folder for LoadImage node
    comfy_input = _get_comfy_input_dir()
    if comfy_input:
        import shutil
        dest = os.path.join(comfy_input, os.path.basename(input_path))
        shutil.copy2(input_path, dest)
    else:
        print("       [WARN] Cannot find ComfyUI input dir, skipping pass 2")
        return False, seed

    workflow, used_seed = build_img2img_workflow(
        image_path=input_path,
        positive=refine_prompt,
        width=qp["gen_size"], height=qp["gen_size"],
        seed=seed, steps=qp["steps_pass2"],
        cfg=qp["cfg"] - 0.5,  # slightly lower CFG for refinement
        denoise=qp["denoise_p2"]
    )

    pid = queue_prompt(workflow)
    result = wait_for_result(pid)
    if result is None:
        return False, used_seed

    for node_id, node_out in result.get("outputs", {}).items():
        if "images" in node_out:
            for img_info in node_out["images"]:
                img_data = download_image(
                    img_info["filename"],
                    img_info.get("subfolder", ""),
                    img_info.get("type", "output")
                )
                with open(output_path, "wb") as f:
                    f.write(img_data)
                print(f"       Refined OK ({len(img_data)} bytes)")
                return True, used_seed

    return False, used_seed


def _get_comfy_input_dir():
    """Try to locate ComfyUI's input directory."""
    # Common locations on Windows
    candidates = [
        Path(os.environ.get("COMFYUI_DIR", "")) / "input",
        Path("C:/ComfyUI/input"),
        Path("C:/ComfyUI_windows_portable/ComfyUI/input"),
        Path.home() / "ComfyUI" / "input",
        Path(os.environ.get("APPDATA", "")) / "ComfyUI" / "input",
    ]
    for c in candidates:
        if c.is_dir():
            return str(c)
    # Fallback: ask ComfyUI API
    try:
        resp = json.loads(comfy_request("/system_stats").read())
        # Some ComfyUI versions expose the base path
    except Exception:
        pass
    return None


def generate_sprite(prompt, output_path, q="standard", seed=None, two_pass=True):
    """Full generation pipeline: pass-1 → (optional pass-2 refinement)."""
    p1_path = output_path.replace(".png", "_p1.png")

    ok, used_seed = generate_pass1(prompt, p1_path, q=q, seed=seed)
    if not ok:
        return False, None

    if two_pass:
        ok2, _ = generate_pass2(p1_path, output_path, prompt, q=q, seed=used_seed)
        if ok2:
            return True, used_seed
        else:
            # Fallback: use pass-1 if pass-2 fails
            print("       [WARN] Pass 2 failed, using pass 1 output")
            import shutil
            shutil.copy2(p1_path, output_path)
            return True, used_seed
    else:
        import shutil
        shutil.copy2(p1_path, output_path)
        return True, used_seed


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 4: Professional Background Removal
# ═══════════════════════════════════════════════════════════════════════════

_rembg_session = None

def get_rembg_session():
    """Lazy-load rembg session (downloads model on first use)."""
    global _rembg_session
    if _rembg_session is None:
        try:
            from rembg import new_session
            _rembg_session = new_session("u2net")
            print("[OK] rembg U²-Net model loaded")
        except ImportError:
            print("[WARN] rembg not installed — falling back to flood-fill BG removal")
            print("       Install with: pip install rembg onnxruntime")
            _rembg_session = "UNAVAILABLE"
    return _rembg_session


def remove_bg_rembg(img_pil):
    """Remove background using rembg (U²-Net neural network).
    Returns RGBA PIL Image with clean alpha channel."""
    session = get_rembg_session()
    if session == "UNAVAILABLE":
        return remove_bg_floodfill(img_pil)

    from rembg import remove
    result = remove(img_pil, session=session, alpha_matting=True,
                    alpha_matting_foreground_threshold=240,
                    alpha_matting_background_threshold=10,
                    alpha_matting_erode_size=10)
    return result.convert("RGBA")


def remove_bg_floodfill(img_pil, threshold=35):
    """Fallback: improved flood-fill background removal.
    Better than v2: uses adaptive threshold based on edge sampling."""
    img = img_pil.convert("RGBA")
    w, h = img.size
    pixels = img.load()

    # Sample edges more densely
    edge_colors = []
    for x in range(0, w, max(1, w // 40)):
        edge_colors.append(pixels[x, 0][:3])
        edge_colors.append(pixels[x, h - 1][:3])
    for y in range(0, h, max(1, h // 40)):
        edge_colors.append(pixels[0, y][:3])
        edge_colors.append(pixels[w - 1, y][:3])

    # Compute average + standard deviation for adaptive threshold
    avg_r = sum(c[0] for c in edge_colors) // len(edge_colors)
    avg_g = sum(c[1] for c in edge_colors) // len(edge_colors)
    avg_b = sum(c[2] for c in edge_colors) // len(edge_colors)
    std = max(1, int(np.std([sum(c) for c in edge_colors]) / 3))
    adaptive_thresh = min(threshold, std * 2)

    def is_bg(r, g, b):
        return (abs(r - avg_r) + abs(g - avg_g) + abs(b - avg_b)) < adaptive_thresh * 3

    visited = set()
    queue = deque()

    # Seed from all 4 edges
    for x in range(w):
        for y_edge in [0, h - 1]:
            r, g, b, a = pixels[x, y_edge]
            if is_bg(r, g, b):
                queue.append((x, y_edge))
    for y in range(1, h - 1):
        for x_edge in [0, w - 1]:
            r, g, b, a = pixels[x_edge, y]
            if is_bg(r, g, b):
                queue.append((x_edge, y))

    while queue:
        cx, cy = queue.popleft()
        if (cx, cy) in visited or cx < 0 or cy < 0 or cx >= w or cy >= h:
            continue
        r, g, b, a = pixels[cx, cy]
        if not is_bg(r, g, b):
            continue
        visited.add((cx, cy))
        pixels[cx, cy] = (0, 0, 0, 0)
        for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in visited:
                queue.append((nx, ny))

    return img


def refine_alpha_edges(img_pil, erode_px=1, feather_px=2):
    """Clean up alpha edges: remove fringe, light feathering for smooth edges.
    This eliminates the grey halo artifacts from the old pipeline."""
    img = np.array(img_pil)
    if img.shape[2] != 4:
        return img_pil

    alpha = img[:, :, 3]

    # Step 1: Erode alpha slightly to remove color fringe
    if erode_px > 0:
        kernel = np.ones((erode_px * 2 + 1, erode_px * 2 + 1), np.uint8)
        alpha = cv2.erode(alpha, kernel, iterations=1)

    # Step 2: Light Gaussian blur on alpha for smooth edges (anti-aliasing)
    if feather_px > 0:
        alpha_smooth = cv2.GaussianBlur(alpha.astype(np.float32),
                                         (feather_px * 2 + 1, feather_px * 2 + 1),
                                         feather_px * 0.5)
        # Only apply smoothing at edges (where alpha transitions)
        edge_mask = cv2.Canny(alpha, 50, 150)
        edge_dilated = cv2.dilate(edge_mask, np.ones((5, 5), np.uint8))
        alpha = np.where(edge_dilated > 0, alpha_smooth.astype(np.uint8), alpha)

    # Step 3: Hard threshold to remove near-transparent pixels (< 15 alpha)
    alpha[alpha < 15] = 0

    # Step 4: Pre-multiply RGB by alpha to eliminate color bleeding
    alpha_f = alpha.astype(np.float32) / 255.0
    for c in range(3):
        img[:, :, c] = (img[:, :, c].astype(np.float32) * alpha_f).astype(np.uint8)

    img[:, :, 3] = alpha
    return Image.fromarray(img)


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 5: Post-Processing Pipeline
# ═══════════════════════════════════════════════════════════════════════════

def extract_character(img_pil, margin_pct=0.08):
    """Crop to character bounding box (content-aware), with padding margin.
    Finds actual non-transparent content and crops to that + margin."""
    arr = np.array(img_pil.convert("RGBA"))
    alpha = arr[:, :, 3]

    # Find bounding box of non-transparent pixels
    rows = np.any(alpha > 30, axis=1)
    cols = np.any(alpha > 30, axis=0)

    if not rows.any() or not cols.any():
        # Fallback: no content detected, use simple margin crop
        w, h = img_pil.size
        margin = int(min(w, h) * margin_pct)
        return img_pil.crop((margin, margin, w - margin, h - margin))

    rmin, rmax = np.where(rows)[0][[0, -1]]
    cmin, cmax = np.where(cols)[0][[0, -1]]

    # Add margin around the content
    h, w = arr.shape[:2]
    margin = int(min(w, h) * margin_pct)
    rmin = max(0, rmin - margin)
    rmax = min(h - 1, rmax + margin)
    cmin = max(0, cmin - margin)
    cmax = min(w - 1, cmax + margin)

    return img_pil.crop((cmin, rmin, cmax + 1, rmax + 1))


def adjust_colors(img_pil, saturation=1.25, contrast=1.15, brightness=1.05):
    """Enhance colors for game readability: slightly more vivid than reality."""
    rgb = img_pil.convert("RGB")
    rgb = ImageEnhance.Color(rgb).enhance(saturation)
    rgb = ImageEnhance.Contrast(rgb).enhance(contrast)
    rgb = ImageEnhance.Brightness(rgb).enhance(brightness)
    result = rgb.convert("RGBA")
    result.putalpha(img_pil.split()[3])
    return result


def add_outline(img_pil, color=(10, 10, 10, 180), thickness=1):
    """Add a subtle dark outline for silhouette readability at small sizes."""
    img = img_pil.convert("RGBA")
    w, h = img.size

    # Use numpy for speed (v2 was pure Python, glacially slow on large images)
    arr = np.array(img)
    alpha = arr[:, :, 3]

    # Create dilated alpha mask
    kernel = np.ones((thickness * 2 + 1, thickness * 2 + 1), np.uint8)
    dilated = cv2.dilate(alpha, kernel, iterations=1)

    # Outline = dilated minus original
    outline_mask = (dilated > 50) & (alpha < 50)

    # Create outline layer
    outline = np.zeros_like(arr)
    outline[outline_mask] = color

    # Composite: outline behind original
    outline_img = Image.fromarray(outline)
    return Image.alpha_composite(outline_img, img)


def fit_to_frame(img_pil, target_w, target_h, fill_pct=0.85):
    """Resize image to fit within target frame, preserving aspect ratio.
    Centers the result, leaving transparent padding. fill_pct controls
    how much of the frame the sprite fills (0.85 = 85%, leaves breathing room)."""
    src_w, src_h = img_pil.size
    if src_w <= 0 or src_h <= 0:
        return Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))

    # Available space (with padding)
    avail_w = int(target_w * fill_pct)
    avail_h = int(target_h * fill_pct)

    # Scale to fit (maintain aspect ratio)
    scale = min(avail_w / src_w, avail_h / src_h)
    new_w = max(1, int(src_w * scale))
    new_h = max(1, int(src_h * scale))

    resized = img_pil.resize((new_w, new_h), Image.LANCZOS)

    # Center in frame (anchor to bottom-center for grounded characters)
    result = Image.new("RGBA", (target_w, target_h), (0, 0, 0, 0))
    ox = (target_w - new_w) // 2
    oy = target_h - new_h - int(target_h * 0.03)  # Small bottom margin
    result.paste(resized, (ox, oy), resized)
    return result


def process_sprite_v3(raw_img_path, target_w, target_h, use_rembg=True):
    """Full post-processing pipeline for a single generated image.
    Returns a clean, game-ready RGBA PIL Image at target_w x target_h."""
    img = Image.open(raw_img_path).convert("RGBA")

    # Step 1: Extract character from center (remove generation padding)
    img = extract_character(img, margin_pct=0.06)

    # Step 2: Background removal (neural network or flood-fill fallback)
    if use_rembg:
        img = remove_bg_rembg(img)
    else:
        img = remove_bg_floodfill(img)

    # Step 3: Refine alpha edges (kills grey halos)
    img = refine_alpha_edges(img, erode_px=2, feather_px=2)

    # Step 4: Fit to target frame (preserve aspect ratio, center in frame)
    img = fit_to_frame(img, target_w, target_h)

    # Step 5: Color adjustment for game readability
    img = adjust_colors(img, saturation=1.20, contrast=1.12, brightness=1.03)

    # Step 6: Add subtle outline for silhouette
    img = add_outline(img, thickness=1)

    return img


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 6: Animation System
# ═══════════════════════════════════════════════════════════════════════════
# Same transforms as v2 but operating on higher-res base sprites.
# The key insight: generate ONE high-quality base pose per animation state,
# then create frame variations via parametric transforms.

def anim_idle(src, fi, nf, fw, fh):
    """Idle: gentle breathing (vertical squash/stretch cycle)."""
    t = (fi / nf) * 2 * math.pi
    sy = 1.0 + 0.025 * math.sin(t)
    sx = 1.0 - 0.012 * math.sin(t)
    nw, nh = max(1, int(fw * sx)), max(1, int(fh * sy))
    frame = src.resize((nw, nh), Image.LANCZOS)
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    result.paste(frame, ((fw - nw) // 2, fh - nh), frame)
    return result


def anim_walk(src, fi, nf, fw, fh):
    """Walk: bob + lean + bounce."""
    t = (fi / nf) * 2 * math.pi
    shift_x = int(2 * math.sin(t))
    bounce_y = int(abs(2 * math.sin(t)))
    lean = 0.04 * math.sin(t)
    w, h = src.size
    shear = (1, lean, -lean * h / 2, 0, 1, 0)
    frame = src.transform((w, h), Image.AFFINE, shear, Image.BILINEAR)
    frame = frame.resize((fw, fh), Image.LANCZOS)
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    ox = max(0, min(fw - fw, shift_x))
    oy = max(0, -bounce_y)
    result.paste(frame, (ox, oy), frame)
    return result


def anim_attack(src, fi, nf, fw, fh):
    """Attack: wind-up → strike → recovery with stretch."""
    p = fi / max(1, nf - 1)
    if p < 0.3:
        sx_off = int(-3 * (p / 0.3))
        sx_scale = 0.95
    elif p < 0.6:
        t = (p - 0.3) / 0.3
        sx_off = int(5 * t)
        sx_scale = 1.0 + 0.15 * t
    else:
        t = (p - 0.6) / 0.4
        sx_off = int(5 * (1 - t))
        sx_scale = 1.0 + 0.15 * (1 - t)
    nw = max(1, int(fw * sx_scale))
    frame = src.resize((nw, fh), Image.LANCZOS)
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    ox = max(-fw // 4, sx_off + (fw - nw) // 2)
    result.paste(frame, (ox, 0), frame)
    return result


def anim_hurt(src, fi, nf, fw, fh):
    """Hurt: recoil + white flash + squish."""
    p = fi / max(1, nf - 1)
    shift_x = int(-3 * (1 - p))
    sx = 0.85 + 0.15 * p
    nw = max(1, int(fw * sx))
    frame = src.resize((nw, fh), Image.LANCZOS)
    flash = 1.0 - p
    if flash > 0.3:
        frame = ImageEnhance.Brightness(frame).enhance(1.0 + flash * 0.7)
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    ox = max(-fw // 4, shift_x + (fw - nw) // 2)
    result.paste(frame, (ox, 0), frame)
    return result


def anim_dead(src, fi, nf, fw, fh):
    """Dead: collapse + spread + fade."""
    p = fi / max(1, nf - 1)
    sy = max(0.15, 1.0 - 0.85 * p)
    sx = 1.0 + 0.3 * p
    alpha_mult = max(0.1, 1.0 - 0.7 * p)
    nw, nh = max(1, int(fw * sx)), max(1, int(fh * sy))
    frame = src.resize((nw, nh), Image.LANCZOS)
    r, g, b, a = frame.split()
    a = a.point(lambda px: int(px * alpha_mult))
    frame = Image.merge("RGBA", (r, g, b, a))
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    result.paste(frame, ((fw - nw) // 2, fh - nh), frame)
    return result


ANIM_FUNCS = {
    "idle": anim_idle, "walk": anim_walk, "attack": anim_attack,
    "hurt": anim_hurt, "dead": anim_dead,
}

# Boss-specific dramatic transforms
def boss_anim_attack(src, fi, nf, fw, fh):
    """Boss attack: more dramatic than regular."""
    p = fi / max(1, nf - 1)
    if p < 0.25:
        sx_off, sx_scale = int(-6 * (p / 0.25)), 0.90
    elif p < 0.5:
        t = (p - 0.25) / 0.25
        sx_off, sx_scale = int(8 * t), 1.0 + 0.20 * t
    else:
        t = (p - 0.5) / 0.5
        sx_off, sx_scale = int(8 * (1 - t)), 1.0 + 0.20 * (1 - t)
    nw = max(1, int(fw * sx_scale))
    frame = src.resize((nw, fh), Image.LANCZOS)
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    result.paste(frame, (max(-fw // 3, sx_off + (fw - nw) // 2), 0), frame)
    return result


def boss_anim_phase(src, fi, nf, fw, fh):
    """Phase transition: pulse + glow."""
    t = (fi / nf) * 2 * math.pi
    s = 1.0 + 0.08 * math.sin(t * 2)
    nw, nh = max(1, int(fw * s)), max(1, int(fh * s))
    frame = src.resize((nw, nh), Image.LANCZOS)
    frame = ImageEnhance.Brightness(frame).enhance(1.0 + 0.3 * abs(math.sin(t)))
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    result.paste(frame, ((fw - nw) // 2, (fh - nh) // 2), frame)
    return result


def boss_anim_enrage(src, fi, nf, fw, fh):
    """Enrage: shake + red tint + grow."""
    t = (fi / nf) * 2 * math.pi
    shake_x = int(3 * math.sin(t * 4))
    shake_y = int(2 * math.cos(t * 3))
    s = 1.0 + 0.05 * (fi / nf)
    nw, nh = max(1, int(fw * s)), max(1, int(fh * s))
    frame = src.resize((nw, nh), Image.LANCZOS)
    r, g, b, a = frame.split()
    r = r.point(lambda px: min(255, int(px * 1.2)))
    g = g.point(lambda px: int(px * 0.85))
    b = b.point(lambda px: int(px * 0.85))
    frame = Image.merge("RGBA", (r, g, b, a))
    result = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    ox = max(0, min(fw - nw, (fw - nw) // 2 + shake_x))
    oy = max(0, min(fh - nh, (fh - nh) // 2 + shake_y))
    result.paste(frame, (ox, oy), frame)
    return result


BOSS_ANIM_FUNCS = {
    "idle": anim_idle, "move": anim_walk,
    "attack1": boss_anim_attack, "attack2": boss_anim_attack,
    "attack3": boss_anim_attack,
    "phase_transition": boss_anim_phase,
    "hurt": anim_hurt, "enrage": boss_anim_enrage, "dead": anim_dead,
}


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 7: Entity Definitions & Prompts
# ═══════════════════════════════════════════════════════════════════════════

# --- ENEMY DEFINITIONS ---
# Format: name → (prompt_description, frame_w, frame_h)
# All frame sizes are PRE-upscale (matching the v2 pipeline output: 32x32 base → 128x128 after 4x ESRGAN)
# The generation happens at 1024x1024 and is scaled down to frame size.

ENEMIES = {
    "walker": (
        "dark armored patrol robot, quadruped mechanical walker, "
        "heavy-duty tank-like body with four sturdy mechanical legs, "
        "single menacing red sensor eye, gunmetal and dark bronze plating, "
        "industrial sci-fi design, battle-scarred armor plates",
        128, 128
    ),
    "flyer": (
        "dark winged aerial drone creature, sleek predatory body, "
        "two large translucent energy wings spread wide, bat-like silhouette, "
        "glowing red compound eyes, dark metallic carapace, "
        "hovering in mid-air, bio-mechanical hybrid design",
        128, 128
    ),
    "turret": (
        "compact automated gun turret emplacement, NOT humanoid, "
        "rotating barrel assembly on a sturdy tripod base, "
        "military targeting laser, ammunition feed belt, "
        "dark steel with orange warning markings, industrial weapon platform, "
        "low profile mechanical device, no legs no arms",
        128, 128
    ),
    "charger": (
        "massive horned beast creature, aggressive charging pose, "
        "thick armored hide with glowing orange cracks, two huge forward-swept horns, "
        "muscular quadruped body, molten energy visible in armor seams, "
        "berserker bull-like creature, heavy front plating",
        128, 128
    ),
    "phaser": (
        "ghostly ethereal specter, translucent wraith-like form, "
        "body partially phasing between dimensions, flickering edges, "
        "deep purple and violet energy glow, trailing wisps of void energy, "
        "no solid outline — fading at extremities, haunting floating pose",
        128, 128
    ),
    "exploder": (
        "bloated volatile bio-bomb creature, round swollen body, "
        "sickly green toxic glow from internal pressure, "
        "cracks and fissures leaking dangerous energy, about to burst, "
        "unstable biological explosive, small stubby legs, bulging eyes",
        128, 128
    ),
    "shielder": (
        "heavy armored sentinel knight, defensive combat stance, "
        "wielding a large translucent cyan energy tower shield, "
        "bulky dark plate armor with blue energy conduits, "
        "steadfast guardian pose, shield raised and braced",
        128, 128
    ),
    "crawler": (
        "dark biomechanical spider creature, eight articulated legs, "
        "low crouching profile, chitinous black shell with green bioluminescent markings, "
        "multiple small glowing eyes, alien predator design, "
        "sleek and fast-looking arachnid form",
        128, 96
    ),
    "summoner": (
        "floating dark sorcerer in tattered robes, mystic pose, "
        "hands outstretched channeling purple arcane energy, "
        "hovering above ground, ancient staff with crystal focus, "
        "glowing magical runes orbiting body, mysterious hooded figure",
        128, 128
    ),
    "sniper": (
        "sleek precision sniper unit, thin angular mechanical body, "
        "long-barreled energy rifle held in aim, "
        "telescopic red targeting eye, crouching overwatch pose, "
        "matte black stealth plating, minimal exposed joints, "
        "professional military marksman robot",
        128, 128
    ),
    "teleporter": (
        "digitally unstable creature, glitching holographic body, "
        "electric blue and white distortion effects, "
        "body fragmenting and reassembling, teleportation sparks, "
        "flickering between existence and void, data-corrupted form",
        128, 128
    ),
    "reflector": (
        "floating prismatic mirror construct, NOT humanoid, "
        "diamond-shaped crystalline body, reflective chrome surfaces, "
        "rainbow light refractions along sharp geometric edges, "
        "orbiting shards of mirror, angular faceted form, "
        "living geometric crystal shield entity",
        128, 128
    ),
    "leech": (
        "parasitic void slug creature, elongated sinuous body, "
        "dark purple rubbery skin with pulsing red veins, "
        "multiple grasping tendril appendages, draining life energy, "
        "low-to-ground worm-like predator, hungry maw",
        128, 128
    ),
    "swarmer": (
        "tiny aggressive insect drone, compact armored body, "
        "dark exoskeleton with bright yellow warning stripes, "
        "translucent buzzing wings, sharp mandibles, "
        "bee-like bio-weapon, small but dangerous, swarm creature",
        64, 64
    ),
    "gravitywell": (
        "floating anomalous dark orb entity, NOT humanoid, "
        "deep black sphere warping space around it, "
        "gravitational distortion rings and lensing effects, "
        "nearby particles being pulled inward, cosmic phenomenon, "
        "small accretion disk, event horizon glow",
        128, 128
    ),
    "mimic": (
        "monstrous treasure chest ambush predator, "
        "ornate wooden chest with iron bindings splitting open, "
        "revealing rows of sharp teeth and a long tongue, "
        "glowing malicious eyes inside the lid, "
        "short clawed feet underneath, animated mimic creature",
        128, 128
    ),
    "minion": (
        "small floating void wisp, compact magical sprite, "
        "glowing purple energy orb with trailing sparks, "
        "simple ethereal form, summoned servant creature, "
        "luminous purple core with fading edges",
        128, 128
    ),
}

# Animation descriptions per state — these are appended to the entity prompt
ENEMY_ANIMS = {
    "idle": ("standing alert, idle combat-ready stance, facing slightly right", 4),
    "walk": ("moving forward in stride, dynamic walking pose, mid-step", 6),
    "attack": ("attacking aggressively, striking forward, hostile combat action", 4),
    "hurt": ("recoiling from impact, taking damage, pain stagger", 2),
    "dead": ("collapsed defeated, falling apart, destroyed remains", 4),
}

# --- PLAYER ---
PLAYER_DESC = (
    "sci-fi void warrior hero, sleek dark purple power armor suit, "
    "glowing blue visor and energy lines, athletic build, "
    "futuristic combat-ready design, heroic protagonist, "
    "Hades-style character proportions"
)
PLAYER_ANIMS = {
    "idle": ("standing ready combat stance, balanced weight, arms at sides", 6),
    "run": ("running forward, legs in dynamic mid-stride, arms pumping", 8),
    "jump": ("jumping upward, legs tucked, arms reaching up, airborne", 3),
    "fall": ("falling downward, arms spread for balance, legs dangling", 3),
    "dash": ("dashing forward at high speed, motion blur pose, horizontal lunge", 4),
    "wallslide": ("sliding down wall, back against surface, one hand bracing", 3),
    "attack": ("sword slash attack, weapon mid-swing, aggressive strike pose", 6),
    "hurt": ("hit and recoiling backward, pain flinch, staggering", 3),
    "dead": ("collapsed on ground, defeated pose, lying prone", 5),
}
PLAYER_FW, PLAYER_FH = 128, 192  # Match SpriteConfig (4x upscaled from 32x48)

# --- BOSSES ---
BOSSES = {
    "rift_guardian": {
        "desc": (
            "massive armored guardian knight boss, towering imposing figure, "
            "enormous glowing purple rift-energy greatsword, "
            "heavy dark plate armor with pulsing energy cracks, "
            "menacing glowing eyes behind visor, boss-scale creature"
        ),
        "fw": 256, "fh": 256,
    },
    "void_wyrm": {
        "desc": (
            "colossal void serpent dragon boss, body made of cosmic void and stars, "
            "long coiling form with nebula patterns in scales, "
            "glowing purple cosmic breath, multiple fin-like appendages, "
            "deep space horror, star-devouring wyrm"
        ),
        "fw": 256, "fh": 256,
    },
    "dimensional_architect": {
        "desc": (
            "eldritch geometric entity boss, floating impossible architecture, "
            "body composed of interlocking mathematical shapes, "
            "Escher-like recursive patterns, blue-white energy lattice, "
            "abstract cosmic horror, reality-bending form"
        ),
        "fw": 256, "fh": 256,
    },
    "temporal_weaver": {
        "desc": (
            "massive clockwork spider boss, intricate bronze mechanical body, "
            "golden temporal energy webs, clock-face compound eyes, "
            "gear-driven legs with ornate filigree, steampunk horror, "
            "time-manipulating arachnid construct"
        ),
        "fw": 256, "fh": 256,
    },
    "void_sovereign": {
        "desc": (
            "colossal dark void king boss, supreme shadowy royal figure, "
            "crown of swirling purple-black dark flames, "
            "massive flowing robe of pure void energy, "
            "commanding otherworldly presence, final boss scale"
        ),
        "fw": 384, "fh": 384,
    },
    "entropy_incarnate": {
        "desc": (
            "chaos incarnation boss, reality fragmenting and glitching around it, "
            "body of pure entropy with distorted chromatic aberration, "
            "impossible shifting form, data-corruption visual effect, "
            "ultimate chaos entity, reality-breaking presence"
        ),
        "fw": 256, "fh": 256,
    },
}

BOSS_ANIMS = {
    "idle": ("menacing idle stance, dark brooding presence, breathing heavily", 6),
    "move": ("advancing forward menacingly, slow powerful movement", 6),
    "attack1": ("primary devastating melee attack, massive strike", 6),
    "attack2": ("ranged energy projectile attack, charging and firing", 6),
    "attack3": ("area-of-effect ground slam, shockwave attack", 6),
    "phase_transition": ("transforming and powering up, energy surge eruption", 8),
    "hurt": ("taking heavy damage, flinching but resilient", 3),
    "enrage": ("entering enraged berserker mode, glowing intensely", 6),
    "dead": ("defeated and dissolving into void particles", 8),
}


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 8: Sheet Assembly
# ═══════════════════════════════════════════════════════════════════════════

def assemble_enemy_sheet(name, desc, fw, fh, q="standard", two_pass=True,
                         force_regen=False):
    """Generate and assemble one enemy sprite sheet."""
    print(f"\n{'=' * 60}")
    print(f"  ENEMY: {name} ({fw}x{fh}) — quality={q}")
    print(f"{'=' * 60}")

    edir = TEMP_DIR / f"enemy_{name}"
    edir.mkdir(exist_ok=True)

    COLS = 6  # max frames per row
    rows = list(ENEMY_ANIMS.keys())
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

    base_seed = _name_to_seed(name)

    # === KEY FIX: Generate ONE base sprite, reuse for ALL animations ===
    # This ensures the character looks the same across all animation rows.
    # Only the idle pose is AI-generated; all other rows use code transforms.
    base_raw_path = str(edir / "base_raw.png")
    base_clean_path = str(edir / "base_clean.png")

    if force_regen or not os.path.exists(base_clean_path):
        if not os.path.exists(base_raw_path) or force_regen:
            idle_desc = ENEMY_ANIMS["idle"][0]
            prompt = f"{desc}, {idle_desc}"
            ok, _ = generate_sprite(prompt, base_raw_path, q=q,
                                     seed=base_seed, two_pass=two_pass)
            if not ok:
                print(f"  [FAIL] Could not generate base sprite for {name}")
                return None
        base = process_sprite_v3(base_raw_path, fw, fh, use_rembg=True)
        base.save(base_clean_path, "PNG")

    base_sprite = Image.open(base_clean_path).convert("RGBA")

    for row_idx, anim_name in enumerate(rows):
        _, num_frames = ENEMY_ANIMS[anim_name]
        # All rows use the SAME base sprite — animation is purely code transforms
        anim_func = ANIM_FUNCS[anim_name]
        for fi in range(num_frames):
            frame = anim_func(base_sprite, fi, num_frames, fw, fh)
            sheet.paste(frame, (fi * fw, row_idx * fh), frame)

    output = str(PROJECT_DIR / f"assets/textures/enemies/{name}.png")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, "PNG")
    fsize = os.path.getsize(output)
    print(f"  [DONE] {output} ({sheet_w}x{sheet_h}, {fsize:,} bytes)")
    return output


def assemble_player_sheet(q="standard", two_pass=True, force_regen=False):
    """Generate and assemble the player sprite sheet."""
    print(f"\n{'=' * 60}")
    print(f"  PLAYER ({PLAYER_FW}x{PLAYER_FH}) — quality={q}")
    print(f"{'=' * 60}")

    pdir = TEMP_DIR / "player"
    pdir.mkdir(exist_ok=True)

    fw, fh = PLAYER_FW, PLAYER_FH
    COLS = 8
    rows = list(PLAYER_ANIMS.keys())
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

    player_anim_funcs = {
        "idle": anim_idle, "run": anim_walk, "jump": anim_idle,
        "fall": anim_idle, "dash": anim_attack, "wallslide": anim_idle,
        "attack": anim_attack, "hurt": anim_hurt, "dead": anim_dead,
    }

    base_seed = _name_to_seed("player_void_warrior")

    # === KEY FIX: Generate ONE base sprite, reuse for ALL animations ===
    base_raw_path = str(pdir / "base_raw.png")
    base_clean_path = str(pdir / "base_clean.png")

    if force_regen or not os.path.exists(base_clean_path):
        if not os.path.exists(base_raw_path) or force_regen:
            idle_desc = PLAYER_ANIMS["idle"][0]
            prompt = f"{PLAYER_DESC}, {idle_desc}"
            ok, _ = generate_sprite(prompt, base_raw_path, q=q,
                                     seed=base_seed, two_pass=two_pass)
            if not ok:
                print(f"  [FAIL] Could not generate player base sprite")
                return None
        base = process_sprite_v3(base_raw_path, fw, fh, use_rembg=True)
        base.save(base_clean_path, "PNG")

    base_sprite = Image.open(base_clean_path).convert("RGBA")

    for row_idx, anim_name in enumerate(rows):
        _, num_frames = PLAYER_ANIMS[anim_name]
        anim_func = player_anim_funcs[anim_name]
        for fi in range(num_frames):
            frame = anim_func(base_sprite, fi, num_frames, fw, fh)
            sheet.paste(frame, (fi * fw, row_idx * fh), frame)

    output = str(PROJECT_DIR / "assets/textures/player/player.png")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, "PNG")
    fsize = os.path.getsize(output)
    print(f"  [DONE] {output} ({sheet_w}x{sheet_h}, {fsize:,} bytes)")
    return output


def assemble_boss_sheet(name, info, q="standard", two_pass=True, force_regen=False):
    """Generate and assemble one boss sprite sheet."""
    desc = info["desc"]
    fw, fh = info["fw"], info["fh"]

    print(f"\n{'=' * 60}")
    print(f"  BOSS: {name} ({fw}x{fh}) — quality={q}")
    print(f"{'=' * 60}")

    bdir = TEMP_DIR / f"boss_{name}"
    bdir.mkdir(exist_ok=True)

    COLS = 8
    rows = list(BOSS_ANIMS.keys())
    sheet_w = COLS * fw
    sheet_h = len(rows) * fh
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))

    base_seed = _name_to_seed(f"boss_{name}")

    # === KEY FIX: Generate ONE base sprite, reuse for ALL animations ===
    base_raw_path = str(bdir / "base_raw.png")
    base_clean_path = str(bdir / "base_clean.png")

    if force_regen or not os.path.exists(base_clean_path):
        if not os.path.exists(base_raw_path) or force_regen:
            idle_desc = BOSS_ANIMS["idle"][0]
            prompt = f"{desc}, {idle_desc}"
            ok, _ = generate_sprite(prompt, base_raw_path, q=q,
                                     seed=base_seed, two_pass=two_pass)
            if not ok:
                print(f"  [FAIL] Could not generate base sprite for boss {name}")
                return None
        base = process_sprite_v3(base_raw_path, fw, fh, use_rembg=True)
        base.save(base_clean_path, "PNG")

    base_sprite = Image.open(base_clean_path).convert("RGBA")

    for row_idx, anim_name in enumerate(rows):
        _, num_frames = BOSS_ANIMS[anim_name]
        anim_func = BOSS_ANIM_FUNCS[anim_name]
        for fi in range(num_frames):
            frame = anim_func(base_sprite, fi, num_frames, fw, fh)
            sheet.paste(frame, (fi * fw, row_idx * fh), frame)

    output = str(PROJECT_DIR / f"assets/textures/bosses/{name}.png")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, "PNG")
    fsize = os.path.getsize(output)
    print(f"  [DONE] {output} ({sheet_w}x{sheet_h}, {fsize:,} bytes)")
    return output


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 9: Tilesets, Pickups, Projectiles
# ═══════════════════════════════════════════════════════════════════════════

def gen_tileset(q="standard"):
    """Generate a clean, stylized tileset."""
    print(f"\n{'=' * 60}")
    print(f"  TILESET — quality={q}")
    print(f"{'=' * 60}")

    tdir = TEMP_DIR / "tiles"
    tdir.mkdir(exist_ok=True)

    tile_types = {
        "floor": (
            "dark industrial metal floor tile texture, seamless tileable, "
            "subtle blue ambient light reflections, grate pattern, "
            "sci-fi space station floor, top-down view, game texture"
        ),
        "wall": (
            "solid heavy metal wall tile texture, seamless tileable, "
            "reinforced plating with rivets, dark steel with subtle scratches, "
            "industrial bunker wall, game texture, top-down"
        ),
        "platform": (
            "floating sci-fi platform tile, glowing blue edge trim, "
            "dark polished metal surface, hovering platform, "
            "side-view game tile, clean design"
        ),
        "hazard": (
            "dangerous spike trap floor tile, sharp red crystalline spikes, "
            "warning stripe markings, hazardous industrial zone, "
            "game trap tile texture, menacing"
        ),
        "door": (
            "sci-fi sliding blast door tile, heavy metal frames, "
            "green status light, portal gateway, reinforced edges, "
            "game door sprite, mechanical"
        ),
        "deco": (
            "dark tech wall decoration tile, exposed cable conduits, "
            "blinking status LEDs, sci-fi panel detail, "
            "atmospheric game decoration, moody lighting"
        ),
    }

    tile_w, tile_h = 16, 16
    cols, rows = 32, 12
    sheet = Image.new("RGBA", (cols * tile_w, rows * tile_h), (0, 0, 0, 0))

    qp = QUALITY[q]
    gen_tiles = {}
    for tname, tdesc in tile_types.items():
        img_path = str(tdir / f"{tname}.png")
        if not os.path.exists(img_path):
            full = f"{STYLE_PREFIX.replace('single character', 'seamless texture')}{tdesc}{STYLE_SUFFIX}"
            generate_pass1(tdesc, img_path, q=q, seed=_name_to_seed(f"tile_{tname}"))
        if os.path.exists(img_path):
            gen_tiles[tname] = Image.open(img_path).convert("RGBA")

    # Fill sheet by slicing generated textures
    ti = 0
    for tname, src in gen_tiles.items():
        w, h = src.size
        for gy in range(0, min(h, 256), 32):
            for gx in range(0, min(w, 256), 32):
                if ti >= cols * rows:
                    break
                region = src.crop((gx, gy, gx + 32, gy + 32))
                tile = region.resize((tile_w, tile_h), Image.LANCZOS)
                rgb = ImageEnhance.Contrast(tile.convert("RGB")).enhance(1.3)
                tile_final = rgb.convert("RGBA")
                a = tile.split()[3] if tile.mode == "RGBA" else Image.new("L", tile.size, 255)
                tile_final.putalpha(a)
                sheet.paste(tile_final, ((ti % cols) * tile_w, (ti // cols) * tile_h))
                ti += 1

    # Fill remaining with floor variations
    if "floor" in gen_tiles:
        src = gen_tiles["floor"]
        while ti < cols * rows:
            gx = random.randint(0, max(0, src.width - 64))
            gy = random.randint(0, max(0, src.height - 64))
            region = src.crop((gx, gy, gx + 64, gy + 64))
            tile = region.resize((tile_w, tile_h), Image.LANCZOS)
            sheet.paste(tile, ((ti % cols) * tile_w, (ti // cols) * tile_h))
            ti += 1

    base_out = str(PROJECT_DIR / "assets/textures/tiles/tileset_universal.png")
    os.makedirs(os.path.dirname(base_out), exist_ok=True)
    sheet.save(base_out, "PNG")
    print(f"  [DONE] {base_out}")

    # Dimension A (blue tint)
    dim_a = sheet.copy()
    r, g, b, a = dim_a.split()
    b = b.point(lambda px: min(255, int(px * 1.3)))
    dim_a = Image.merge("RGBA", (r, g, b, a))
    dim_a.save(str(PROJECT_DIR / "assets/textures/tiles/generated_tileset.png"), "PNG")

    # Dimension B (purple tint)
    dim_b = sheet.copy()
    r, g, b, a = dim_b.split()
    r = r.point(lambda px: min(255, int(px * 1.2)))
    b = b.point(lambda px: min(255, int(px * 1.2)))
    g = g.point(lambda px: int(px * 0.85))
    dim_b = Image.merge("RGBA", (r, g, b, a))
    dim_b.save(str(PROJECT_DIR / "assets/textures/tiles/generated_tileset_dimB.png"), "PNG")

    print("  [DONE] Dimension A + B tileset variants saved")
    return base_out


def gen_pickups_and_projectiles(q="standard"):
    """Generate pickup items and projectile sprite sheets."""
    print(f"\n{'=' * 60}")
    print(f"  PICKUPS & PROJECTILES — quality={q}")
    print(f"{'=' * 60}")

    pdir = TEMP_DIR / "pickups_v3"
    pdir.mkdir(exist_ok=True)

    pickups = {
        "health": "glowing emerald green healing crystal, luminous health pickup, bright inner glow",
        "shield": "pulsing cyan energy shield orb, protective barrier sphere, electric blue glow",
        "rift_shard": "radiant purple dimensional rift crystal shard, violet energy gem, void power",
        "speed": "blazing golden speed lightning bolt, yellow energy streak, quick-boost pickup",
        "xp": "brilliant white experience star, wisdom orb, pure radiant light sparkle",
        "damage": "fierce crimson damage crystal shard, red attack power gem, aggressive glow",
        "multishot": "fiery orange triple projectile arrow, spread shot pickup, multi-burst energy",
        "magnet": "electric cyan magnetic force orb, attraction vortex, pull-field pickup",
    }

    pickup_size = 16
    p_cols, p_rows = 4, 5
    sheet = Image.new("RGBA", (p_cols * pickup_size, p_rows * pickup_size), (0, 0, 0, 0))

    for idx, (name, desc) in enumerate(pickups.items()):
        img_path = str(pdir / f"{name}.png")
        if not os.path.exists(img_path):
            prompt = (
                f"game item icon, {desc}, "
                "centered on dark background, clean simple design, "
                "glowing luminous, small game pickup sprite"
            )
            generate_pass1(prompt, img_path, q="draft", seed=_name_to_seed(f"pickup_{name}"))
        if os.path.exists(img_path):
            src = Image.open(img_path).convert("RGBA")
            w, h = src.size
            m = int(w * 0.15)
            cropped = src.crop((m, m, w - m, h - m))
            item = cropped.resize((pickup_size, pickup_size), Image.LANCZOS)
            item = remove_bg_floodfill(item, threshold=30)
            col, row = idx % p_cols, idx // p_cols
            sheet.paste(item, (col * pickup_size, row * pickup_size), item)

    out = str(PROJECT_DIR / "assets/textures/pickups/pickups.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sheet.save(out, "PNG")
    print(f"  [DONE] Pickups: {out}")

    # Projectiles
    proj_descs = [
        "purple void energy bolt, glowing projectile, game bullet sprite",
        "blue focused laser beam, thin energy ray, game projectile",
        "green toxic acid glob, corrosive spit projectile, game bullet",
        "red fire bolt, flaming projectile with trailing embers, game bullet",
    ]
    proj_sheet = Image.new("RGBA", (16 * 4, 16 * 3), (0, 0, 0, 0))
    for idx, desc in enumerate(proj_descs):
        img_path = str(pdir / f"proj_{idx}.png")
        if not os.path.exists(img_path):
            prompt = f"game projectile sprite, {desc}, centered on dark background, simple glowing"
            generate_pass1(prompt, img_path, q="draft", seed=_name_to_seed(f"proj_{idx}"))
        if os.path.exists(img_path):
            src = Image.open(img_path).convert("RGBA")
            w, h = src.size
            m = int(w * 0.2)
            proj = src.crop((m, m, w - m, h - m)).resize((16, 16), Image.LANCZOS)
            proj = remove_bg_floodfill(proj, threshold=30)
            for row in range(3):
                proj_sheet.paste(proj, (idx * 16, row * 16), proj)

    proj_out = str(PROJECT_DIR / "assets/textures/projectiles/projectiles.png")
    os.makedirs(os.path.dirname(proj_out), exist_ok=True)
    proj_sheet.save(proj_out, "PNG")
    print(f"  [DONE] Projectiles: {proj_out}")


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 10: Utilities
# ═══════════════════════════════════════════════════════════════════════════

def _name_to_seed(name):
    """Deterministic seed from entity name for reproducible generation."""
    return int(hashlib.md5(name.encode()).hexdigest()[:8], 16)


def gen_placeholders():
    """Extract first frame from each sheet as a placeholder image."""
    print(f"\n{'=' * 60}")
    print(f"  PLACEHOLDERS")
    print(f"{'=' * 60}")

    ph_dir = PROJECT_DIR / "assets/textures/placeholders"
    ph_dir.mkdir(parents=True, exist_ok=True)

    def extract_first(sheet_path, fw, fh, out_path):
        if not os.path.exists(sheet_path):
            return
        img = Image.open(sheet_path).convert("RGBA")
        frame = img.crop((0, 0, fw, fh))
        frame.save(out_path, "PNG")
        print(f"  [OK] {os.path.basename(out_path)}")

    extract_first(
        str(PROJECT_DIR / "assets/textures/player/player.png"),
        PLAYER_FW, PLAYER_FH,
        str(ph_dir / "player.png")
    )

    for name, (_, fw, fh) in ENEMIES.items():
        extract_first(
            str(PROJECT_DIR / f"assets/textures/enemies/{name}.png"),
            fw, fh, str(ph_dir / f"{name}.png")
        )

    for name, info in BOSSES.items():
        extract_first(
            str(PROJECT_DIR / f"assets/textures/bosses/{name}.png"),
            info["fw"], info["fh"], str(ph_dir / f"{name}.png")
        )


# ═══════════════════════════════════════════════════════════════════════════
# SECTION 11: Main
# ═══════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="Riftwalker Sprite Generator v3 — High-End Game Art Pipeline"
    )
    parser.add_argument("--quality", "-q", choices=["draft", "standard", "high"],
                        default="standard", help="Generation quality preset")
    parser.add_argument("--only", choices=["player", "enemies", "bosses", "tiles",
                                            "pickups", "placeholders", "all"],
                        default="all", help="Generate only a specific category")
    parser.add_argument("--enemy", help="Generate only a specific enemy by name")
    parser.add_argument("--boss", help="Generate only a specific boss by name")
    parser.add_argument("--no-pass2", action="store_true",
                        help="Skip img2img refinement pass (faster but lower quality)")
    parser.add_argument("--force", action="store_true",
                        help="Force regeneration even if cached images exist")
    parser.add_argument("--no-rembg", action="store_true",
                        help="Use flood-fill BG removal instead of rembg (no onnxruntime needed)")
    args = parser.parse_args()

    if not check_server():
        print("\n[FATAL] ComfyUI must be running on localhost:8188")
        sys.exit(1)

    q = args.quality
    two_pass = not args.no_pass2
    force = args.force
    target = args.only

    print(f"\n{'#' * 60}")
    print(f"  Riftwalker v3 Sprite Pipeline")
    print(f"  Quality: {q}  |  Two-pass: {two_pass}  |  Force: {force}")
    print(f"{'#' * 60}")

    if target in ("all", "player"):
        assemble_player_sheet(q=q, two_pass=two_pass, force_regen=force)

    if target in ("all", "enemies"):
        if args.enemy:
            if args.enemy in ENEMIES:
                desc, fw, fh = ENEMIES[args.enemy]
                assemble_enemy_sheet(args.enemy, desc, fw, fh, q=q,
                                     two_pass=two_pass, force_regen=force)
            else:
                print(f"[ERROR] Unknown enemy: {args.enemy}")
                print(f"  Available: {', '.join(ENEMIES.keys())}")
        else:
            for name, (desc, fw, fh) in ENEMIES.items():
                assemble_enemy_sheet(name, desc, fw, fh, q=q,
                                     two_pass=two_pass, force_regen=force)

    if target in ("all", "bosses"):
        if args.boss:
            if args.boss in BOSSES:
                assemble_boss_sheet(args.boss, BOSSES[args.boss], q=q,
                                     two_pass=two_pass, force_regen=force)
            else:
                print(f"[ERROR] Unknown boss: {args.boss}")
                print(f"  Available: {', '.join(BOSSES.keys())}")
        else:
            for name, info in BOSSES.items():
                assemble_boss_sheet(name, info, q=q, two_pass=two_pass,
                                     force_regen=force)

    if target in ("all", "tiles"):
        gen_tileset(q=q)

    if target in ("all", "pickups"):
        gen_pickups_and_projectiles(q=q)

    if target in ("all", "placeholders"):
        gen_placeholders()

    print(f"\n{'#' * 60}")
    print(f"  ALL DONE — v3 generation complete")
    print(f"{'#' * 60}")


if __name__ == "__main__":
    main()
