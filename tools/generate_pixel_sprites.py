"""Generate clean pixel-art sprite sheets for Riftwalker.
Replaces AI-generated sprites with hand-crafted pixel art."""

from PIL import Image, ImageDraw
import os, math

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures')

def rect(draw, x, y, w, h, color):
    if w <= 0 or h <= 0:
        return
    draw.rectangle([x, y, x+w-1, y+h-1], fill=color)

def outline_rect(draw, x, y, w, h, color):
    draw.rectangle([x, y, x+w-1, y+h-1], outline=color)

# ============================================================
# TILESET: 512x192, 16 cols x 6 rows of 32x32
# ============================================================
def generate_tileset():
    W, H = 512, 192
    TS = 32
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    STONE = (110, 100, 90, 255)
    STONE_LIGHT = (140, 130, 118, 255)
    STONE_DARK = (75, 68, 60, 255)
    STONE_EDGE = (160, 150, 135, 255)
    MORTAR = (85, 78, 70, 255)

    def draw_solid_tile(tx, ty, mask):
        bx, by = tx * TS, ty * TS
        has_top = bool(mask & 1)
        has_right = bool(mask & 2)
        has_bottom = bool(mask & 4)
        has_left = bool(mask & 8)

        # Base stone fill
        rect(draw, bx, by, TS, TS, STONE)

        # Brick pattern
        for row in range(4):
            ry = by + row * 8
            offset = 8 if row % 2 else 0
            draw.line([(bx, ry), (bx + TS - 1, ry)], fill=MORTAR)
            for col in range(-1, 3):
                mx = bx + col * 16 + offset
                if bx <= mx < bx + TS:
                    draw.line([(mx, ry), (mx, ry + 7)], fill=MORTAR)
            # Brick shading
            for col in range(3):
                brickX = bx + col * 16 + offset
                shade = ((row * 3 + col * 7) % 5) - 2
                c = tuple(max(0, min(255, STONE[i] + shade * 4)) for i in range(3)) + (255,)
                x0 = max(bx, brickX + 1)
                x1 = min(bx + TS - 1, brickX + 14)
                if x1 > x0:
                    rect(draw, x0, ry + 1, x1 - x0, 6, c)

        # Edge highlights where no neighbor
        if not has_top:
            rect(draw, bx, by, TS, 2, STONE_EDGE)
        if not has_left:
            rect(draw, bx, by, 2, TS, STONE_EDGE)
        if not has_bottom:
            rect(draw, bx, by + TS - 2, TS, 2, STONE_DARK)
        if not has_right:
            rect(draw, bx + TS - 2, by, 2, TS, STONE_DARK)

    for mask in range(16):
        draw_solid_tile(mask, 0, mask)

    # Row 1: Spike(0), Fire(4-7), Conveyor(8-11), Laser(12-15)
    # Spike
    bx, by = 0, TS
    rect(draw, bx, by, TS, TS, (60, 55, 50, 255))
    for i in range(4):
        sx = bx + 2 + i * 7
        for r in range(14):
            t = r / 14.0
            hw = int(3 * t)
            cy = by + 2 + r
            c = (180 - int(60 * t), 180 - int(80 * t), 180 - int(80 * t), 255)
            if hw > 0:
                draw.line([(sx + 3 - hw, cy), (sx + 3 + hw, cy)], fill=c)
            else:
                img.putpixel((sx + 3, cy), c)
        rect(draw, sx + 2, by + 1, 2, 3, (230, 220, 200, 255))

    # Fire (4 frames)
    for frame in range(4):
        bx = (4 + frame) * TS
        by = TS
        rect(draw, bx, by + TS - 8, TS, 8, (80, 30, 10, 255))
        for i in range(5):
            fx = bx + 3 + i * 6
            fh = 14 + ((frame + i * 3) % 5) * 3
            for r in range(fh):
                t = r / max(1, fh)
                red = int(255 * (1 - t * 0.3))
                grn = int(180 * (1 - t * 0.6))
                py = by + TS - 8 - r
                if by <= py < by + TS:
                    draw.line([(fx, py), (fx + 2, py)], fill=(red, grn, 20, 255))

    # Conveyor (4 frames)
    for frame in range(4):
        bx = (8 + frame) * TS
        by = TS
        rect(draw, bx, by, TS, TS, (70, 70, 80, 255))
        rect(draw, bx, by + 2, TS, 3, (95, 95, 105, 255))
        rect(draw, bx, by + TS - 5, TS, 3, (95, 95, 105, 255))
        for i in range(3):
            ax = bx + 4 + (i * 10 + frame * 3) % TS
            ay = by + TS // 2
            if ax + 4 <= bx + TS:
                draw.line([(ax, ay - 2), (ax + 3, ay)], fill=(200, 200, 60, 200))
                draw.line([(ax + 3, ay), (ax, ay + 2)], fill=(200, 200, 60, 200))

    # Laser emitter (4 frames)
    for frame in range(4):
        bx = (12 + frame) * TS
        by = TS
        rect(draw, bx, by, TS, TS, (50, 50, 60, 255))
        outline_rect(draw, bx + 1, by + 1, TS - 2, TS - 2, (80, 80, 90, 255))
        glow = 150 + (frame * 35) % 106
        rect(draw, bx + TS // 2 - 4, by + TS // 2 - 4, 8, 8, (glow, 20, 20, 255))
        rect(draw, bx + TS // 2 - 2, by + TS // 2 - 2, 4, 4, (255, 80, 80, 255))

    # Row 2: OneWay(0), Ice(4-7), GravityWell(12-15)
    bx, by = 0, 2 * TS
    rect(draw, bx, by, TS, 6, (160, 150, 130, 255))
    rect(draw, bx, by, TS, 2, (200, 190, 170, 255))
    for i in range(4):
        draw.line([(bx + 3 + i * 8, by + 6), (bx + 3 + i * 8, by + TS - 1)],
                  fill=(120, 110, 100, 120))

    for frame in range(4):
        bx = (4 + frame) * TS
        by = 2 * TS
        rect(draw, bx, by, TS, TS, (140, 200, 240, 220))
        rect(draw, bx + 2, by + 2, TS - 4, TS // 3, (180, 220, 255, 80))
        draw.line([(bx + 2, by + 1), (bx + TS - 3, by + 1)], fill=(220, 240, 255, 180))

    for frame in range(4):
        bx = (12 + frame) * TS
        by = 2 * TS
        rect(draw, bx, by, TS, TS, (40, 20, 60, 200))
        cx, cy = bx + TS // 2, by + TS // 2
        for ring in range(3):
            r = 4 + ring * 4
            for a in range(12):
                angle = math.radians(a * 30 + frame * 25 + ring * 15)
                px = cx + int(r * math.cos(angle))
                py = cy + int(r * math.sin(angle))
                if bx <= px < bx + TS and by <= py < by + TS:
                    img.putpixel((px, py), (160, 80, 220, 180 - ring * 40))

    # Row 3: Teleporter(10-11)
    for frame in range(2):
        bx = (10 + frame) * TS
        by = 3 * TS
        rect(draw, bx, by, TS, TS, (30, 60, 30, 200))
        glow = 180 + frame * 40
        rect(draw, bx + 4, by + 4, TS - 8, TS - 8, (50, min(255, glow), 100, 200))
        rect(draw, bx + TS // 2 - 4, by + TS // 2 - 4, 8, 8, (100, 255, 150, 230))
        outline_rect(draw, bx, by, TS, TS, (80, 255, 120, 160))

    path = os.path.join(OUT, 'tiles', 'tileset_universal.png')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Tileset: {path}")


# ============================================================
# PLAYER: 256x432, 8 cols x 9 rows of 32x48
# ============================================================
def draw_player_frame(img, col, row, fw, fh, pose, fidx):
    bx, by = col * fw, row * fh
    draw = ImageDraw.Draw(img)

    BODY = (50, 100, 200)
    BODY_L = (80, 140, 240)
    BODY_D = (30, 60, 140)
    VISOR = (0, 220, 220)
    OL = (20, 20, 40)
    BELT = (180, 160, 60)
    BOOT = (40, 40, 60)

    cx = bx + 16
    bob = 0
    lean = 0

    if pose == 'idle':
        bob = [0, -1, -1, 0, 0, 1][fidx % 6]
    elif pose == 'run':
        bob = [-1, -2, -1, 0, -1, -2, -1, 0][fidx % 8]
        lean = 1
    elif pose == 'jump':
        bob = [-2, -3, -2][fidx % 3]
    elif pose == 'fall':
        bob = [1, 2, 2][fidx % 3]
    elif pose == 'dash':
        lean = [3, 4, 3, 2][fidx % 4]
        bob = -1
    elif pose == 'wallslide':
        lean = -1
    elif pose == 'attack':
        lean = [0, 1, 2, 2, 1, 0][fidx % 6]
    elif pose == 'hurt':
        lean = [-2, 0, -1][fidx % 3]
        bob = [2, 0, 1][fidx % 3]
    elif pose == 'dead':
        bob = min(fidx * 2, 8)
        if fidx >= 3:
            yb = by + 32 + bob
            rect(draw, cx - 12, yb, 24, 8, BODY)
            rect(draw, cx - 14, yb + 1, 6, 6, BODY_D)
            rect(draw, cx - 13, yb + 2, 3, 2, VISOR)
            return

    yb = by + 14 + bob

    # Head
    hx = cx - 4 + lean
    hy = yb
    rect(draw, hx - 1, hy - 1, 10, 10, OL)
    rect(draw, hx, hy, 8, 8, BODY)
    rect(draw, hx, hy, 8, 3, BODY_L)
    rect(draw, hx + 2, hy + 3, 5, 2, VISOR)

    # Torso
    tx = cx - 5 + lean
    ty = yb + 9
    rect(draw, tx - 1, ty, 12, 13, OL)
    rect(draw, tx, ty, 10, 12, BODY)
    rect(draw, tx + 1, ty + 1, 4, 6, BODY_L)
    rect(draw, tx, ty + 10, 10, 2, BELT)

    # Arms
    ay = ty + 2
    if pose == 'attack':
        phase = [0, 1, 2, 3, 2, 1][fidx % 6]
        wx = tx + 10
        wy = ay - phase * 2
        rect(draw, wx, wy, 4, 8, BODY_D)
        rect(draw, wx + 1, wy - 6 + phase, 2, 8, (180, 200, 220))
        rect(draw, wx + 1, wy - 6 + phase, 2, 2, (220, 240, 255))
        rect(draw, tx - 3, ay, 3, 8, BODY_D)
    elif pose == 'dash':
        rect(draw, tx - 3, ay + 2, 3, 6, BODY_D)
        rect(draw, tx + 10, ay + 2, 3, 6, BODY_D)
    else:
        rect(draw, tx - 3, ay, 3, 8, BODY_D)
        rect(draw, tx + 10, ay, 3, 8, BODY_D)

    # Legs
    ly = ty + 12
    if pose == 'run':
        offs = [(-2, 0, 2, -2), (0, -2, 0, 2), (2, -2, -2, 0), (-1, 1, 1, -1),
                (2, 0, -2, 2), (0, 2, 0, -2), (-2, 2, 2, 0), (1, -1, -1, 1)]
        lo = offs[fidx % 8]
        rect(draw, cx - 4 + lo[0], ly + lo[1], 3, 10, BODY_D)
        rect(draw, cx + 1 + lo[2], ly + lo[3], 3, 10, BODY_D)
        rect(draw, cx - 5 + lo[0], ly + 8 + lo[1], 4, 3, BOOT)
        rect(draw, cx + 1 + lo[2], ly + 8 + lo[3], 4, 3, BOOT)
    else:
        rect(draw, cx - 4, ly, 3, 10, BODY_D)
        rect(draw, cx + 1, ly, 3, 10, BODY_D)
        rect(draw, cx - 5, ly + 8, 4, 3, BOOT)
        rect(draw, cx + 1, ly + 8, 4, 3, BOOT)


def generate_player():
    fw, fh = 32, 48
    img = Image.new('RGBA', (fw * 8, fh * 9), (0, 0, 0, 0))
    poses = ['idle', 'run', 'jump', 'fall', 'dash', 'wallslide', 'attack', 'hurt', 'dead']
    counts = [6, 8, 3, 3, 4, 3, 6, 3, 5]
    for row, (pose, cnt) in enumerate(zip(poses, counts)):
        for col in range(cnt):
            draw_player_frame(img, col, row, fw, fh, pose, col)
    path = os.path.join(OUT, 'player', 'player.png')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Player: {path}")


# ============================================================
# ENEMIES: 192x160, 6 cols x 5 rows of 32x32
# ============================================================
ENEMY_CONFIGS = {
    'walker':   {'body': (200, 50, 50),   'eye': (255, 255, 0),   'shape': 'round'},
    'flyer':    {'body': (200, 180, 40),   'eye': (255, 100, 0),   'shape': 'wing'},
    'charger':  {'body': (220, 120, 30),   'eye': (255, 50, 50),   'shape': 'bull'},
    'turret':   {'body': (100, 100, 110),  'eye': (255, 60, 60),   'shape': 'box'},
    'phaser':   {'body': (140, 50, 200),   'eye': (200, 150, 255), 'shape': 'crystal'},
    'exploder': {'body': (220, 100, 30),   'eye': (255, 200, 50),  'shape': 'blob'},
    'shielder': {'body': (50, 160, 200),   'eye': (100, 255, 255), 'shape': 'shield'},
    'crawler':  {'body': (80, 180, 60),    'eye': (255, 255, 100), 'shape': 'bug'},
    'summoner': {'body': (180, 50, 160),   'eye': (255, 150, 255), 'shape': 'robe'},
    'sniper':   {'body': (60, 80, 160),    'eye': (100, 200, 255), 'shape': 'hood'},
    'minion':   {'body': (180, 60, 60),    'eye': (255, 200, 0),   'shape': 'round'},
}

def draw_enemy(img, col, row, fw, fh, etype, pose, fidx):
    bx, by = col * fw, row * fh
    draw = ImageDraw.Draw(img)
    cfg = ENEMY_CONFIGS.get(etype, ENEMY_CONFIGS['walker'])
    body = cfg['body']
    dark = tuple(max(0, c - 40) for c in body)
    light = tuple(min(255, c + 40) for c in body)
    eye = cfg['eye']
    ol = (20, 20, 30)
    cx, cy = bx + fw // 2, by + fh // 2
    bob = 0

    if pose == 'idle':
        bob = [0, -1, 0, 1][fidx % 4]
    elif pose == 'walk':
        bob = [-1, 0, 1, 0, -1, 0][fidx % 6]
    elif pose == 'hurt':
        bob = [2, -1][fidx % 2]
    elif pose == 'dead':
        body = tuple(int(c * max(0.2, 1.0 - fidx * 0.25)) for c in body)
        dark = tuple(int(c * max(0.2, 1.0 - fidx * 0.25)) for c in dark)
    cy += bob

    shape = cfg['shape']

    if shape == 'round':
        rect(draw, cx - 8, cy - 7, 16, 14, ol)
        rect(draw, cx - 7, cy - 6, 14, 12, body)
        rect(draw, cx - 6, cy - 5, 6, 5, light)
        rect(draw, cx - 4, cy - 3, 3, 3, eye)
        rect(draw, cx + 2, cy - 3, 3, 3, eye)
        img.putpixel((cx - 3, cy - 2), (0, 0, 0, 255))
        img.putpixel((cx + 3, cy - 2), (0, 0, 0, 255))
        lo = [0, 1, 0, -1][fidx % 4] if pose in ('walk', 'idle') else 0
        rect(draw, cx - 6, cy + 6 + lo, 4, 6, dark)
        rect(draw, cx + 2, cy + 6 - lo, 4, 6, dark)

    elif shape == 'wing':
        wu = fidx % 2 == 0
        wy = -4 if wu else 2
        rect(draw, cx - 14, cy + wy, 8, 6, body)
        rect(draw, cx + 6, cy + wy, 8, 6, body)
        rect(draw, cx - 12, cy + wy + 1, 6, 4, light)
        rect(draw, cx + 7, cy + wy + 1, 6, 4, light)
        rect(draw, cx - 5, cy - 4, 10, 10, ol)
        rect(draw, cx - 4, cy - 3, 8, 8, body)
        rect(draw, cx - 3, cy - 1, 2, 2, eye)
        rect(draw, cx + 1, cy - 1, 2, 2, eye)

    elif shape == 'bull':
        ln = 2 if pose == 'walk' else 0
        rect(draw, cx - 10 + ln, cy - 5, 20, 14, ol)
        rect(draw, cx - 9 + ln, cy - 4, 18, 12, body)
        rect(draw, cx - 8 + ln, cy - 3, 8, 6, light)
        rect(draw, cx - 10 + ln, cy - 8, 3, 5, light)
        rect(draw, cx + 7 + ln, cy - 8, 3, 5, light)
        rect(draw, cx - 4 + ln, cy - 2, 3, 2, eye)
        rect(draw, cx + 2 + ln, cy - 2, 3, 2, eye)
        rect(draw, cx - 7, cy + 8, 4, 6, dark)
        rect(draw, cx + 3, cy + 8, 4, 6, dark)

    elif shape == 'box':
        rect(draw, cx - 9, cy - 8, 18, 18, ol)
        rect(draw, cx - 8, cy - 7, 16, 16, body)
        rect(draw, cx - 6, cy - 5, 12, 6, light)
        rect(draw, cx - 2, cy + 2, 4, 10, dark)
        rect(draw, cx - 1, cy + 2, 2, 10, (150, 150, 160, 255))
        rect(draw, cx - 2, cy - 4, 4, 3, eye)

    elif shape == 'crystal':
        pts = [(cx, cy - 10), (cx + 8, cy), (cx, cy + 10), (cx - 8, cy)]
        draw.polygon(pts, fill=body, outline=ol)
        pts2 = [(cx, cy - 5), (cx + 4, cy), (cx, cy + 5), (cx - 4, cy)]
        draw.polygon(pts2, fill=light)
        rect(draw, cx - 2, cy - 2, 4, 3, eye)

    elif shape == 'blob':
        r = 8 + (fidx % 3)
        draw.ellipse([cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1], fill=ol)
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=body)
        draw.ellipse([cx - r + 2, cy - r + 2, cx + 2, cy], fill=light)
        rect(draw, cx - 2, cy - 2, 4, 4, eye)
        draw.line([(cx, cy - r), (cx + 3, cy - r - 4)], fill=(200, 200, 200), width=1)
        rect(draw, cx + 2, cy - r - 5, 3, 3, (255, 150, 0, 255))

    elif shape == 'shield':
        rect(draw, cx - 6, cy - 7, 12, 16, ol)
        rect(draw, cx - 5, cy - 6, 10, 14, body)
        rect(draw, cx - 4, cy - 5, 5, 6, light)
        rect(draw, cx - 10, cy - 6, 5, 14, (80, 200, 240, 255))
        rect(draw, cx - 9, cy - 5, 3, 12, (120, 230, 255, 255))
        rect(draw, cx, cy - 3, 3, 2, eye)
        rect(draw, cx - 4, cy + 8, 3, 6, dark)
        rect(draw, cx + 1, cy + 8, 3, 6, dark)

    elif shape == 'bug':
        rect(draw, cx - 10, cy - 3, 20, 10, ol)
        rect(draw, cx - 9, cy - 2, 18, 8, body)
        rect(draw, cx - 8, cy - 1, 8, 4, light)
        for i in range(3):
            lo2 = ((fidx + i) % 3) - 1
            rect(draw, cx - 8 + i * 7, cy + 5 + lo2, 2, 4, dark)
            rect(draw, cx - 7 + i * 7, cy + 5 - lo2, 2, 4, dark)
        rect(draw, cx - 5, cy - 1, 2, 2, eye)
        rect(draw, cx + 3, cy - 1, 2, 2, eye)

    elif shape == 'robe':
        rect(draw, cx - 7, cy - 6, 14, 18, ol)
        rect(draw, cx - 6, cy - 5, 12, 16, body)
        rect(draw, cx - 5, cy - 4, 5, 8, light)
        rect(draw, cx - 5, cy - 10, 10, 6, dark)
        rect(draw, cx - 4, cy - 9, 8, 4, body)
        rect(draw, cx - 3, cy - 7, 2, 2, eye)
        rect(draw, cx + 1, cy - 7, 2, 2, eye)
        draw.line([(cx + 7, cy - 8), (cx + 7, cy + 10)], fill=(160, 140, 80), width=2)
        rect(draw, cx + 5, cy - 10, 5, 4, (200, 100, 255, 255))

    elif shape == 'hood':
        rect(draw, cx - 5, cy - 8, 10, 18, ol)
        rect(draw, cx - 4, cy - 7, 8, 16, body)
        rect(draw, cx - 3, cy - 6, 4, 7, light)
        rect(draw, cx - 5, cy - 10, 10, 5, dark)
        rect(draw, cx - 1, cy - 5, 4, 2, eye)
        rect(draw, cx + 4, cy - 4, 10, 2, (120, 120, 130, 255))
        rect(draw, cx + 4, cy - 3, 2, 6, dark)
        rect(draw, cx - 3, cy + 9, 3, 5, dark)
        rect(draw, cx + 1, cy + 9, 3, 5, dark)

    if pose == 'attack':
        ga = 80 + (fidx * 40) % 120
        rect(draw, cx - 2, cy - 2, 4, 4, (*eye, ga))


def generate_enemy(etype):
    fw, fh = 32, 32
    img = Image.new('RGBA', (fw * 6, fh * 5), (0, 0, 0, 0))
    poses = ['idle', 'walk', 'attack', 'hurt', 'dead']
    counts = [4, 6, 4, 2, 4]
    for row, (pose, cnt) in enumerate(zip(poses, counts)):
        for col in range(cnt):
            draw_enemy(img, col, row, fw, fh, etype, pose, col)
    path = os.path.join(OUT, 'enemies', f'{etype}.png')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Enemy {etype}: {path}")


# ============================================================
# PICKUPS: 64x80, 4 cols x 5 rows of 16x16
# ============================================================
def generate_pickups():
    S = 16
    img = Image.new('RGBA', (64, 80), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    def gem(c, r, color, hi):
        bx, by = c * S, r * S
        cx, cy = bx + 8, by + 8
        pts = [(cx, cy - 5), (cx + 5, cy), (cx, cy + 5), (cx - 5, cy)]
        draw.polygon(pts, fill=color, outline=(20, 20, 30))
        pts2 = [(cx, cy - 3), (cx - 3, cy), (cx, cy + 1)]
        draw.polygon(pts2, fill=hi)
        if 0 <= cx + 2 < img.width and 0 <= cy - 3 < img.height:
            img.putpixel((cx + 2, cy - 3), (255, 255, 255, 200))

    for c in range(4): gem(c, 0, (220, 50, 50, 255), (255, 120, 120, 200))
    for c in range(4): gem(c, 1, (50, 120, 220, 255), (120, 180, 255, 200))
    for c in range(4): gem(c, 2, (160, 80, 220, 255), (200, 140, 255, 200))
    gem(0, 3, (80, 200, 80, 255), (140, 255, 140, 200))
    gem(1, 3, (220, 200, 50, 255), (255, 240, 120, 200))
    gem(2, 3, (220, 160, 40, 255), (255, 200, 100, 200))
    gem(3, 3, (60, 200, 200, 255), (120, 240, 240, 200))
    gem(0, 4, (80, 220, 80, 255), (140, 255, 140, 200))
    gem(1, 4, (200, 200, 200, 255), (240, 240, 240, 200))
    gem(2, 4, (60, 180, 220, 255), (120, 220, 255, 200))
    gem(3, 4, (220, 120, 60, 255), (255, 180, 120, 200))

    path = os.path.join(OUT, 'pickups', 'pickups.png')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Pickups: {path}")


# ============================================================
# BOSSES: 512x576 (64x64), Void Sovereign 768x864 (96x96)
# ============================================================
def draw_boss(img, col, row, fw, fh, btype, pose, fidx):
    bx, by = col * fw, row * fh
    draw = ImageDraw.Draw(img)

    cfgs = {
        0: ((160, 60, 200), (255, 100, 255), 'golem'),
        1: ((40, 160, 120), (100, 255, 200), 'wyrm'),
        2: ((60, 80, 200), (100, 150, 255), 'mage'),
        3: ((200, 180, 40), (255, 240, 100), 'clock'),
        4: ((100, 20, 160), (200, 80, 255), 'king'),
        5: ((180, 30, 30), (255, 100, 50), 'chaos'),
    }
    body, eye, shape = cfgs.get(btype, cfgs[0])
    dark = tuple(max(0, c - 50) for c in body)
    light = tuple(min(255, c + 50) for c in body)
    ol = (15, 15, 25)
    s = fw / 64.0
    cx, cy = bx + fw // 2, by + fh // 2

    bob = 0
    if pose == 'idle':
        bob = [0, -1, -2, -1, 0, 1][fidx % 6]
    elif pose in ('attack1', 'attack2', 'attack3'):
        bob = [0, -2, -4, -2, 0, 2][fidx % 6]
    elif pose == 'hurt':
        bob = [3, -1, 1][fidx % 3]
    elif pose == 'dead':
        bob = min(fidx * 2, 10)
    cy += int(bob * s)

    def sr(v): return int(v * s)

    if shape == 'golem':
        rect(draw, cx - sr(20), cy - sr(18), sr(40), sr(36), ol)
        rect(draw, cx - sr(19), cy - sr(17), sr(38), sr(34), body)
        rect(draw, cx - sr(16), cy - sr(14), sr(16), sr(14), light)
        rect(draw, cx - sr(10), cy - sr(26), sr(20), sr(12), ol)
        rect(draw, cx - sr(9), cy - sr(25), sr(18), sr(10), body)
        rect(draw, cx - sr(6), cy - sr(21), sr(4), sr(3), eye)
        rect(draw, cx + sr(2), cy - sr(21), sr(4), sr(3), eye)
        rect(draw, cx - sr(26), cy - sr(12), sr(8), sr(24), dark)
        rect(draw, cx + sr(18), cy - sr(12), sr(8), sr(24), dark)
        rect(draw, cx - sr(14), cy + sr(16), sr(10), sr(14), dark)
        rect(draw, cx + sr(4), cy + sr(16), sr(10), sr(14), dark)

    elif shape == 'wyrm':
        for i in range(5):
            sx = cx - sr(20) + i * sr(10)
            sy = cy + int(math.sin(fidx * 0.5 + i * 0.8) * sr(4))
            seg = sr(10) - i
            if seg > 0:
                draw.ellipse([sx - seg, sy - seg, sx + seg, sy + seg], fill=body, outline=ol)
        rect(draw, cx - sr(14), cy - sr(16), sr(20), sr(16), ol)
        rect(draw, cx - sr(13), cy - sr(15), sr(18), sr(14), body)
        rect(draw, cx - sr(10), cy - sr(12), sr(8), sr(6), light)
        rect(draw, cx - sr(8), cy - sr(10), sr(4), sr(3), eye)
        rect(draw, cx, cy - sr(10), sr(4), sr(3), eye)
        rect(draw, cx - sr(8), cy - sr(2), sr(12), sr(4), dark)

    elif shape == 'mage':
        rect(draw, cx - sr(14), cy - sr(6), sr(28), sr(28), ol)
        rect(draw, cx - sr(13), cy - sr(5), sr(26), sr(26), body)
        rect(draw, cx - sr(10), cy - sr(3), sr(10), sr(12), light)
        rect(draw, cx - sr(8), cy - sr(18), sr(16), sr(14), ol)
        rect(draw, cx - sr(7), cy - sr(17), sr(14), sr(12), body)
        for i in range(3):
            sx = cx - sr(6) + i * sr(5)
            rect(draw, sx, cy - sr(24), sr(3), sr(8), eye)
        rect(draw, cx - sr(4), cy - sr(12), sr(3), sr(3), eye)
        rect(draw, cx + sr(1), cy - sr(12), sr(3), sr(3), eye)

    elif shape == 'clock':
        r = sr(24)
        if r > 0:
            draw.ellipse([cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1], fill=ol)
            draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=body)
            inner = r - sr(4)
            if inner > 0:
                draw.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], fill=light)
        a1 = math.radians(fidx * 60)
        a2 = math.radians(fidx * 15)
        draw.line([(cx, cy), (cx + int(sr(14) * math.sin(a1)), cy - int(sr(14) * math.cos(a1)))],
                  fill=ol, width=max(1, sr(2)))
        draw.line([(cx, cy), (cx + int(sr(18) * math.sin(a2)), cy - int(sr(18) * math.cos(a2)))],
                  fill=dark, width=max(1, sr(1)))
        rect(draw, cx - sr(4), cy - sr(3), sr(8), sr(6), eye)

    elif shape == 'king':
        rect(draw, cx - sr(28), cy - sr(20), sr(56), sr(44), ol)
        rect(draw, cx - sr(27), cy - sr(19), sr(54), sr(42), body)
        rect(draw, cx - sr(22), cy - sr(16), sr(20), sr(18), light)
        rect(draw, cx - sr(14), cy - sr(34), sr(28), sr(18), ol)
        rect(draw, cx - sr(13), cy - sr(33), sr(26), sr(16), body)
        for i in range(5):
            rect(draw, cx - sr(10) + i * sr(5), cy - sr(40), sr(3), sr(8), eye)
        for ro in range(2):
            ey = cy - sr(28) + ro * sr(6)
            rect(draw, cx - sr(8), ey, sr(4), sr(3), eye)
            rect(draw, cx + sr(4), ey, sr(4), sr(3), eye)
        rect(draw, cx - sr(36), cy - sr(14), sr(10), sr(30), dark)
        rect(draw, cx + sr(26), cy - sr(14), sr(10), sr(30), dark)

    elif shape == 'chaos':
        for i in range(8):
            angle = math.radians(i * 45 + fidx * 10)
            dist = sr(12) + int(math.sin(fidx * 0.3 + i) * sr(4))
            sx = cx + int(dist * math.cos(angle))
            sy = cy + int(dist * math.sin(angle))
            seg = sr(6)
            if seg > 0:
                draw.ellipse([sx - seg, sy - seg, sx + seg, sy + seg], fill=body)
        cr = sr(14)
        if cr > 0:
            draw.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], fill=ol)
            draw.ellipse([cx - cr + 2, cy - cr + 2, cx + cr - 2, cy + cr - 2], fill=body)
        rect(draw, cx - sr(6), cy - sr(4), sr(4), sr(4), eye)
        rect(draw, cx + sr(2), cy - sr(4), sr(4), sr(4), eye)


