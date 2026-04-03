#!/usr/bin/env bash
# visual-regression.sh — Visual regression testing for Riftwalker (Git Bash)
#
# Usage:
#   ./tools/visual-regression.sh                    # Compare against baselines
#   ./tools/visual-regression.sh --update           # Capture new baselines
#   ./tools/visual-regression.sh --report           # Compare + open HTML report
#   ./tools/visual-regression.sh --threshold 0.2    # Custom diff threshold
#
# Prerequisites: build/Release/Riftwalker.exe and build/Release/visual_diff.exe

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

EXE="$PROJECT_DIR/build/Release/Riftwalker.exe"
DIFF_TOOL="$PROJECT_DIR/build/Release/visual_diff.exe"
REF_DIR="$PROJECT_DIR/tests/visual_reference"
TEST_DIR="$PROJECT_DIR/build/Release/screenshots/visual_test"
REPORT_PY="$PROJECT_DIR/tools/visual_report.py"

UPDATE=false
REPORT=false
THRESHOLD=0.1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --update)  UPDATE=true; shift ;;
        --report)  REPORT=true; shift ;;
        --threshold) THRESHOLD="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 2 ;;
    esac
done

# Validate
if [[ ! -f "$EXE" ]]; then
    echo "ERROR: Game not built. Run: cmake --build build --config Release"
    exit 2
fi
if [[ ! -f "$DIFF_TOOL" ]]; then
    echo "ERROR: visual_diff.exe not found. Build the project first."
    exit 2
fi

# Step 1: Capture screenshots via --visual-test
echo "=== Capturing screenshots ==="
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$PROJECT_DIR/build/Release"
timeout 60 ./Riftwalker.exe --visual-test 2>/dev/null || true
cd "$PROJECT_DIR"

CAPTURES=$(find "$TEST_DIR" -name "*.png" 2>/dev/null | wc -l)
if [[ "$CAPTURES" -eq 0 ]]; then
    echo "ERROR: No screenshots captured. Check --visual-test mode."
    exit 2
fi
echo "Captured $CAPTURES screenshot(s)"

# Step 2a: Update mode — save as new baselines
if $UPDATE; then
    mkdir -p "$REF_DIR"
    cp -v "$TEST_DIR"/*.png "$REF_DIR/"
    echo ""
    echo "=== Baseline updated with $CAPTURES reference(s) ==="
    exit 0
fi

# Step 2b: Compare mode
if [[ ! -d "$REF_DIR" ]] || [[ -z "$(ls "$REF_DIR"/*.png 2>/dev/null)" ]]; then
    echo ""
    echo "No baselines found. Run with --update first to create references."
    exit 0
fi

# If --report, use Python HTML report
if $REPORT; then
    echo ""
    echo "=== Generating HTML report ==="
    python "$REPORT_PY" \
        --ref "$REF_DIR" \
        --actual "$TEST_DIR" \
        --diff-tool "$DIFF_TOOL" \
        --threshold "$THRESHOLD" \
        --output "$PROJECT_DIR/visual_report.html"
    exit $?
fi

# Simple CLI comparison
echo ""
echo "=== Comparing against baselines ==="
PASSED=0
FAILED=0
SKIPPED=0

for ref in "$REF_DIR"/*.png; do
    name=$(basename "$ref")
    actual="$TEST_DIR/$name"

    if [[ ! -f "$actual" ]]; then
        echo "  SKIP    $name (no actual)"
        ((SKIPPED++)) || true
        continue
    fi

    # Animated screens get higher tolerance
    MAX_DIFF="0.1"
    if [[ "$name" =~ (menu|class_select|gameplay|debug|pause) ]]; then
        MAX_DIFF="5.0"
    fi

    OUTPUT=$("$DIFF_TOOL" "$ref" "$actual" /dev/null --threshold "$THRESHOLD" --max-diff "$MAX_DIFF" 2>&1) || true
    EXIT_CODE=${PIPESTATUS[0]:-$?}

    if [[ "$EXIT_CODE" -eq 0 ]]; then
        PCTLINE=$(echo "$OUTPUT" | grep -o 'Different pixels: [0-9]* ([0-9.]*%)' || echo "")
        echo "  PASS    $name  $PCTLINE"
        ((PASSED++)) || true
    elif [[ "$EXIT_CODE" -eq 1 ]]; then
        PCTLINE=$(echo "$OUTPUT" | grep -o 'Different pixels: [0-9]* ([0-9.]*%)' || echo "")
        echo "  FAIL    $name  $PCTLINE"
        ((FAILED++)) || true
    else
        echo "  ERROR   $name (exit $EXIT_CODE)"
        ((FAILED++)) || true
    fi
done

# New screenshots without baselines
for actual in "$TEST_DIR"/*.png; do
    name=$(basename "$actual")
    if [[ ! -f "$REF_DIR/$name" ]]; then
        echo "  NEW     $name (no baseline)"
    fi
done

echo ""
echo "=== Visual Regression Summary ==="
echo "  Passed:  $PASSED"
echo "  Failed:  $FAILED"
echo "  Skipped: $SKIPPED"

if [[ "$FAILED" -gt 0 ]]; then
    echo ""
    echo "FAIL: $FAILED regression(s) detected!"
    echo "Run with --update to accept as new baseline."
    echo "Run with --report for interactive HTML comparison."
    exit 1
else
    echo ""
    echo "PASS: All visual tests passed."
    exit 0
fi
