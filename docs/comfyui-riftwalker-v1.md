# ComfyUI v1 Pipeline for Riftwalker

## Scope

Start with three asset families only:

1. Ingame backgrounds
2. Main menu / splash art
3. UI / panel backgrounds

Do not start with player, enemy, or hitbox-critical gameplay sprites.

## Why this order

- Backgrounds add production value fastest and are forgiving.
- Menu art gives the game an identity before gameplay art is replaced.
- UI panels benefit from image support but still need strong readability, so they are best handled after the background style is stable.

## Current game targets

- Runtime target resolution: 1280x720
- Current play background is multi-layer parallax
- Current pause panel uses a 1160x460 central panel
- Backgrounds should support a side-view gameplay lane and keep the center readable

## v1 ComfyUI workflow

Assumption:
Use one checkpoint per asset family. Do not mix multiple models inside one DIM-A or DIM-B batch.

### A. Base text-to-image

Use the simplest stable graph first:

1. `CheckpointLoaderSimple`
2. `CLIPTextEncode` for positive prompt
3. `CLIPTextEncode` for negative prompt
4. `EmptyLatentImage`
5. `KSampler`
6. `VAEDecode`
7. `SaveImage`

Recommended v1 settings:

- Output base size for backgrounds: 1344x768 or 1536x864
- Output base size for menu splash: 1344x768 or 1536x864
- Output base size for UI panel art: 1024x576
- Steps: 28-36
- CFG: 5.5-7.0
- Sampler: keep one sampler for the whole batch family
- Denoise: standard txt2img full generation

### B. Variant generation

Keep the graph identical.
Only vary:

- seed
- one short prompt suffix
- occasionally CFG by small steps only

Use 4-6 variants per prompt family, not 20 random shots.

### C. Upscaling

Keep this simple in v1.

Preferred order:

1. Select image first
2. Upscale second
3. Cleanup third

Two safe options:

- If you already have an upscale model in ComfyUI:
  `LoadImage -> UpscaleModelLoader -> ImageUpscaleWithModel -> SaveImage`
- If not:
  use a second pass with larger latent/image size and low denoise rather than inventing a custom chain

### D. Optional inpaint / outpaint

Only use after selection.

Best use cases:

- remove a bright focal object from the gameplay center
- widen left/right edges for parallax or menu crop safety
- simplify noisy areas behind platforms or UI text

Keep inpaint denoise low to medium.
Do not repaint the whole image unless the base image is already close.

## Production rules

### Backgrounds

- Generate in wide landscape format only
- Ask for layered depth: far sky, mid silhouette, near silhouette
- Keep the center 35-45% visually calmer than the edges
- Avoid strong single objects in dead center
- Avoid tiny high-frequency detail in the gameplay lane

### Menu splash

- Strong silhouette
- Clear focal mass in upper-middle, not exactly center
- Leave lower-middle and button areas calmer
- Make room for title and menu buttons

### UI / panel backgrounds

- Generate them as decorative surface art, not as finished UI mockups
- Use low-contrast ornamental texture
- Avoid faces, text, symbols, hands, architecture with perspective pull
- These should read as materials: void glass, worn alloy, cracked screen, resonance panel, rift haze

## Seed strategy

Use seed blocks by family:

- DIM-A: 1100-1199
- DIM-B: 2100-2199
- Boss: 3100-3199
- Finale: 4100-4199
- Menu/UI: 5100-5199

Keep the chosen seed in the filename.

## Batch strategy

For each new family:

1. Pick one base prompt
2. Run 4 variants with seed offsets `+0 +1 +2 +3`
3. Pick 1-2 survivors
4. Upscale only survivors
5. Inpaint only finals

## File naming

Format:

`rw_<family>_<theme>_<shot>_v###_s####.png`

Examples:

- `rw_bg_dima_ruinswide_v001_s1104.png`
- `rw_bg_dimb_voidarches_v002_s2107.png`
- `rw_bg_boss_monolithhall_v001_s3102.png`
- `rw_bg_final_dimensioncollision_v001_s4108.png`
- `rw_menu_splash_riftgate_v001_s5103.png`
- `rw_ui_panel_voidglass_v001_s5112.png`

## Workflow exports

Save exported ComfyUI JSONs with matching names:

- `rw_wf_bg_base_v001.json`
- `rw_wf_bg_upscale_v001.json`
- `rw_wf_bg_inpaint_v001.json`
- `rw_wf_menu_base_v001.json`
- `rw_wf_ui_panel_v001.json`

Do not version by date first. Version by intent.

## Suggested first production pass

1. DIM-A background set, 2 prompt families, 4 variants each
2. DIM-B background set, 2 prompt families, 4 variants each
3. Boss arena set, 2 prompt families, 4 variants each
4. Main menu splash set, 3 variants
5. UI panel material set, 6 small variants
