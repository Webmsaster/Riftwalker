#!/usr/bin/env python3
"""visual_report.py -- Generate an HTML visual regression report.

Compares reference screenshots against actual screenshots using visual_diff.exe,
then produces a self-contained HTML report with embedded base64 images.

Usage:
    python tools/visual_report.py \
        --ref tests/visual_reference \
        --actual build/Release/screenshots/visual_test \
        --diff-tool build/Release/visual_diff.exe \
        --output visual_report.html
"""

import argparse
import base64
import os
import re
import subprocess
import sys
import tempfile
import webbrowser
from datetime import datetime
from pathlib import Path


def png_to_base64(path: str) -> str:
    """Read a PNG file and return a base64 data-URI string."""
    with open(path, "rb") as f:
        encoded = base64.b64encode(f.read()).decode("ascii")
    return f"data:image/png;base64,{encoded}"


def run_diff(diff_tool: str, ref_path: str, actual_path: str,
             diff_output: str, threshold: float) -> dict:
    """Run visual_diff.exe on a pair of images and parse stdout.

    Returns a dict with keys: passed, diff_pixels, diff_percent, total_pixels,
    width, height, error.
    """
    result = {
        "passed": False,
        "diff_pixels": 0,
        "diff_percent": 0.0,
        "total_pixels": 0,
        "width": 0,
        "height": 0,
        "error": None,
    }

    cmd = [diff_tool, ref_path, actual_path, diff_output,
           "--threshold", str(threshold)]
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=30
        )
    except FileNotFoundError:
        result["error"] = f"Diff tool not found: {diff_tool}"
        return result
    except subprocess.TimeoutExpired:
        result["error"] = "Diff tool timed out (30s)"
        return result

    stdout = proc.stdout

    # Parse structured output from visual_diff
    size_match = re.search(r"Size:\s*(\d+)x(\d+)\s*\((\d+)\s*pixels\)", stdout)
    if size_match:
        result["width"] = int(size_match.group(1))
        result["height"] = int(size_match.group(2))
        result["total_pixels"] = int(size_match.group(3))

    diff_match = re.search(
        r"Different pixels:\s*(\d+)\s*\(([\d.]+)%\)", stdout
    )
    if diff_match:
        result["diff_pixels"] = int(diff_match.group(1))
        result["diff_percent"] = float(diff_match.group(2))

    if proc.returncode == 0:
        result["passed"] = True
    elif proc.returncode == 1:
        result["passed"] = False
    else:
        # returncode 2 = error
        stderr_msg = proc.stderr.strip() or stdout.strip()
        result["error"] = stderr_msg or f"Exit code {proc.returncode}"

    return result


