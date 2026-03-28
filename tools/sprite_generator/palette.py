"""
Riftwalker Pixel Art Color Palette
Consistent palette used across all sprite generators.
"""

# Transparent
TRANSPARENT = (0, 0, 0, 0)

# Universal outline
OUTLINE = (20, 12, 28, 255)
OUTLINE_LIGHT = (50, 40, 60, 255)

# Player colors (Sci-fi suit)
PLAYER = {
    'suit_dark': (30, 60, 120),
    'suit_mid': (50, 100, 180),
    'suit_light': (80, 140, 220),
    'suit_highlight': (130, 180, 240),
    'visor': (0, 220, 255),
    'visor_glow': (120, 240, 255),
    'skin': (220, 180, 150),
    'belt': (60, 60, 80),
    'boots': (40, 40, 55),
}

# Enemy base colors
ENEMY_COLORS = {
    'walker': {
        'dark': (120, 30, 30),
        'mid': (180, 50, 50),
        'light': (220, 80, 70),
        'highlight': (240, 130, 110),
        'eye': (255, 220, 60),
    },
    'flyer': {
        'dark': (90, 30, 120),
        'mid': (150, 60, 180),
        'light': (190, 100, 220),
        'highlight': (220, 150, 240),
        'eye': (255, 180, 255),
    },
    'turret': {
        'dark': (120, 110, 30),
        'mid': (180, 170, 50),
        'light': (220, 210, 80),
        'highlight': (240, 235, 130),
        'eye': (255, 60, 60),
    },
    'charger': {
        'dark': (140, 60, 15),
        'mid': (200, 100, 30),
        'light': (240, 140, 50),
        'highlight': (255, 190, 100),
        'eye': (255, 50, 30),
    },
    'phaser': {
        'dark': (50, 20, 100),
        'mid': (80, 40, 160),
        'light': (120, 70, 210),
        'highlight': (160, 120, 240),
        'eye': (200, 160, 255),
    },
    'exploder': {
        'dark': (160, 40, 10),
        'mid': (220, 70, 20),
        'light': (255, 120, 40),
        'highlight': (255, 180, 80),
        'core': (255, 240, 100),
    },
    'shielder': {
        'dark': (30, 90, 130),
        'mid': (60, 150, 200),
        'light': (100, 190, 230),
        'highlight': (160, 220, 245),
        'shield': (80, 200, 255),
    },
    'crawler': {
        'dark': (25, 80, 35),
        'mid': (50, 140, 65),
        'light': (80, 180, 95),
        'highlight': (130, 210, 140),
        'eye': (255, 80, 80),
    },
    'summoner': {
        'dark': (100, 20, 120),
        'mid': (160, 40, 190),
        'light': (200, 80, 230),
        'highlight': (230, 140, 250),
        'orb': (255, 200, 255),
    },
    'sniper': {
        'dark': (120, 100, 20),
        'mid': (180, 160, 40),
        'light': (220, 200, 70),
        'highlight': (245, 230, 130),
        'scope': (255, 60, 60),
    },
    'minion': {
        'dark': (120, 50, 150),
        'mid': (170, 85, 220),
        'light': (210, 130, 245),
        'highlight': (235, 180, 255),
        'eye': (255, 220, 255),
    },
    'teleporter': {
        'dark': (20, 60, 100),
        'mid': (40, 110, 180),
        'light': (70, 155, 220),
        'highlight': (120, 200, 255),
        'eye': (0, 255, 200),
        'warp': (80, 220, 255),
    },
    'reflector': {
        'dark': (100, 100, 110),
        'mid': (160, 165, 175),
        'light': (200, 205, 215),
        'highlight': (230, 235, 245),
        'eye': (255, 255, 100),
        'mirror': (220, 230, 255),
    },
    'leech': {
        'dark': (60, 15, 30),
        'mid': (120, 30, 60),
        'light': (170, 50, 85),
        'highlight': (210, 90, 120),
        'eye': (255, 200, 50),
        'mouth': (180, 20, 40),
    },
    'swarmer': {
        'dark': (100, 80, 10),
        'mid': (170, 140, 30),
        'light': (210, 180, 50),
        'highlight': (240, 215, 90),
        'eye': (255, 60, 60),
        'wing': (190, 160, 40),
    },
    'gravitywell': {
        'dark': (30, 10, 60),
        'mid': (60, 20, 120),
        'light': (100, 50, 180),
        'highlight': (150, 90, 220),
        'eye': (200, 100, 255),
        'core': (180, 60, 255),
    },
    'mimic': {
        'dark': (90, 70, 40),
        'mid': (150, 120, 70),
        'light': (190, 160, 100),
        'highlight': (220, 200, 140),
        'eye': (255, 50, 50),
        'teeth': (240, 240, 230),
    },
}

