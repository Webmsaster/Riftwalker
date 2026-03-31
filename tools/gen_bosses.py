"""Generate all 6 boss sprite sheets via ComfyUI."""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comfyui_gen import generate_image
from PIL import Image

TEMP = 'tools/comfyui_temp'
S = 'digital painting, epic boss creature art, stylized, detailed, dark sci-fi, '
E = ', high quality, sharp details, single massive creature centered, dark void background, full body visible, imposing scale, game boss design, menacing'

BOSSES = {
    "rift_guardian": {
        "desc": "massive armored guardian knight of dimensional rift, huge glowing sword, ancient protector, imposing heavy armor with purple rift energy cracks",
        "size": 64,
        "sheet": (512, 576)  # 8*64, 9*64
    },
    "void_wyrm": {
        "desc": "enormous serpentine void dragon, coiling body filled with stars and void, cosmic horror, void energy purple breath, deep black scales",
        "size": 64,
        "sheet": (512, 576)
    },
    "dimensional_architect": {
        "desc": "geometric floating entity, reality-bending impossible shapes forming around it, mathematical horror, blue and white energy geometric patterns",
        "size": 64,
        "sheet": (512, 576)
    },
    "temporal_weaver": {
        "desc": "spider-like time manipulator, clockwork bronze body, temporal distortion golden webs, multiple arms with clock faces",
        "size": 64,
        "sheet": (512, 576)
    },
    "void_sovereign": {
        "desc": "colossal dark king of the void, massive shadowy royal figure, crown of purple dark energy flames, commanding eldritch presence",
        "size": 96,
        "sheet": (768, 864)  # 8*96, 9*96
    },
    "entropy_incarnate": {
        "desc": "chaos incarnate final boss entity, reality dissolving and fragmenting around it, all colors distorting, pure entropy given physical form, reality glitch effect",
        "size": 64,
        "sheet": (512, 576)
    },
}

ANIMS = {
    "idle": "idle menacing stance, breathing, dark presence",
    "move": "advancing forward, moving menacingly, approaching",
    "attack1": "primary attack, striking with main weapon, powerful blow",
    "attack2": "secondary attack, energy blast projectile, ranged assault",
    "attack3": "special area attack, devastating area effect, ground slam",
    "phase_transition": "transforming, powering up surge, energy transformation, phase change",
    "hurt": "taking massive damage, flinching in pain, wounded",
    "enrage": "enraged berserk mode, powered up, glowing intensely, fury",
    "dead": "defeated collapsing, dissolving into particles, destruction"
}
ANIM_FRAMES = {
    "idle": 6, "move": 6, "attack1": 6, "attack2": 6, "attack3": 6,
    "phase_transition": 8, "hurt": 3, "enrage": 6, "dead": 8
}

def gen_boss(name, info):
    """Generate one boss sprite sheet."""
    desc = info["desc"]
    fsize = info["size"]
    sheet_w, sheet_h = info["sheet"]
    COLS = 8

    print(f"\n{'='*50}")
    print(f"BOSS: {name} ({fsize}x{fsize} frames)")
    print(f"{'='*50}")

    bdir = f'{TEMP}/boss_{name}'
    os.makedirs(bdir, exist_ok=True)

    # Generate one image per animation pose
    for anim_name, anim_desc in ANIMS.items():
        prompt = f"{S}{desc}, {anim_desc}{E}"
        img_path = f'{bdir}/{anim_name}.png'

        # Bosses get larger generation for more detail
        gen_size = 768 if fsize == 96 else 512
        generate_image(prompt, img_path, width=gen_size, height=gen_size,
                      steps=25, cfg=7.5, seed=random.randint(1, 2**31))

    # Assemble sprite sheet
    rows = list(ANIMS.keys())
    sheet = Image.new('RGBA', (sheet_w, sheet_h), (0, 0, 0, 0))

    for row_idx, anim_name in enumerate(rows):
        img_path = f'{bdir}/{anim_name}.png'
        if not os.path.exists(img_path):
            print(f"  MISSING: {img_path}")
            continue

        src = Image.open(img_path).convert('RGBA')
        w, h = src.size
        margin = int(w * 0.08)
        char_region = src.crop((margin, margin, w - margin, h - margin))

        num_frames = ANIM_FRAMES[anim_name]
        cw, ch = char_region.size

        for frame_idx in range(num_frames):
            shift_x = int((frame_idx - num_frames // 2) * (cw * 0.012))
            shift_y = int(abs(frame_idx - num_frames // 2) * (ch * -0.008))

            cx = cw // 2 + shift_x
            cy = ch // 2 + shift_y
            half = int(min(cw, ch) * 0.45)

            x1 = max(0, cx - half)
            y1 = max(0, cy - half)
            x2 = min(cw, cx + half)
            y2 = min(ch, cy + half)

            frame = char_region.crop((x1, y1, x2, y2))
            frame = frame.resize((fsize, fsize), Image.LANCZOS)
            sheet.paste(frame, (frame_idx * fsize, row_idx * fsize))

    output = f'assets/textures/bosses/{name}.png'
    os.makedirs(os.path.dirname(output), exist_ok=True)
    sheet.save(output, 'PNG')
    print(f"  Sheet: {output} ({sheet_w}x{sheet_h}, {os.path.getsize(output)} bytes)")


if __name__ == "__main__":
    for name, info in BOSSES.items():
        gen_boss(name, info)
    print("\nALL 6 BOSSES DONE!")
