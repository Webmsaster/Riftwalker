# Riftwalker — Steam Release Guide

End-to-end checklist for shipping Riftwalker on Steam. Everything **technical
inside this repo is already prepared** (build scripts, depot config, Steam
shim, store screenshots). The remaining steps are account-, payment- and
Valve-review-gated and can only be done by the account owner.

> **Status legend:** ✅ done in repo · 🔲 you must do (account/manual) · ⏳ waiting on Valve

---

## What's already prepared in this repo

| Item | Location |
|---|---|
| ✅ Steamworks shim (init / achievements / cloud saves) | `src/Core/SteamShim.{h,cpp}` |
| ✅ CMake opt-in flag `ENABLE_STEAMWORKS` | `CMakeLists.txt` |
| ✅ SteamPipe app build script | `steam/scripts/app_build.vdf` |
| ✅ SteamPipe depot build script | `steam/scripts/depot_build.vdf` |
| ✅ Clean-payload builder | `tools/make_steam_build.ps1` |
| ✅ Test App ID file (Spacewar 480) | `steam_appid.txt` |
| ✅ 9 store screenshots @ 1920×1080 | `dist/screenshots/` |
| ✅ Embedded app icon | `assets/icon.ico` |
| ✅ Crash handler + save versioning | `src/Core/CrashHandler.*`, `SaveUtils.h` |

---

## Phase 1 — Steamworks account 🔲

1. Go to <https://partner.steamgames.com/> → "Sign up".
2. Complete the **Steam Direct** onboarding:
   - Identity verification + tax forms (W-9 US / W-8BEN international).
   - Bank details for payouts.
   - Pay the **one-time 100 USD Steam Direct fee** (recoupable after $1,000 in sales).
3. Wait for account approval (usually 1–3 business days).

## Phase 2 — Create the app 🔲

1. In the Steamworks dashboard: **"Create new app"** → you receive an **App ID**.
2. Open *App Admin → App Landing Page → Edit Steamworks Settings*.
3. Under **SteamPipe → Depots**, note the auto-created **Depot ID** (normally `App ID + 1`).
4. Fill in `steam/scripts/app_build.vdf` and `depot_build.vdf`:
   - Replace `YOUR_APP_ID` with your App ID.
   - Replace `YOUR_DEPOT_ID` with your Depot ID.
5. Put your real App ID into `steam_appid.txt` (single line, App ID only) for
   local testing. **Do not commit it and do not ship it** — `.gitignore` and the
   depot `FileExclusion` already handle that. Valve injects the real ID at runtime.

## Phase 3 — SDK + Steam build 🔲

1. Download the **Steamworks SDK** from the dashboard (Partner login required).
2. Extract into `vendor/steamworks/`:
   ```
   vendor/steamworks/public/steam/*.h
   vendor/steamworks/redistributable_bin/win64/steam_api64.lib
   vendor/steamworks/redistributable_bin/win64/steam_api64.dll
   ```
3. Configure + build with Steam linked:
   ```powershell
   cd build
   cmake -DENABLE_STEAMWORKS=ON ..
   cmake --build . --config Release
   ```
   `steam_api64.dll` is copied next to the exe automatically (see CMakeLists.txt).

## Phase 4 — Local test with Steam 🔲

1. Make sure the Steam client is **running and logged in**.
2. Run `build/Release/Riftwalker.exe` with `steam_appid.txt` (your real ID) beside it.
3. Confirm the log line `Connected to Steam (user: ...)`.
4. Unlock an in-game achievement → verify it mirrors to Steam (needs achievements
   defined in *Steamworks → Stats & Achievements* first — see Phase 4b below).

## Phase 4b — Steam achievement schema 🔲

The in-game side is **already wired**: `AchievementSystem::unlock(id)` calls
`Steam::setAchievement(id)`, and the local achievement ID **is** the Steam API
name. All 41 in-game achievements will mirror to Steam automatically once
they exist in the Steamworks dashboard.

