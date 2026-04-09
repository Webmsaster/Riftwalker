#!/usr/bin/env bash
# check_visuals.sh — One-command visual sanity check for Riftwalker
#
# Just run:
#     ./check_visuals.sh
#
# What it does:
#   1. Builds the game (Release config)
#   2. Runs the automated visual test (17 screenshots across all menus + gameplay)
#   3. Compares every screenshot against the saved baseline
#   4. If ANYTHING changed, opens an interactive HTML diff report in your browser
#      so you can see side-by-side: reference, actual, and a red pixel-diff overlay
#
# If you intentionally changed a screen and the diff is expected, accept the new
# look as the new baseline with:
#     ./check_visuals.sh --accept

set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

ACCEPT=false
if [[ "${1:-}" == "--accept" ]]; then
    ACCEPT=true
fi

# ------- Pre-check: self-assignment guard -------
# Catches replace_all regressions (like the 2026-04-09 HUD m_frameTicks bug)
if [[ -f check_self_assigns.sh ]]; then
    ./check_self_assigns.sh --ci || { echo "BLOCKED: Self-assignment detected. Fix before visual test."; exit 1; }
fi

# ------- Step 1: build -------
echo "[1/3] Building Release..."
if ! cmake --build build --config Release 2>&1 | tail -5 | grep -qE "error|Error|FEHLER"; then
    :
else
    echo "ERROR: Build failed. Fix compile errors and re-run."
    exit 1
fi

# ------- Step 2: run the visual test -------
echo "[2/3] Capturing 17 screenshots via --visual-test..."
rm -f build/Release/screenshots/visual_test/*.png
(cd build/Release && timeout 220 ./Riftwalker.exe --visual-test 2>&1 | grep -E "capture|Complete" || true)

CAPTURED=$(ls build/Release/screenshots/visual_test/*.png 2>/dev/null | wc -l)
if [[ "$CAPTURED" -lt 17 ]]; then
    echo "ERROR: Expected 17 screenshots, got $CAPTURED. Visual test crashed or stalled."
    exit 1
fi

# ------- Step 3a: accept mode -------
if $ACCEPT; then
    echo "[3/3] Accepting new baseline..."
    rm -f tests/visual_reference/*.png
    cp build/Release/screenshots/visual_test/*.png tests/visual_reference/
    echo "OK: Baseline updated ($CAPTURED screenshots)."
    exit 0
fi

# ------- Step 3b: compare mode -------
echo "[3/3] Comparing against baseline..."
FAILED=0
NEW=0
for actual in build/Release/screenshots/visual_test/*.png; do
    name=$(basename "$actual")
    ref="tests/visual_reference/$name"
    if [[ ! -f "$ref" ]]; then
        echo "  NEW   $name"
        NEW=$((NEW+1))
        continue
    fi
    # Higher tolerance for animated screens (menu particles, pause pulse,
    # gameplay physics/time, debug overlay FPS, etc.)
    MAX_DIFF="0.1"
    if [[ "$name" =~ (menu|class_select|difficulty|gameplay|debug|pause|tutorial|credits|lore|bestiary|challenges) ]]; then
        MAX_DIFF="8.0"
    fi
    if ! ./build/Release/visual_diff.exe "$ref" "$actual" /dev/null \
            --threshold 0.1 --max-diff "$MAX_DIFF" > /tmp/diff_$$ 2>&1; then
        PCT=$(grep -o 'Different pixels: [0-9]* ([0-9.]*%)' /tmp/diff_$$ || echo "diff")
        echo "  FAIL  $name  $PCT"
        FAILED=$((FAILED+1))
    fi
done
rm -f /tmp/diff_$$

echo ""
if [[ "$FAILED" -eq 0 && "$NEW" -eq 0 ]]; then
    echo "PASS: All 17 screens match the baseline. Game looks correct."
    exit 0
fi

if [[ "$FAILED" -gt 0 ]]; then
    echo "FAIL: $FAILED screen(s) look different from baseline."
fi
if [[ "$NEW" -gt 0 ]]; then
    echo "NEW:  $NEW screen(s) have no baseline yet."
fi

echo ""
echo "Generating interactive HTML report with side-by-side comparison..."
python tools/visual_report.py \
    --ref tests/visual_reference \
    --actual build/Release/screenshots/visual_test \
    --diff-tool build/Release/visual_diff.exe \
    --threshold 0.1 \
    --output visual_report.html
echo ""
echo "Report written to: visual_report.html"
echo "Open it in a browser to see exactly what changed, then either:"
echo "  - Fix the code if the change was unintended"
echo "  - Run ./check_visuals.sh --accept if the change is intentional"
exit 1
