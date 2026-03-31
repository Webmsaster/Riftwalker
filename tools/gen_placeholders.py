"""Generate placeholder fallback sprites from the main sprites.
Placeholders are simpler/smaller versions for fallback rendering."""
import os
from PIL import Image

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(PROJECT, "assets", "textures")
PLACEHOLDERS = os.path.join(ASSETS, "placeholders")

def extract_first_frame(sheet_path, frame_w, frame_h, output_path):
    """Extract the first frame from a sprite sheet as a placeholder."""
    if not os.path.exists(sheet_path):
        print(f"  SKIP (no sheet): {sheet_path}")
        return False

    img = Image.open(sheet_path).convert("RGBA")
    frame = img.crop((0, 0, frame_w, frame_h))
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    frame.save(output_path, "PNG")
    print(f"  OK: {output_path} ({frame_w}x{frame_h})")
    return True

def main():
    print("Generating placeholder sprites from main sprite sheets...")

    # Player placeholder (32x48)
    extract_first_frame(
        os.path.join(ASSETS, "player", "player.png"),
        32, 48,
        os.path.join(PLACEHOLDERS, "player.png")
    )

    # Enemy placeholders (32x32, except crawler=32x24, swarmer=16x16)
    enemies = {
        "walker": (32, 32), "flyer": (32, 32), "turret": (32, 32),
        "charger": (32, 32), "phaser": (32, 32), "exploder": (32, 32),
        "shielder": (32, 32), "crawler": (32, 24), "summoner": (32, 32),
        "sniper": (32, 32), "teleporter": (32, 32), "reflector": (32, 32),
        "leech": (32, 32), "swarmer": (16, 16), "gravitywell": (32, 32),
        "mimic": (32, 32), "minion": (32, 32)
    }

    for name, (fw, fh) in enemies.items():
        extract_first_frame(
            os.path.join(ASSETS, "enemies", f"{name}.png"),
            fw, fh,
            os.path.join(PLACEHOLDERS, f"{name}.png")
        )

    # Boss placeholders (64x64, except void_sovereign=96x96)
    bosses = {
        "rift_guardian": 64, "void_wyrm": 64, "dimensional_architect": 64,
        "temporal_weaver": 64, "void_sovereign": 96, "entropy_incarnate": 64
    }

    for name, size in bosses.items():
        extract_first_frame(
            os.path.join(ASSETS, "bosses", f"{name}.png"),
            size, size,
            os.path.join(PLACEHOLDERS, f"{name}.png")
        )

    # Tile placeholder (16x16)
    extract_first_frame(
        os.path.join(ASSETS, "tiles", "tileset_universal.png"),
        16, 16,
        os.path.join(PLACEHOLDERS, "tile_solid.png")
    )

    # Exit and rift placeholders (32x32) - from pickups
    pickups_path = os.path.join(ASSETS, "pickups", "pickups.png")
    if os.path.exists(pickups_path):
        img = Image.open(pickups_path).convert("RGBA")
        # Use rift shard as exit/rift placeholder
        rift_icon = img.crop((32, 0, 48, 16))  # 3rd column, 1st row
        rift_scaled = rift_icon.resize((32, 32), Image.LANCZOS)
        rift_scaled.save(os.path.join(PLACEHOLDERS, "exit.png"), "PNG")
        rift_scaled.save(os.path.join(PLACEHOLDERS, "rift.png"), "PNG")
        print(f"  OK: exit.png, rift.png (32x32)")

    print("\nAll placeholders generated!")

if __name__ == "__main__":
    main()
