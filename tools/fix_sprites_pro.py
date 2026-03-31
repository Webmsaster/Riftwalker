"""Professional sprite cleanup: transparency fix, halo removal, edge smoothing."""
import cv2
import numpy as np
import glob
import os


def fix_frame_transparency(frame, tolerance=40):
    """Remove background from a single sprite frame using multi-method approach."""
    h, w = frame.shape[:2]
    alpha = frame[:, :, 3]

    # Skip frames that already have good transparency (>15% transparent)
    if np.sum(alpha == 0) > 0.15 * alpha.size:
        return frame, False

    # Skip completely empty/near-empty frames
    if np.sum(alpha > 0) < 0.01 * alpha.size:
        return frame, False

    # Check corners - if most are opaque, this frame needs fixing
    margin = 4
    corners = [
        frame[0:margin, 0:margin],
        frame[0:margin, -margin:],
        frame[-margin:, 0:margin],
        frame[-margin:, -margin:],
    ]
    corner_alphas = [c[:, :, 3].mean() for c in corners]
    opaque_corners = sum(1 for a in corner_alphas if a > 200)
    if opaque_corners < 3:
        return frame, False

    # Determine background color from corners
    corner_pixels = np.concatenate([c.reshape(-1, 4) for c in corners], axis=0)
    opaque_mask = corner_pixels[:, 3] > 200
    if opaque_mask.sum() < 4:
        return frame, False
    bg_color = np.median(corner_pixels[opaque_mask, :3], axis=0).astype(np.float32)

    # Method 1: Flood fill from edges
    bgr = frame[:, :, :3].copy()
    mask = np.zeros((h + 2, w + 2), np.uint8)
    tol = (tolerance, tolerance, tolerance, 0)
    seeds = [
        (1, 1), (w - 2, 1), (1, h - 2), (w - 2, h - 2),
        (w // 2, 0), (w // 2, h - 1), (0, h // 2), (w - 1, h // 2),
        (w // 4, 0), (3 * w // 4, 0), (w // 4, h - 1), (3 * w // 4, h - 1),
    ]
    for seed in seeds:
        if 0 <= seed[0] < w and 0 <= seed[1] < h:
            try:
                cv2.floodFill(bgr, mask, seed, (0, 0, 0), tol, tol,
                              cv2.FLOODFILL_MASK_ONLY | (255 << 8))
            except cv2.error:
                pass
    flood_bg = mask[1:-1, 1:-1] == 255

    # Method 2: Color distance from detected bg color
    pixel_colors = frame[:, :, :3].astype(np.float32)
    color_dist = np.sqrt(np.sum((pixel_colors - bg_color) ** 2, axis=2))
    color_bg = color_dist < (tolerance * 1.5)

    # Combine: pixel is background if flood fill says so OR color distance is very close
    # But only trust color_distance for pixels connected to edges
    bg_mask = flood_bg.astype(np.uint8) * 255

    # Also add color-similar pixels adjacent to flood-fill area
    dilated = cv2.dilate(bg_mask, np.ones((3, 3), np.uint8), iterations=2)
    extended = (dilated > 0) & color_bg
    bg_mask_final = flood_bg | extended

    # Apply transparency
    new_alpha = alpha.copy()
    new_alpha[bg_mask_final] = 0

    # Halo removal: reduce alpha for semi-transparent edge pixels near background
    kernel = np.ones((3, 3), np.uint8)
    bg_dilated = cv2.dilate(bg_mask_final.astype(np.uint8), kernel, iterations=2)
    fringe = (bg_dilated > 0) & (~bg_mask_final)

    # For fringe pixels, reduce alpha based on how close color is to bg
    fringe_dist = color_dist[fringe]
    max_fringe_dist = tolerance * 2.5
    fringe_alpha_factor = np.clip(fringe_dist / max_fringe_dist, 0.0, 1.0)
    current_alpha = new_alpha[fringe].astype(np.float32)
    new_alpha[fringe] = (current_alpha * fringe_alpha_factor).astype(np.uint8)

    # Premultiply alpha for clean edges (prevents color bleeding)
    result = frame.copy()
    result[:, :, 3] = new_alpha
    alpha_f = new_alpha.astype(np.float32) / 255.0
    for c in range(3):
        result[:, :, c] = (result[:, :, c].astype(np.float32) * alpha_f).astype(np.uint8)

    return result, True


def fix_sprite_sheet(path, fw, fh):
    """Process all frames in a sprite sheet."""
    img = cv2.imread(path, cv2.IMREAD_UNCHANGED)
    if img is None or len(img.shape) < 3 or img.shape[2] != 4:
        return 0

    h, w = img.shape[:2]
    cols = w // fw
    rows = h // fh
    fixed = 0

    for row in range(rows):
        for col in range(cols):
            y1, y2 = row * fh, (row + 1) * fh
            x1, x2 = col * fw, (col + 1) * fw
            if y2 > h or x2 > w:
                continue
            frame = img[y1:y2, x1:x2].copy()
            result, was_fixed = fix_frame_transparency(frame)
            if was_fixed:
                img[y1:y2, x1:x2] = result
                fixed += 1

    if fixed > 0:
        cv2.imwrite(path, img)
    return fixed


def main():
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    results = []

    print("=== Professional Sprite Cleanup ===\n")

    # Player: 128x192 frames
    fixed = fix_sprite_sheet('assets/textures/player/player.png', 128, 192)
    results.append(('player/player.png', fixed))

    # Enemies: 128x128 frames (special sizes for some)
    for f in sorted(glob.glob('assets/textures/enemies/*.png')):
        fw, fh = 128, 128
        name = os.path.basename(f)
        if 'swarmer' in name:
            fw, fh = 64, 64
        elif 'crawler' in name:
            fh = 96
        fixed = fix_sprite_sheet(f, fw, fh)
        results.append(('enemies/' + name, fixed))

    # Bosses: 256x256 frames
    for f in sorted(glob.glob('assets/textures/bosses/*.png')):
        fw, fh = 256, 256
        name = os.path.basename(f)
        if 'void_sovereign' in name:
            fw, fh = 384, 384
        fixed = fix_sprite_sheet(f, fw, fh)
        results.append(('bosses/' + name, fixed))

    # Pickups: 64x64
    fixed = fix_sprite_sheet('assets/textures/pickups/pickups.png', 64, 64)
    results.append(('pickups/pickups.png', fixed))

    # Projectiles
    proj = 'assets/textures/projectiles/projectiles.png'
    if os.path.exists(proj):
        img = cv2.imread(proj, cv2.IMREAD_UNCHANGED)
        if img is not None:
            ph = img.shape[0]
            pw = img.shape[1]
            # Guess frame size from image dimensions
            frame_size = ph  # assume square frames in a row
            fixed = fix_sprite_sheet(proj, frame_size, frame_size)
            results.append(('projectiles/projectiles.png', fixed))

    print("\nResults:")
    for name, count in results:
        status = f'{count} frames fixed' if count > 0 else 'OK (no fix needed)'
        print(f'  {name}: {status}')

    total = sum(c for _, c in results)
    print(f'\nTotal: {total} frames cleaned up')


if __name__ == '__main__':
    main()
