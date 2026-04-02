#!/usr/bin/env python3
"""Extract the best single frame from each Riftwalker spritesheet.

Analyzes all frames in a spritesheet, scores them by:
- Non-transparent pixel count (more = better)
- Centering quality (closer to center = better)
- Bounding box compactness

Saves best frame as single PNG in assets/textures/singles/ directory.
"""
import os
import sys
from PIL import Image
import numpy as np

ASSETS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      "assets", "textures")
OUT_DIR = os.path.join(ASSETS, "singles")

# Spritesheet layouts: (frame_w, frame_h, cols, rows)
SHEETS = {
    "player/player.png":                (128, 192, 8, 9),
    "enemies/walker.png":               (128, 128, 6, 5),
    "enemies/flyer.png":                (128, 128, 6, 5),
    "enemies/turret.png":               (128, 128, 6, 5),
    "enemies/charger.png":              (128, 128, 6, 5),
    "enemies/phaser.png":               (128, 128, 6, 5),
    "enemies/exploder.png":             (128, 128, 6, 5),
    "enemies/shielder.png":             (128, 128, 6, 5),
    "enemies/crawler.png":              (128, 128, 6, 5),
    "enemies/summoner.png":             (128, 128, 6, 5),
    "enemies/sniper.png":               (128, 128, 6, 5),
    "enemies/teleporter.png":           (128, 128, 6, 5),
    "enemies/reflector.png":            (128, 128, 6, 5),
    "enemies/leech.png":                (128, 128, 6, 5),
    "enemies/swarmer.png":              (128, 128, 6, 5),
    "enemies/gravitywell.png":          (128, 128, 6, 5),
    "enemies/mimic.png":                (128, 128, 6, 5),
    "enemies/minion.png":               (128, 128, 6, 5),
    "bosses/rift_guardian.png":         (256, 256, 8, 9),
    "bosses/void_wyrm.png":            (256, 256, 8, 9),
    "bosses/dimensional_architect.png": (256, 256, 8, 9),
    "bosses/temporal_weaver.png":       (256, 256, 8, 9),
    "bosses/entropy_incarnate.png":     (256, 256, 8, 9),
    "bosses/void_sovereign.png":        (384, 384, 8, 9),
}

# Prefer idle row (row 0) frames — they're the most "neutral" pose
PREFERRED_ROW = 0


def score_frame(frame_img):
    """Score a frame: higher = better. Returns (score, details)."""
    arr = np.array(frame_img)
    if arr.shape[2] < 4:
        return 0, "no alpha"

    alpha = arr[:, :, 3]
    visible = alpha > 20  # Pixels with some opacity
    count = np.sum(visible)

    if count < 10:
        return 0, "empty"

    h, w = alpha.shape

    # Find bounding box of visible content
    rows_any = np.any(visible, axis=1)
    cols_any = np.any(visible, axis=0)
    if not np.any(rows_any) or not np.any(cols_any):
        return 0, "empty"

    ymin = np.argmax(rows_any)
    ymax = h - np.argmax(rows_any[::-1]) - 1
    xmin = np.argmax(cols_any)
    xmax = w - np.argmax(cols_any[::-1]) - 1

    bbox_w = xmax - xmin + 1
    bbox_h = ymax - ymin + 1
    bbox_area = bbox_w * bbox_h

    # Score components:
    # 1. Coverage: more visible pixels = better (weight 3)
    coverage = count / (w * h)

    # 2. Centering: content centered in frame = better (weight 2)
    cx = (xmin + xmax) / 2
    cy = (ymin + ymax) / 2
    center_dist = ((cx - w / 2) ** 2 + (cy - h / 2) ** 2) ** 0.5
    max_dist = ((w / 2) ** 2 + (h / 2) ** 2) ** 0.5
    centering = 1.0 - (center_dist / max_dist)

    # 3. Compactness: tight bounding box = better (weight 1)
    compactness = count / max(bbox_area, 1)

    # 4. Average opacity of visible pixels: more opaque = better (weight 1)
    avg_alpha = np.mean(alpha[visible]) / 255.0

    score = coverage * 3.0 + centering * 2.0 + compactness * 1.0 + avg_alpha * 1.0
    return score, f"cov={coverage:.2f} cen={centering:.2f} comp={compactness:.2f} alpha={avg_alpha:.2f}"


def extract_best(rel_path, fw, fh, cols, rows):
    """Extract the best frame from a spritesheet."""
    full = os.path.join(ASSETS, rel_path)
    if not os.path.exists(full):
        print(f"  [--] {rel_path}: not found")
        return None

    img = Image.open(full).convert("RGBA")
    entity_name = os.path.splitext(os.path.basename(rel_path))[0]
    entity_type = rel_path.split("/")[0]  # player, enemies, bosses

    best_score = -1
    best_frame = None
    best_info = ""

    # First pass: check preferred row (idle, row 0) for good frames
    # Second pass: check all rows if idle row has no good frames
    for priority_pass in range(2):
        for row in range(rows):
            if priority_pass == 0 and row != PREFERRED_ROW:
                continue
            if priority_pass == 1 and row == PREFERRED_ROW:
                continue  # Already checked

            for col in range(cols):
                x = col * fw
                y = row * fh
                if x + fw > img.width or y + fh > img.height:
                    continue

                frame = img.crop((x, y, x + fw, y + fh))
                score, info = score_frame(frame)

                # Bonus for idle row (preferred neutral pose)
                if row == PREFERRED_ROW:
                    score *= 1.3

                if score > best_score:
                    best_score = score
                    best_frame = frame
                    best_info = f"row={row} col={col} score={score:.2f} ({info})"

        # If we found a good frame in preferred row, stop
        if priority_pass == 0 and best_score > 1.0:
            break

    if best_frame is None or best_score < 0.1:
        print(f"  [!!] {rel_path}: no usable frame found")
        return None

    # Determine output subdirectory
    out_subdir = os.path.join(OUT_DIR, entity_type)
    os.makedirs(out_subdir, exist_ok=True)
    out_path = os.path.join(out_subdir, f"{entity_name}.png")

    best_frame.save(out_path)
    print(f"  [OK] {rel_path} -> singles/{entity_type}/{entity_name}.png ({fw}x{fh}) [{best_info}]")
    return out_path


def main():
    print("=== Riftwalker Best Frame Extractor ===")
    print(f"Output: {OUT_DIR}")
    print()

    os.makedirs(OUT_DIR, exist_ok=True)
    extracted = 0

    for rel_path, (fw, fh, cols, rows) in sorted(SHEETS.items()):
        result = extract_best(rel_path, fw, fh, cols, rows)
        if result:
            extracted += 1

    print()
    print(f"Extracted {extracted}/{len(SHEETS)} best frames to assets/textures/singles/")


if __name__ == "__main__":
    main()
