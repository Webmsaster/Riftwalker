"""GrabCut-based background removal for stubborn sprites with similar bg/fg colors."""
import cv2
import numpy as np
import os


def grabcut_frame(frame):
    """Use GrabCut to separate foreground from background."""
    h, w = frame.shape[:2]
    alpha = frame[:, :, 3]

    # Skip if already mostly transparent
    if np.sum(alpha == 0) > 0.20 * alpha.size:
        return frame, False

    # Skip empty
    if np.sum(alpha > 0) < 0.05 * alpha.size:
        return frame, False

    bgr = frame[:, :, :3].copy()

    # Initial rectangle for GrabCut: exclude thin edge margin
    margin = max(4, min(w, h) // 16)
    rect = (margin, margin, w - 2 * margin, h - 2 * margin)

    mask = np.zeros((h, w), np.uint8)
    # Mark edges as definite background
    mask[:margin, :] = cv2.GC_BGD
    mask[-margin:, :] = cv2.GC_BGD
    mask[:, :margin] = cv2.GC_BGD
    mask[:, -margin:] = cv2.GC_BGD
    # Mark center as probable foreground
    center_m = max(margin * 2, min(w, h) // 4)
    mask[center_m:-center_m, center_m:-center_m] = cv2.GC_PR_FGD
    # Rest is probable background
    remaining = mask == 0
    mask[remaining] = cv2.GC_PR_BGD

    bgdModel = np.zeros((1, 65), np.float64)
    fgdModel = np.zeros((1, 65), np.float64)

    try:
        cv2.grabCut(bgr, mask, rect, bgdModel, fgdModel, 5, cv2.GC_INIT_WITH_MASK)
    except cv2.error:
        return frame, False

    # Foreground = definite FG or probable FG
    fg_mask = ((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD)).astype(np.uint8)

    # Smooth the mask edges
    fg_mask_smooth = cv2.GaussianBlur(fg_mask.astype(np.float32) * 255, (5, 5), 1.5)
    new_alpha = np.clip(fg_mask_smooth, 0, 255).astype(np.uint8)

    # Check we didn't remove too much (sprite should be at least 10% of frame)
    if np.sum(new_alpha > 128) < 0.10 * alpha.size:
        return frame, False  # GrabCut removed too much, skip

    # Check we actually improved (more transparency than before)
    old_transparent = np.sum(alpha == 0)
    new_transparent = np.sum(new_alpha < 30)
    if new_transparent <= old_transparent * 1.1:
        return frame, False  # Didn't improve enough

    result = frame.copy()
    result[:, :, 3] = new_alpha
    return result, True


def process_sheet(path, fw, fh):
    """Process sprite sheet with GrabCut."""
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
            result, was_fixed = grabcut_frame(frame)
            if was_fixed:
                img[y1:y2, x1:x2] = result
                fixed += 1

    if fixed > 0:
        cv2.imwrite(path, img)
    return fixed


def main():
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

    print("=== GrabCut Background Removal (stubborn sprites) ===\n")

    # Only target sprites with known stubborn backgrounds
    targets = [
        ('assets/textures/enemies/walker.png', 128, 128),
        ('assets/textures/enemies/sniper.png', 128, 128),
        ('assets/textures/enemies/charger.png', 128, 128),
        ('assets/textures/enemies/flyer.png', 128, 128),
    ]

    total = 0
    for path, fw, fh in targets:
        if not os.path.exists(path):
            continue
        fixed = process_sheet(path, fw, fh)
        total += fixed
        print(f'  {os.path.basename(path)}: {fixed} frames fixed')

    print(f'\nTotal: {total} frames improved via GrabCut')


if __name__ == '__main__':
    main()