1. Generate the schema from the source of truth:
   ```powershell
   python tools\gen_steam_achievements.py
   ```
   This parses `src/Game/AchievementSystem.cpp` and emits three files into
   `steam/`:

   | File | Purpose |
   |---|---|
   | `achievements_schema.csv` | Steamworks "Import Achievements" CSV |
   | `achievements_dashboard_list.md` | Human-readable preview of what'll be uploaded |
   | `achievements_icons.txt` | Per-achievement icon checklist (64×64 PNG + grey variant) |

2. Open *Steamworks → Stats & Achievements → Import* and upload
   `steam/achievements_schema.csv`.
3. For each achievement, upload the achieved + greyed icon PNGs via the
   per-achievement editor. Source PNGs live under `steam/icons/`.
4. **Re-run `gen_steam_achievements.py`** after any edit to
   `AchievementSystem.cpp` to keep the schema in sync.

## Phase 5 — Upload the build 🔲

1. Get **steamcmd**: <https://developer.valvesoftware.com/wiki/SteamCMD>
2. Stage a clean payload (excludes saves/logs/screenshots automatically):
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\make_steam_build.ps1
   ```
3. Upload:
   ```powershell
   steamcmd +login <partner_user> +run_app_build "<repo>\steam\scripts\app_build.vdf" +quit
   ```
4. In Steamworks → **Builds**: the upload appears but is **not live** (by design).
   Set it live on a **beta branch** first, test, then promote to `default`.

## Phase 6 — Store page + assets 🔲

Fill out *Store → Store Page Edit*. Required graphic assets (see checklist below).
Set: short + full description, tags, genres, system requirements, price,
release date, age rating (IARC questionnaire), languages (EN + DE — already
fully localized).

## Phase 7 — Review + release ⏳ → 🔲

1. Submit the **build for review** (Valve checks it launches/quits cleanly) — 1–5 days.
2. Submit the **store page for review** — separate pass.
3. Once both pass, set a release date. Steam requires the store page live
   ≥ 2 weeks before launch (for a "Coming Soon" page).
4. On launch day: promote the reviewed build to `default` and hit **Release**.

---

## Store asset checklist

Steam graphic asset specs (PNG/JPG). Make at `tools/make_store_capsules.py` once
you have a logo, or commission them.

| Asset | Size (px) | Status |
|---|---|---|
| Small Capsule | 462 × 174 | 🔲 |
| Header Capsule | 460 × 215 | 🔲 |
| Main Capsule | 616 × 353 | 🔲 |
| Vertical Capsule | 374 × 448 | 🔲 |
| Page Background | 1438 × 810 | 🔲 (optional) |
| Library Capsule | 600 × 900 | 🔲 |
| Library Hero | 3840 × 1240 | 🔲 |
| Library Logo (transparent) | up to 1280 × 720 | 🔲 |
| Client Icon | 32 × 32 (.tga / .jpg) | ✅ source: `assets/icon.ico` (export 32px) |
| Screenshots (min 5) | 1920 × 1080 | ✅ `dist/screenshots/` (9 provided) |
| Trailer (strongly recommended) | 1920×1080, H.264 | 🔲 capture from gameplay |

> The **capsule art** (Small/Header/Main/Vertical/Library) is the one thing not
> auto-generatable from the game — it needs key art / a logo treatment. Everything
> else is in place.

---

## Quick reference — file map

```
steam/
  scripts/
    app_build.vdf      # edit YOUR_APP_ID + YOUR_DEPOT_ID
    depot_build.vdf    # edit YOUR_DEPOT_ID
  content/             # generated by make_steam_build.ps1 (gitignored)
  output/              # steamcmd logs (gitignored)
tools/
  make_steam_build.ps1 # build + stage clean payload
steam_appid.txt        # test ID 480; replace locally with real ID (gitignored)
vendor/steamworks/     # you drop the SDK here (Phase 3)
```

## Alternative: ship DRM-free first

The game already runs fully stand-alone (`ENABLE_STEAMWORKS=OFF`, the default).
The Inno Setup installer (`installer/Riftwalker.iss`) produces a distributable
`Setup.exe` for itch.io / direct download with **no Steam dependency** — useful
to release while the Steamworks onboarding completes.