# Boss colors
BOSS_COLORS = {
    'rift_guardian': {
        'dark': (100, 15, 90),
        'mid': (170, 30, 150),
        'light': (210, 60, 190),
        'highlight': (240, 120, 220),
        'eye': (255, 50, 50),
        'core': (255, 200, 255),
    },
    'void_wyrm': {
        'dark': (15, 90, 60),
        'mid': (30, 150, 100),
        'light': (50, 200, 140),
        'highlight': (100, 230, 180),
        'eye': (255, 255, 100),
        'venom': (120, 255, 80),
    },
    'dimensional_architect': {
        'dark': (60, 30, 120),
        'mid': (100, 60, 180),
        'light': (140, 100, 220),
        'highlight': (180, 150, 245),
        'eye': (100, 255, 255),
        'rift': (200, 100, 255),
    },
    'temporal_weaver': {
        'dark': (110, 90, 30),
        'mid': (170, 145, 60),
        'light': (210, 190, 90),
        'highlight': (240, 225, 140),
        'eye': (100, 200, 255),
        'clock': (255, 220, 120),
    },
    'void_sovereign': {
        'dark': (40, 0, 60),
        'mid': (70, 10, 110),
        'light': (100, 30, 160),
        'highlight': (140, 70, 200),
        'eye': (255, 0, 100),
        'void': (150, 0, 200),
        'crown': (255, 200, 50),
    },
    'entropy_incarnate': {
        'dark': (100, 10, 10),
        'mid': (180, 30, 30),
        'light': (220, 60, 50),
        'highlight': (255, 100, 80),
        'eye': (255, 255, 80),
        'chaos': (255, 140, 40),
        'crack': (255, 200, 100),
    },
}

# Tile colors (grayscale - will be tinted per theme)
TILE = {
    'solid_dark': (60, 60, 65),
    'solid_mid': (100, 100, 108),
    'solid_light': (140, 140, 148),
    'solid_highlight': (170, 170, 178),
    'edge': (45, 45, 50),
    'crack': (75, 75, 82),
}

# Pickup colors
PICKUP = {
    'health': (255, 60, 80),
    'health_glow': (255, 150, 160),
    'energy': (60, 180, 255),
    'energy_glow': (150, 220, 255),
    'shard': (200, 100, 255),
    'shard_glow': (230, 180, 255),
    'coin': (255, 210, 60),
    'coin_glow': (255, 240, 160),
    'powerup': (80, 255, 120),
    'powerup_glow': (180, 255, 200),
}

# Projectile colors
PROJECTILE = {
    'player_bullet': (100, 200, 255),
    'player_glow': (180, 230, 255),
    'enemy_bullet': (255, 100, 80),
    'enemy_glow': (255, 180, 160),
    'boss_bullet': (200, 50, 255),
    'boss_glow': (230, 150, 255),
}

# Element colors (for tinting)
ELEMENT = {
    'fire': (255, 100, 20),
    'ice': (80, 160, 255),
    'electric': (255, 255, 60),
    'poison': (120, 255, 80),
    'void': (150, 0, 200),
}