def generate_boss(btype):
    fw, fh = 96 if btype == 4 else 64, 96 if btype == 4 else 64
    img = Image.new('RGBA', (fw * 8, fh * 9), (0, 0, 0, 0))
    poses = ['idle', 'move', 'attack1', 'attack2', 'attack3',
             'phase_transition', 'hurt', 'enrage', 'dead']
    counts = [6, 6, 6, 6, 6, 8, 3, 6, 8]
    for row, (pose, cnt) in enumerate(zip(poses, counts)):
        for col in range(cnt):
            draw_boss(img, col, row, fw, fh, btype, pose, col)
    names = {0: 'rift_guardian', 1: 'void_wyrm', 2: 'dimensional_architect',
             3: 'temporal_weaver', 4: 'void_sovereign', 5: 'entropy_incarnate'}
    path = os.path.join(OUT, 'bosses', f'{names[btype]}.png')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Boss {names[btype]}: {path}")


if __name__ == '__main__':
    print("Generating pixel-art sprites...")
    generate_tileset()
    generate_player()
    generate_pickups()
    for e in ['walker', 'flyer', 'charger', 'turret', 'phaser',
              'exploder', 'shielder', 'crawler', 'summoner', 'sniper', 'minion']:
        generate_enemy(e)
    for bt in range(6):
        generate_boss(bt)
    print("Done!")
