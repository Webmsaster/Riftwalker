"""Screenshot tool for Riftwalker visual testing.

Usage:
    python tools/screenshot.py                  # Screenshot game window
    python tools/screenshot.py --all            # Full screen
    python tools/screenshot.py --output path.png  # Custom output path
"""
import argparse
import os
import sys
import time

import pyautogui
import pygetwindow as gw


def find_game_window():
    """Find the Riftwalker SDL2 window."""
    for w in gw.getAllWindows():
        if w.title == "Riftwalker" and w.width > 200:
            return w
    return None


def take_screenshot(output="screenshot.png", full_screen=False):
    win = find_game_window()
    if not win and not full_screen:
        print("ERROR: Riftwalker window not found. Is the game running?")
        sys.exit(1)

    os.makedirs(os.path.dirname(output) or ".", exist_ok=True)

    if full_screen or not win:
        img = pyautogui.screenshot(output)
    else:
        # Bring window to front
        try:
            win.activate()
            time.sleep(0.3)
        except Exception:
            pass
        x = max(0, win.left)
        y = max(0, win.top)
        img = pyautogui.screenshot(output, region=(x, y, win.width, win.height))

    print(f"OK: {output} ({img.size[0]}x{img.size[1]})")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Screenshot Riftwalker")
    parser.add_argument("--output", "-o", default="screenshot.png")
    parser.add_argument("--all", action="store_true", help="Full screen capture")
    args = parser.parse_args()
    take_screenshot(args.output, args.all)
