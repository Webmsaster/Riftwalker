"""Aggressive background removal for dark sprites on gray/white backgrounds.
Uses GrabCut-like approach + color distance + edge detection."""
import cv2
import numpy as np
import glob
import os


def detect_bg_color(frame):
    """Detect background color from edge pixels."""
    h, w = frame.shape[:2]
    margin = 6
    # Collect edge pixels (top/bottom/left/right strips)
    edges = np.concatenate([
        frame[0:margin, :].reshape(-1, 4),         # top
        frame[-margin:, :].reshape(-1, 4),          # bottom
        frame[:, 0:margin].reshape(-1, 4),          # left
        frame[:, -margin:].reshape(-1, 4),          # right
    ], axis=0)

    opaque = edges[edges[:, 3] > 200]
    if len(opaque) < 8:
        return None, False

    # Use median of opaque edge pixels as bg color
    bg_color = np.median(opaque[:, :3], axis=0)
    return bg_color, True


def remove_background_aggressive(frame, bg_color, sensitivity=1.0):
    """Remove background using color distance from detected bg color."""
    h, w = frame.shape[:2]
    alpha = frame[:, :, 3].copy()

    # Color distance from background
    pixel_colors = frame[:, :, :3].astype(np.float32)
    bg = bg_color.astype(np.float32)
    dist = np.sqrt(np.sum((pixel_colors - bg) ** 2, axis=2))

    # Threshold: pixels close to bg color
    # Gray backgrounds: tolerance ~60, white: ~50
    bg_brightness = np.mean(bg_color)
    if bg_brightness > 200:  # white-ish background
        threshold = 45 * sensitivity
    elif bg_brightness > 120:  # gray background
        threshold = 55 * sensitivity
    else:  # dark background
        threshold = 40 * sensitivity

    # Create initial bg mask from color distance
    potential_bg = dist < threshold

    # Flood fill from edges to find connected background regions
    # This prevents removing dark parts of the sprite that happen to be near bg color
    seed_mask = np.zeros((h, w), dtype=bool)
    margin = 3
    seed_mask[0:margin, :] = True
    seed_mask[-margin:, :] = True
    seed_mask[:, 0:margin] = True
    seed_mask[:, -margin:] = True

    # Connected component analysis: keep only bg regions connected to edges
    bg_candidates = potential_bg.astype(np.uint8) * 255
    edge_seeds = (seed_mask & potential_bg).astype(np.uint8) * 255

    # Use morphological operations to connect nearby bg regions
    kernel = np.ones((3, 3), np.uint8)
    bg_connected = cv2.dilate(edge_seeds, kernel, iterations=1)

    # Iteratively grow connected bg from edges
    for _ in range(max(h, w) // 3):
        new_connected = cv2.dilate(bg_connected, kernel, iterations=1)
        new_connected = new_connected & bg_candidates
        if np.array_equal(new_connected, bg_connected):
            break
        bg_connected = new_connected

    bg_mask = bg_connected > 0

    # Don't remove center region (likely the sprite)
    center_margin_x = w // 5
    center_margin_y = h // 5
    # Create a soft center weight
    y_coords, x_coords = np.mgrid[0:h, 0:w]
    center_dist = np.sqrt(((x_coords - w/2) / (w/2)) ** 2 + ((y_coords - h/2) / (h/2)) ** 2)

    # Pixels very close to center need higher confidence to be removed
    center_protection = center_dist < 0.5
    # Only remove center pixels if they're VERY close to bg color
    center_bg = dist < (threshold * 0.5)
    bg_mask[center_protection & ~center_bg] = False

    # Apply transparency
    new_alpha = alpha.copy()
    new_alpha[bg_mask] = 0

    # Anti-aliased edge: gradual alpha falloff at sprite boundaries
    bg_uint8 = bg_mask.astype(np.uint8) * 255
    edge_zone = cv2.dilate(bg_uint8, kernel, iterations=2)
    fringe = (edge_zone > 0) & (~bg_mask)

    fringe_colors = pixel_colors[fringe]
    fringe_dist = dist[fringe]
    alpha_factor = np.clip(fringe_dist / (threshold * 2.0), 0.0, 1.0)
    fringe_alpha = (alpha[fringe].astype(np.float32) * alpha_factor)
    new_alpha[fringe] = np.clip(fringe_alpha, 0, 255).astype(np.uint8)

    result = frame.copy()
    result[:, :, 3] = new_alpha
    return result


def process_sprite_sheet(path, fw, fh, sensitivity=1.0):
    """Process all frames, fix backgrounds."""
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
            alpha = frame[:, :, 3]

            # Skip already-transparent frames
            transparent_ratio = np.sum(alpha == 0) / alpha.size
            if transparent_ratio > 0.20:
                continue

            # Skip empty frames
            if np.sum(alpha > 0) < 0.01 * alpha.size:
                continue

            bg_color, has_bg = detect_bg_color(frame)
            if not has_bg:
                continue

            result = remove_background_aggressive(frame, bg_color, sensitivity)
            new_transparent = np.sum(result[:, :, 3] == 0) / alpha.size
            if new_transparent > transparent_ratio + 0.05:  # Must improve by at least 5%
                img[y1:y2, x1:x2] = result
                fixed += 1

    if fixed > 0:
        cv2.imwrite(path, img)
    return fixed


def main():
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

    print("=== Aggressive Background Removal ===\n")

    # Sprites with known gray/white background issues
    targets = [
        ('assets/textures/enemies/walker.png', 128, 128, 1.2),
        ('assets/textures/enemies/sniper.png', 128, 128, 1.1),
        ('assets/textures/enemies/charger.png', 128, 128, 1.0),
        ('assets/textures/enemies/flyer.png', 128, 128, 1.1),
        ('assets/textures/enemies/crawler.png', 128, 96, 1.0),
        ('assets/textures/enemies/exploder.png', 128, 128, 1.0),
        ('assets/textures/enemies/reflector.png', 128, 128, 1.0),
        ('assets/textures/enemies/summoner.png', 128, 128, 1.0),
        ('assets/textures/enemies/swarmer.png', 64, 64, 1.0),
        ('assets/textures/enemies/shielder.png', 128, 128, 1.0),
        ('assets/textures/enemies/turret.png', 128, 128, 1.0),
        ('assets/textures/enemies/phaser.png', 128, 128, 1.0),
        ('assets/textures/enemies/gravitywell.png', 128, 128, 1.0),
        ('assets/textures/enemies/leech.png', 128, 128, 1.0),
        ('assets/textures/enemies/mimic.png', 128, 128, 1.0),
        ('assets/textures/enemies/teleporter.png', 128, 128, 1.0),
        ('assets/textures/enemies/minion.png', 128, 128, 1.0),
        ('assets/textures/player/player.png', 128, 192, 1.0),
        ('assets/textures/bosses/rift_guardian.png', 256, 256, 0.9),
        ('assets/textures/bosses/void_wyrm.png', 256, 256, 0.9),
        ('assets/textures/bosses/temporal_weaver.png', 256, 256, 0.9),
        ('assets/textures/bosses/dimensional_architect.png', 256, 256, 0.9),
        ('assets/textures/bosses/entropy_incarnate.png', 256, 256, 0.9),
        ('assets/textures/bosses/void_sovereign.png', 384, 384, 0.9),
        ('assets/textures/pickups/pickups.png', 64, 64, 1.0),
    ]

    total = 0
    for path, fw, fh, sens in targets:
        if not os.path.exists(path):
            print(f'  SKIP {path} (not found)')
            continue
        fixed = process_sprite_sheet(path, fw, fh, sens)
        total += fixed
        status = f'{fixed} frames fixed' if fixed > 0 else 'clean'
        print(f'  {os.path.basename(path)}: {status}')

    print(f'\nTotal: {total} additional frames cleaned')


if __name__ == '__main__':
    main()
