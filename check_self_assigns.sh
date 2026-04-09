#!/usr/bin/env bash
# check_self_assigns.sh — Guard against the replace_all regression pattern
#
# Background: In the 2026-04-09 autonomous session, a replace_all operation
# `SDL_GetTicks() -> m_frameTicks` accidentally caught the init line too,
# turning `m_frameTicks = SDL_GetTicks();` into `m_frameTicks = m_frameTicks;`
# — a no-op self-assignment. That single character change froze every HUD
# pulse animation for 25 commits before being found.
#
# This script greps the src/ tree for any `x = x;` self-assignment pattern
# and returns non-zero if any match is found. It is safe to run as a
# pre-commit check or as part of the build pipeline.
#
# Usage:
#   ./check_self_assigns.sh         — scan and report
#   ./check_self_assigns.sh --ci    — exit 1 on any match (for CI pipelines)

set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

CI_MODE=false
if [[ "${1:-}" == "--ci" ]]; then
    CI_MODE=true
fi

echo "[check_self_assigns] Scanning src/ for 'x = x;' self-assignment patterns..."

# Match: optional whitespace, identifier, whitespace, =, whitespace, SAME identifier, ;
# Uses extended regex with a backreference (\1) to match only true self-assignments.
# Excludes: lines that contain operators/function calls (the second x must be alone).
MATCHES=$(grep -rn -E '^\s*(\w+)\s*=\s*\1\s*;' src/ 2>/dev/null || true)

if [[ -z "$MATCHES" ]]; then
    echo "[check_self_assigns] OK — no self-assignments found."
    exit 0
fi

echo ""
echo "=== SELF-ASSIGNMENTS FOUND ==="
echo ""
echo "$MATCHES"
echo ""
echo "Each line above is a 'x = x;' pattern. These are usually bugs — most"
echo "commonly from a replace_all operation that accidentally rewrote the init"
echo "line. Verify each is intentional (rare) or replace with the correct"
echo "right-hand-side expression."
echo ""

if $CI_MODE; then
    echo "[check_self_assigns] FAIL — self-assignments detected (CI mode)."
    exit 1
fi

echo "[check_self_assigns] WARN — self-assignments detected (report mode)."
exit 0