def build_html(results: list, timestamp: str) -> str:
    """Build a self-contained HTML report string from comparison results."""
    total = len(results)
    passed = sum(1 for r in results if r["passed"])
    failed = sum(1 for r in results if not r["passed"] and r["error"] is None)
    errors = sum(1 for r in results if r["error"] is not None)

    all_passed = failed == 0 and errors == 0

    # Build the card rows
    cards_html = []
    for r in results:
        name = r["name"]
        if r["error"]:
            border_color = "#f59e0b"  # amber for errors
            badge_bg = "#f59e0b"
            badge_text = "ERROR"
            status_detail = f'<span style="color:#f59e0b;">{r["error"]}</span>'
        elif r["passed"]:
            border_color = "#22c55e"
            badge_bg = "#22c55e"
            badge_text = "PASS"
            status_detail = (
                f'<span style="color:#22c55e;">Diff: {r["diff_percent"]:.2f}%'
                f' ({r["diff_pixels"]} px)</span>'
            )
        else:
            border_color = "#ef4444"
            badge_bg = "#ef4444"
            badge_text = "FAIL"
            status_detail = (
                f'<span style="color:#ef4444;">Diff: {r["diff_percent"]:.2f}%'
                f' ({r["diff_pixels"]} px)</span>'
            )

        # Image cells
        ref_img = (
            f'<img src="{r["ref_b64"]}" alt="Reference" '
            f'style="max-width:100%;height:auto;border-radius:4px;">'
            if r.get("ref_b64") else
            '<span style="color:#888;">N/A</span>'
        )
        actual_img = (
            f'<img src="{r["actual_b64"]}" alt="Actual" '
            f'style="max-width:100%;height:auto;border-radius:4px;">'
            if r.get("actual_b64") else
            '<span style="color:#888;">N/A</span>'
        )
        diff_img = (
            f'<img src="{r["diff_b64"]}" alt="Diff" '
            f'style="max-width:100%;height:auto;border-radius:4px;">'
            if r.get("diff_b64") else
            '<span style="color:#888;">No diff image</span>'
        )

        size_info = ""
        if r["width"] and r["height"]:
            size_info = f' &mdash; {r["width"]}x{r["height"]}'

        card = f"""
        <div style="border:3px solid {border_color};border-radius:8px;
                    margin-bottom:24px;background:#1e1e2e;overflow:hidden;">
          <div style="display:flex;align-items:center;gap:12px;padding:12px 16px;
                      background:#2a2a3e;border-bottom:1px solid #333;">
            <span style="background:{badge_bg};color:#fff;font-weight:700;
                         padding:3px 10px;border-radius:4px;font-size:13px;">
              {badge_text}
            </span>
            <span style="font-weight:600;font-size:15px;color:#e0e0e0;">
              {name}
            </span>
            <span style="margin-left:auto;font-size:13px;">
              {status_detail}{size_info}
            </span>
          </div>
          <div style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;
                      padding:16px;">
            <div style="text-align:center;">
              <div style="font-size:12px;color:#888;margin-bottom:6px;
                          text-transform:uppercase;letter-spacing:1px;">
                Reference
              </div>
              {ref_img}
            </div>
            <div style="text-align:center;">
              <div style="font-size:12px;color:#888;margin-bottom:6px;
                          text-transform:uppercase;letter-spacing:1px;">
                Actual
              </div>
              {actual_img}
            </div>
            <div style="text-align:center;">
              <div style="font-size:12px;color:#888;margin-bottom:6px;
                          text-transform:uppercase;letter-spacing:1px;">
                Diff
              </div>
              {diff_img}
            </div>
          </div>
        </div>
        """
        cards_html.append(card)

    # Summary bar colors
    if all_passed:
        summary_bg = "#064e3b"
        summary_border = "#22c55e"
        summary_icon = "&#10004;"
        summary_text = "All tests passed"
    else:
        summary_bg = "#450a0a"
        summary_border = "#ef4444"
        summary_icon = "&#10006;"
        summary_text = f"{failed} failed, {errors} error(s)"

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Visual Regression Report &mdash; {timestamp}</title>
<style>
  * {{ margin:0; padding:0; box-sizing:border-box; }}
  body {{
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto,
                 "Helvetica Neue", Arial, sans-serif;
    background: #121220;
    color: #e0e0e0;
    padding: 24px 32px;
    line-height: 1.5;
  }}
  h1 {{ font-size: 22px; font-weight: 700; margin-bottom: 4px; }}
  .subtitle {{ font-size: 13px; color: #888; margin-bottom: 20px; }}
  .summary {{
    display: flex; align-items: center; gap: 16px;
    padding: 14px 20px; border-radius: 8px; margin-bottom: 28px;
    background: {summary_bg}; border: 2px solid {summary_border};
  }}
  .summary-icon {{ font-size: 22px; }}
  .summary-text {{ font-weight: 600; font-size: 15px; }}
  .stat {{
    display: inline-block; padding: 4px 12px; border-radius: 4px;
    font-size: 13px; font-weight: 600; margin-left: 8px;
  }}
  .stat-total {{ background: #334155; color: #cbd5e1; }}
  .stat-pass {{ background: #064e3b; color: #4ade80; }}
  .stat-fail {{ background: #450a0a; color: #f87171; }}
  .stat-error {{ background: #451a03; color: #fbbf24; }}
</style>
</head>
<body>
  <h1>Visual Regression Report</h1>
  <div class="subtitle">{timestamp}</div>

  <div class="summary">
    <span class="summary-icon">{summary_icon}</span>
    <span class="summary-text">{summary_text}</span>
    <span style="margin-left:auto;">
      <span class="stat stat-total">{total} total</span>
      <span class="stat stat-pass">{passed} passed</span>
      <span class="stat stat-fail">{failed} failed</span>
      {"" if errors == 0 else f'<span class="stat stat-error">{errors} errors</span>'}
    </span>
  </div>

  {"".join(cards_html)}

  <div style="text-align:center;font-size:12px;color:#555;margin-top:32px;
              padding-top:16px;border-top:1px solid #2a2a3e;">
    Generated by visual_report.py &mdash; Riftwalker Visual Regression
  </div>
</body>
</html>"""
    return html


def main():
    parser = argparse.ArgumentParser(
        description="Generate an HTML visual regression report."
    )
    parser.add_argument(
        "--ref", required=True,
        help="Directory containing reference PNG screenshots."
    )
    parser.add_argument(
        "--actual", required=True,
        help="Directory containing actual/current PNG screenshots."
    )
    parser.add_argument(
        "--diff-tool", required=True,
        help="Path to visual_diff.exe."
    )
    parser.add_argument(
        "--output", default="visual_report.html",
        help="Output HTML file path (default: visual_report.html)."
    )
    parser.add_argument(
        "--threshold", type=float, default=0.1,
        help="Pixel comparison threshold for visual_diff (default: 0.1)."
    )
    parser.add_argument(
        "--no-open", action="store_true",
        help="Do not open the report in the default browser."
    )
    args = parser.parse_args()

    ref_dir = Path(args.ref).resolve()
    actual_dir = Path(args.actual).resolve()
    diff_tool = Path(args.diff_tool).resolve()
    output_path = Path(args.output).resolve()

    # Validate inputs
    if not ref_dir.is_dir():
        print(f"ERROR: Reference directory not found: {ref_dir}", file=sys.stderr)
        sys.exit(2)
    if not actual_dir.is_dir():
        print(f"ERROR: Actual directory not found: {actual_dir}", file=sys.stderr)
        sys.exit(2)
    if not diff_tool.is_file():
        print(f"ERROR: Diff tool not found: {diff_tool}", file=sys.stderr)
        sys.exit(2)

    # Collect reference PNGs
    ref_pngs = sorted(ref_dir.glob("*.png"))
    if not ref_pngs:
        print(f"ERROR: No PNG files found in {ref_dir}", file=sys.stderr)
        sys.exit(2)

    print(f"Found {len(ref_pngs)} reference image(s) in {ref_dir}")
    print(f"Comparing against {actual_dir}")
    print()

    # Temporary directory for diff images
    tmp_dir = tempfile.mkdtemp(prefix="visual_report_")

    results = []
    any_failure = False

    for ref_png in ref_pngs:
        name = ref_png.name
        actual_png = actual_dir / name
        diff_png = os.path.join(tmp_dir, f"diff_{name}")

        entry = {
            "name": name,
            "ref_b64": png_to_base64(str(ref_png)),
            "actual_b64": None,
            "diff_b64": None,
            "passed": False,
            "diff_pixels": 0,
            "diff_percent": 0.0,
            "total_pixels": 0,
            "width": 0,
            "height": 0,
            "error": None,
        }

        if not actual_png.is_file():
            entry["error"] = f"Actual image not found: {actual_png.name}"
            print(f"  MISSING  {name}")
            any_failure = True
            results.append(entry)
            continue

        entry["actual_b64"] = png_to_base64(str(actual_png))

        # Run visual_diff
        diff_result = run_diff(
            str(diff_tool), str(ref_png), str(actual_png),
            diff_png, args.threshold
        )

        entry["passed"] = diff_result["passed"]
        entry["diff_pixels"] = diff_result["diff_pixels"]
        entry["diff_percent"] = diff_result["diff_percent"]
        entry["total_pixels"] = diff_result["total_pixels"]
        entry["width"] = diff_result["width"]
        entry["height"] = diff_result["height"]
        entry["error"] = diff_result["error"]

        # Load diff image if it was generated
        if os.path.isfile(diff_png):
            entry["diff_b64"] = png_to_base64(diff_png)

        if entry["error"]:
            print(f"  ERROR    {name}: {entry['error']}")
            any_failure = True
        elif entry["passed"]:
            print(f"  PASS     {name} ({entry['diff_percent']:.2f}%)")
        else:
            print(f"  FAIL     {name} ({entry['diff_percent']:.2f}%)")
            any_failure = True

        results.append(entry)

    # Also check for PNGs in actual that have no reference (new screens)
    actual_pngs = sorted(actual_dir.glob("*.png"))
    ref_names = {p.name for p in ref_pngs}
    for actual_png in actual_pngs:
        if actual_png.name not in ref_names:
            print(f"  NEW      {actual_png.name} (no reference)")

    # Generate HTML
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    html = build_html(results, timestamp)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)

    print()
    print(f"Report written to: {output_path}")

    # Clean up temp diff images
    for f in Path(tmp_dir).glob("*"):
        try:
            f.unlink()
        except OSError:
            pass
    try:
        os.rmdir(tmp_dir)
    except OSError:
        pass

    # Open in browser
    if not args.no_open:
        webbrowser.open(output_path.as_uri())

    # Exit code reflects overall result
    sys.exit(1 if any_failure else 0)


if __name__ == "__main__":
    main()
