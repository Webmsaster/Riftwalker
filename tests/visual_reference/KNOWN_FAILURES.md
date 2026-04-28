# Visual Regression — Known Timing-Variance Failures

The visual regression suite (`ctest -C Release -R visual_regression`) is a
non-blocking quality signal. The 18 reference screenshots cover full UI flow
and gameplay snapshots; **5 are expected to fail on most runs** because the
captured frame includes time-dependent state.

## Stable PASS (13/18, ~72%)

01_splash, 02_menu, 03_class_select, 04_difficulty, 07_pause, 08_upgrades,
09_achievements, 10_tutorial, 11_options, 13_lore, 15_challenges, 17_run_history,
18_daily_leaderboard.

## Expected timing-variance FAILs (5/18)

| # | Screen | Source of variance | Typical diff |
|---|--------|--------------------|--------------|
| 05 | Gameplay | Run-intro overlay fade-in/out timing | 14–17% |
| 06 | Debug overlay | Per-frame stats (FPS, entity counts) | 14–18% |
| 12 | Keybindings | Selection-cursor pulse + scroll position | <1–3% |
| 14 | Bestiary | Pulsing entry highlights | 1–3% |
| 16 | Credits | Scroll position at capture | <1–4% |

These have been stable as known-failures across multiple sessions and are not
release blockers. To re-baseline (e.g. after intentional UI changes), copy
`build/Release/screenshots/visual_test/<file>.png` to `tests/visual_reference/`.

## Hard render regressions (would-be blockers)

NONE in current build (2026-04-28). All FAILs above produce visibly identical
gameplay; the diffs are from animation phase, not from rendering errors.
