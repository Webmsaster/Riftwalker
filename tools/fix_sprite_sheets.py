#!/usr/bin/env python3
"""Fix and validate Riftwalker sprite sheets against expected dimensions.

Checks all entity spritesheets match the frame layout SpriteConfig.cpp expects.
Auto-fixes mismatches by padding (if too small) or resizing (if wrong scale).
"""
import os
import sys
import shutil
from PIL import Image

ASSETS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      "assets", "textures")

# Expected layouts: (frame_w, frame_h, cols, rows)
EXPECTED = {
    "player/player.png":   (128, 192, 8, 9),
    # Standard enemies: 128x128, 6 cols, 5 rows
    "enemies/walker.png":      (128, 128, 6, 5),
    "enemies/flyer.png":       (128, 128, 6, 5),
    "enemies/turret.png":      (128, 128, 6, 5),
    "enemies/charger.png":     (128, 128, 6, 5),
    "enemies/phaser.png":      (128, 128, 6, 5),
    "enemies/exploder.png":    (128, 128, 6, 5),
    "enemies/shielder.png":    (128, 128, 6, 5),
    "enemies/crawler.png":     (128, 128, 6, 5),
    "enemies/summoner.png":    (128, 128, 6, 5),
    "enemies/sniper.png":      (128, 128, 6, 5),
    "enemies/teleporter.png":  (128, 128, 6, 5),
    "enemies/reflector.png":   (128, 128, 6, 5),
    "enemies/leech.png":       (128, 128, 6, 5),
    "enemies/swarmer.png":     (128, 128, 6, 5),
    "enemies/gravitywell.png": (128, 128, 6, 5),
    "enemies/mimic.png":       (128, 128, 6, 5),
    "enemies/minion.png":      (128, 128, 6, 5),
    # Standard bosses: 256x256, 8 cols, 9 rows
    "bosses/rift_guardian.png":         (256, 256, 8, 9),
    "bosses/void_wyrm.png":            (256, 256, 8, 9),
    "bosses/dimensional_architect.png": (256, 256, 8, 9),
    "bosses/temporal_weaver.png":       (256, 256, 8, 9),
    "bosses/entropy_incarnate.png":     (256, 256, 8, 9),
    # Void Sovereign: 384x384, 8 cols, 9 rows
    "bosses/void_sovereign.png":        (384, 384, 8, 9),
    # Pickups: 64x64, 4 cols, 5 rows
    "pickups/pickups.png":              (64, 64, 4, 5),
}


def check_sprite(rel_path, fw, fh, cols, rows):
    """Check a single spritesheet. Returns (status, message, fix_needed)."""
    full = os.path.join(ASSETS, rel_path)
    if not os.path.exists(full):
        return "MISSING", f"{rel_path}: file not found", False

    img = Image.open(full)
    exp_w = fw * cols
    exp_h = fh * rows

    if img.width == exp_w and img.height == exp_h:
        return "OK", f"{rel_path}: {img.width}x{img.height} [OK]", False

    actual_fw = img.width / cols
    actual_fh = img.height / rows
    msg = (f"{rel_path}: {img.width}x{img.height} "
           f"(expected {exp_w}x{exp_h}, "
           f"frame {actual_fw:.0f}x{actual_fh:.0f} vs {fw}x{fh})")
    return "MISMATCH", msg, True


def fix_sprite(rel_path, fw, fh, cols, rows):
    """Fix a mismatched spritesheet by re-laying out frames."""
    full = os.path.join(ASSETS, rel_path)
    img = Image.open(full).convert("RGBA")

    exp_w = fw * cols
    exp_h = fh * rows
    actual_fw = img.width // cols
    actual_fh = img.height // rows

    # Backup original
    backup = full + ".bak"
    if not os.path.exists(backup):
        shutil.copy2(full, backup)
        print(f"  Backed up to {os.path.basename(backup)}")

    # Check if simple integer scale factor works
    scale_x = fw / actual_fw
    scale_y = fh / actual_fh
    if scale_x == scale_y and scale_x == int(scale_x):
        # Simple integer upscale (e.g., swarmer 64->128 = 2x)
        scale = int(scale_x)
        new_img = img.resize((img.width * scale, img.height * scale),
                             Image.LANCZOS)
        new_img.save(full)
        print(f"  Upscaled {scale}x: {img.width}x{img.height} -> {new_img.width}x{new_img.height}")
        return True

    # Different aspect: need to re-layout each frame into new grid
    # Extract each frame, center it in a new fw×fh cell
    new_img = Image.new("RGBA", (exp_w, exp_h), (0, 0, 0, 0))

    for row in range(rows):
        src_y = row * actual_fh
        if src_y >= img.height:
            break
        for col in range(cols):
            src_x = col * actual_fw
            if src_x >= img.width:
                break

            # Crop the source frame
            src_right = min(src_x + actual_fw, img.width)
            src_bottom = min(src_y + actual_fh, img.height)
            frame = img.crop((src_x, src_y, src_right, src_bottom))

            # If frame is smaller than expected, we might need to scale it
            frame_w = src_right - src_x
            frame_h = src_bottom - src_y

            # Center the frame in the target cell
            dst_x = col * fw + (fw - frame_w) // 2
            dst_y = row * fh + (fh - frame_h) // 2

            new_img.paste(frame, (dst_x, dst_y))

    new_img.save(full)
    print(f"  Re-laid out: {img.width}x{img.height} -> {new_img.width}x{new_img.height} "
          f"(frames centered {actual_fw:.0f}x{actual_fh:.0f} in {fw}x{fh} cells)")
    return True


def main():
    fix_mode = "--fix" in sys.argv
    print(f"=== Riftwalker Sprite Sheet Validator {'(FIX MODE)' if fix_mode else ''} ===")
    print()

    ok = 0
    mismatch = 0
    missing = 0
    fixed = 0

    for rel_path, (fw, fh, cols, rows) in sorted(EXPECTED.items()):
        status, msg, needs_fix = check_sprite(rel_path, fw, fh, cols, rows)
        if status == "OK":
            print(f"  [OK] {msg}")
            ok += 1
        elif status == "MISSING":
            print(f"  [--] {msg}")
            missing += 1
        else:
            print(f"  [!!] {msg}")
            mismatch += 1
            if fix_mode:
                if fix_sprite(rel_path, fw, fh, cols, rows):
                    fixed += 1

    print()
    print(f"Results: {ok} OK, {mismatch} mismatch, {missing} missing")
    if fix_mode and fixed > 0:
        print(f"Fixed: {fixed} spritesheets (originals backed up as .bak)")
    elif mismatch > 0 and not fix_mode:
        print("Run with --fix to auto-fix mismatched spritesheets")


if __name__ == "__main__":
    main()
