"""AI-powered visual review of game screenshots using Claude's vision.

Sends screenshots to Claude and asks it to analyze them for visual bugs,
rendering issues, UI problems, and aesthetic consistency.

Usage:
    python tools/ai_visual_review.py screenshots/visual_test/
    python tools/ai_visual_review.py screenshots/visual_test/ --dry-run
    python tools/ai_visual_review.py screenshots/visual_test/ --model claude-sonnet-4-6
    python tools/ai_visual_review.py screenshots/ --output report.md
"""
import argparse
import base64
import os
import sys
import time
from pathlib import Path

DEFAULT_MODEL = "claude-sonnet-4-6"

ANALYSIS_PROMPT = """\
You are a QA visual reviewer for a 2D roguelike platformer game called "Riftwalker" \
built with C++17 and SDL2. Analyze the following game screenshot and report any issues.

Check for the following categories and provide a brief assessment for each:

1. **Sprite/Entity Rendering** — Are all sprites rendered correctly? Any missing textures, \
stretched/squished sprites, or entities rendered as colored rectangles when they should have \
proper art? Any sprites that appear cut off or partially invisible?

2. **UI/HUD Readability** — Is the HUD (health bar, XP bar, floor counter, weapon info, \
entropy meter) visible and properly positioned? Can all text be read clearly? Are UI \
elements properly anchored to screen edges?

3. **Rendering Artifacts** — Any z-order issues (entities drawn in wrong order, UI behind \
game world)? Flickering, tearing, or ghost images? Black or white rectangles that \
shouldn't be there?

4. **Color Palette & Consistency** — Does the color scheme look intentional and consistent? \
Any jarring neon colors on otherwise muted backgrounds? Do dimension shifts (if visible) \
have appropriate color theming?

5. **Text Overlap & Clipping** — Any text overlapping other text? Damage numbers colliding \
with HUD? Notification banners clipping off-screen? Text extending beyond its background box?

6. **General Composition** — Does the overall scene look like a playable game? Is the camera \
positioned well? Is the player character visible and distinguishable from the background?

For each category, respond with one of:
- ✅ OK — no issues found
- ⚠️ MINOR — cosmetic issue, not gameplay-affecting
- ❌ ISSUE — clear visual bug that should be fixed

End with a brief **Summary** listing the most important findings (if any).
"""


def collect_screenshots(directory: str) -> list[Path]:
    """Find all PNG files in the given directory (non-recursive)."""
    dir_path = Path(directory)
    if not dir_path.is_dir():
        print(f"ERROR: '{directory}' is not a directory.", file=sys.stderr)
        sys.exit(1)

    pngs = sorted(dir_path.glob("*.png"))
    if not pngs:
        print(f"WARNING: No PNG files found in '{directory}'.", file=sys.stderr)
    return pngs


def load_image_base64(path: Path) -> str:
    """Read a PNG file and return its base64-encoded content."""
    with open(path, "rb") as f:
        return base64.standard_b64encode(f.read()).decode("ascii")


def analyze_screenshot(client, model: str, image_path: Path) -> str:
    """Send a single screenshot to Claude for visual analysis."""
    image_data = load_image_base64(image_path)
    file_size_kb = image_path.stat().st_size / 1024

    message = client.messages.create(
        model=model,
        max_tokens=1500,
        messages=[
            {
                "role": "user",
                "content": [
                    {
                        "type": "image",
                        "source": {
                            "type": "base64",
                            "media_type": "image/png",
                            "data": image_data,
                        },
                    },
                    {
                        "type": "text",
                        "text": ANALYSIS_PROMPT,
                    },
                ],
            }
        ],
    )

    # Extract text from the response
    return message.content[0].text


