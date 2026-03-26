"""Record Riftwalker gameplay as video using FFmpeg.

Usage:
    python tools/record_gameplay.py                    # Record 60s
    python tools/record_gameplay.py --duration 120     # Record 2 minutes
    python tools/record_gameplay.py --output my_run.mp4
    python tools/record_gameplay.py --fps 30

Requires FFmpeg in PATH. Uses gdigrab to capture the Riftwalker window.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time


def find_ffmpeg():
    """Find ffmpeg executable."""
    path = shutil.which("ffmpeg")
    if path:
        return path
    # Common Windows locations
    for p in [r"C:\ffmpeg\bin\ffmpeg.exe", r"C:\Program Files\ffmpeg\bin\ffmpeg.exe"]:
        if os.path.exists(p):
            return p
    return None


def is_game_running():
    """Check if Riftwalker is running."""
    try:
        result = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq Riftwalker.exe"],
            capture_output=True, text=True
        )
        return "Riftwalker.exe" in result.stdout
    except Exception:
        return False


def record(output="gameplay.mp4", duration=60, fps=30):
    ffmpeg = find_ffmpeg()
    if not ffmpeg:
        print("ERROR: FFmpeg not found. Install from https://ffmpeg.org/download.html")
        print("  or: winget install Gyan.FFmpeg")
        sys.exit(1)

    if not is_game_running():
        print("WARNING: Riftwalker is not running. Starting capture anyway...")

    os.makedirs(os.path.dirname(output) or ".", exist_ok=True)

    cmd = [
        ffmpeg,
        "-y",                          # Overwrite output
        "-f", "gdigrab",               # Windows screen capture
        "-framerate", str(fps),
        "-i", "title=Riftwalker",      # Capture by window title
        "-t", str(duration),           # Duration in seconds
        "-c:v", "libx264",             # H.264 encoding
        "-preset", "ultrafast",        # Fast encoding (less CPU)
        "-crf", "23",                  # Quality (lower = better, 18-28 typical)
        "-pix_fmt", "yuv420p",         # Compatibility
        output
    ]

    print(f"Recording Riftwalker window for {duration}s at {fps}fps...")
    print(f"Output: {output}")
    print("Press Ctrl+C to stop early.\n")

    try:
        proc = subprocess.run(cmd, timeout=duration + 10)
        if proc.returncode == 0:
            size_mb = os.path.getsize(output) / (1024 * 1024)
            print(f"\nDone: {output} ({size_mb:.1f} MB)")
        else:
            print(f"\nFFmpeg exited with code {proc.returncode}")
            if not is_game_running():
                print("Riftwalker window not found — is the game running?")
    except KeyboardInterrupt:
        print("\nRecording stopped by user.")
    except subprocess.TimeoutExpired:
        print("\nRecording timed out.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record Riftwalker gameplay")
    parser.add_argument("--output", "-o", default="recordings/gameplay.mp4")
    parser.add_argument("--duration", "-d", type=int, default=60)
    parser.add_argument("--fps", type=int, default=30)
    args = parser.parse_args()
    record(args.output, args.duration, args.fps)
