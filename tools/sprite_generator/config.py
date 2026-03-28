"""
Riftwalker Sprite Generator Configuration
Frame sizes, animation definitions, and output paths.
"""
import os

# Base paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
ASSETS_DIR = os.path.join(PROJECT_DIR, 'assets', 'textures')

# Output directories
OUTPUT = {
    'player': os.path.join(ASSETS_DIR, 'player'),
    'enemies': os.path.join(ASSETS_DIR, 'enemies'),
    'bosses': os.path.join(ASSETS_DIR, 'bosses'),
    'tiles': os.path.join(ASSETS_DIR, 'tiles'),
    'pickups': os.path.join(ASSETS_DIR, 'pickups'),
    'projectiles': os.path.join(ASSETS_DIR, 'projectiles'),
}

# Frame sizes
TILE_SIZE = 32
PLAYER_FRAME = (32, 48)
ENEMY_FRAME = (32, 32)
BOSS_FRAME = (64, 64)
BOSS_LARGE_FRAME = (96, 96)  # Void Sovereign
PICKUP_FRAME = (16, 16)
PROJECTILE_FRAME = (16, 16)

# Player animations: (name, row, frame_count, loop)
PLAYER_ANIMS = [
    ('idle',       0, 6, True),
    ('run',        1, 8, True),
    ('jump',       2, 3, False),
    ('fall',       3, 3, True),
    ('dash',       4, 4, False),
    ('wallslide',  5, 3, True),
    ('attack',     6, 6, False),
    ('hurt',       7, 3, False),
    ('dead',       8, 5, False),
]

# Enemy animations: (name, row, frame_count, loop)
ENEMY_ANIMS = [
    ('idle',    0, 4, True),
    ('walk',    1, 6, True),
    ('attack',  2, 4, False),
    ('hurt',    3, 2, False),
    ('dead',    4, 4, False),
]

# Boss animations: (name, row, frame_count, loop)
BOSS_ANIMS = [
    ('idle',             0, 6, True),
    ('move',             1, 6, True),
    ('attack1',          2, 6, False),
    ('attack2',          3, 6, False),
    ('attack3',          4, 6, False),
    ('phase_transition', 5, 8, False),
    ('hurt',             6, 3, False),
    ('enrage',           7, 6, False),
    ('dead',             8, 8, False),
]

# Enemy types with their specific frame sizes
ENEMY_TYPES = [
    'walker', 'flyer', 'turret', 'charger', 'phaser',
    'exploder', 'shielder', 'crawler', 'summoner', 'sniper', 'minion',
    'teleporter', 'reflector', 'leech', 'swarmer', 'gravitywell', 'mimic',
]

# Boss types with their sizes
BOSS_TYPES = {
    'rift_guardian':         BOSS_FRAME,
    'void_wyrm':            BOSS_FRAME,
    'dimensional_architect': BOSS_FRAME,
    'temporal_weaver':       BOSS_FRAME,
    'void_sovereign':        BOSS_LARGE_FRAME,
    'entropy_incarnate':     BOSS_FRAME,
}

# Tile types for the universal tileset
# Row 0: 16 auto-tile solid variants (4-bit neighbor mask)
# Row 1: Special tiles
TILE_TYPES = [
    'solid',       # Row 0: 16 auto-tile variants
    'oneway',      # Row 1, col 0
    'spike',       # Row 1, col 1
    'ice',         # Row 1, col 2
    'breakable',   # Row 1, col 3
    'fire',        # Row 1, col 4-7 (4 frames animation)
    'conveyor',    # Row 2, col 0-3 (4 frames, right)
    'conveyor_l',  # Row 2, col 4-7 (4 frames, left)
    'laser',       # Row 3, col 0-3 (4 directions)
    'teleporter',  # Row 3, col 4-7 (4 frames animation)
    'crumbling',   # Row 4, col 0-3 (4 stages)
    'gravity',     # Row 4, col 4-5 (attract/repel)
    'decoration',  # Row 5, col 0-7 (various decorations)
]