def format_report(results: list[dict]) -> str:
    """Format the collected results into a markdown report."""
    lines = []
    lines.append("# Riftwalker Visual Review Report")
    lines.append("")
    lines.append(f"**Date:** {time.strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(f"**Screenshots analyzed:** {len(results)}")
    lines.append("")

    # Count severity across all results
    total_ok = 0
    total_minor = 0
    total_issue = 0
    for r in results:
        text = r.get("analysis", "")
        total_ok += text.count("OK")
        total_minor += text.count("MINOR")
        total_issue += text.count("ISSUE")

    lines.append("## Summary")
    lines.append("")
    if total_issue > 0:
        lines.append(f"- **{total_issue}** issues found that should be fixed")
    if total_minor > 0:
        lines.append(f"- **{total_minor}** minor cosmetic issues")
    if total_ok > 0:
        lines.append(f"- **{total_ok}** checks passed")
    if total_issue == 0 and total_minor == 0:
        lines.append("No visual issues detected across all screenshots.")
    lines.append("")
    lines.append("---")
    lines.append("")

    for i, r in enumerate(results, 1):
        lines.append(f"## Screenshot {i}: `{r['filename']}`")
        lines.append("")
        size_kb = r.get("size_kb", 0)
        lines.append(f"*File size: {size_kb:.1f} KB*")
        lines.append("")
        if "error" in r:
            lines.append(f"> **Error:** {r['error']}")
        else:
            lines.append(r["analysis"])
        lines.append("")
        lines.append("---")
        lines.append("")

    return "\n".join(lines)


def dry_run_report(screenshots: list[Path]) -> str:
    """Generate a dry-run report listing what would be analyzed."""
    lines = []
    lines.append("# Riftwalker Visual Review — Dry Run")
    lines.append("")
    lines.append(f"**Screenshots found:** {len(screenshots)}")
    lines.append("")

    total_size = 0
    for png in screenshots:
        size_kb = png.stat().st_size / 1024
        total_size += size_kb
        lines.append(f"- `{png.name}` ({size_kb:.1f} KB)")

    lines.append("")
    lines.append(f"**Total size:** {total_size:.1f} KB")
    lines.append("")
    lines.append("Run without `--dry-run` to perform the actual analysis.")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="AI visual review of Riftwalker screenshots using Claude's vision"
    )
    parser.add_argument(
        "directory",
        help="Directory containing PNG screenshots to analyze",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="List screenshots without sending to the API",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help=f"Claude model to use (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--output",
        "-o",
        default=None,
        help="Write the report to a file instead of stdout",
    )
    args = parser.parse_args()

    screenshots = collect_screenshots(args.directory)
    if not screenshots:
        print("No screenshots to analyze. Exiting.", file=sys.stderr)
        sys.exit(0)

    # --- Dry run ---
    if args.dry_run:
        report = dry_run_report(screenshots)
        if args.output:
            Path(args.output).parent.mkdir(parents=True, exist_ok=True)
            Path(args.output).write_text(report, encoding="utf-8")
            print(f"Dry-run report written to {args.output}")
        else:
            print(report)
        return

    # --- Real run: check for API key ---
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print(
            "ERROR: ANTHROPIC_API_KEY environment variable is not set.\n"
            "\n"
            "To use this tool, set your API key:\n"
            "  export ANTHROPIC_API_KEY=sk-ant-...\n"
            "\n"
            "Or on Windows:\n"
            "  set ANTHROPIC_API_KEY=sk-ant-...\n"
            "\n"
            "You can get an API key at https://console.anthropic.com/\n"
            "\n"
            "Tip: Use --dry-run to preview which screenshots would be analyzed.",
            file=sys.stderr,
        )
        sys.exit(1)

    try:
        from anthropic import Anthropic
    except ImportError:
        print(
            "ERROR: The 'anthropic' Python package is not installed.\n"
            "\n"
            "Install it with:\n"
            "  pip install anthropic\n",
            file=sys.stderr,
        )
        sys.exit(1)

    client = Anthropic()

    print(f"Analyzing {len(screenshots)} screenshot(s) with {args.model}...",
          file=sys.stderr)

    results = []
    for i, png in enumerate(screenshots, 1):
        filename = png.name
        size_kb = png.stat().st_size / 1024
        print(f"  [{i}/{len(screenshots)}] {filename} ({size_kb:.1f} KB)...",
              file=sys.stderr, end="", flush=True)

        entry = {"filename": filename, "size_kb": size_kb}
        try:
            analysis = analyze_screenshot(client, args.model, png)
            entry["analysis"] = analysis
            print(" done.", file=sys.stderr)
        except Exception as e:
            entry["error"] = str(e)
            print(f" ERROR: {e}", file=sys.stderr)

        results.append(entry)

    report = format_report(results)

    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(report, encoding="utf-8")
        print(f"\nReport written to {args.output}", file=sys.stderr)
    else:
        print(report)


if __name__ == "__main__":
    main()
