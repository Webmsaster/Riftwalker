#!/usr/bin/env python3
"""
Riftwalker Sprite Generator - Master Script
Generates all pixel art spritesheets for the game.

Usage:
    cd tools/sprite_generator
    python generate_all.py
"""
import os
import sys
import time

# Ensure we can import sibling modules
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import OUTPUT


def ensure_dirs():
    """Create all output directories."""
    for name, path in OUTPUT.items():
        os.makedirs(path, exist_ok=True)
        print(f"  Directory ready: {path}")


def main():
    start = time.time()
    print("=" * 50)
    print("Riftwalker Sprite Generator")
    print("=" * 50)
    print()

    print("[1/6] Creating output directories...")
    ensure_dirs()
    print()

    print("[2/6] Generating player sprites...")
    from player_sprites import generate_player
    generate_player()
    print()

    print("[3/6] Generating enemy sprites...")
    from enemy_sprites import generate_enemies
    generate_enemies()
    print()

    print("[4/6] Generating boss sprites...")
    from boss_sprites import generate_bosses
    generate_bosses()
    print()

    print("[5/6] Generating tile sprites...")
    from tile_sprites import generate_tiles
    generate_tiles()
    print()

    print("[6/6] Generating pickup & projectile sprites...")
    from pickup_sprites import generate_pickups
    from projectile_sprites import generate_projectiles
    generate_pickups()
    generate_projectiles()
    print()

    elapsed = time.time() - start
    print("=" * 50)
    print(f"All sprites generated in {elapsed:.1f}s")
    print("=" * 50)


if __name__ == '__main__':
    main()
