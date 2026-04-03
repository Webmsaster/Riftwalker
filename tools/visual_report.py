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
    """Build a self-contained HTML report with interactive comparison modes."""
    import json

    total = len(results)
    passed = sum(1 for r in results if r["passed"])
    failed = sum(1 for r in results if not r["passed"] and r["error"] is None)
    errors = sum(1 for r in results if r["error"] is not None)
    all_passed = failed == 0 and errors == 0

    # Build JSON data for JavaScript
    js_data = []
    for i, r in enumerate(results):
        js_data.append({
            "id": i,
            "name": r["name"],
            "passed": r["passed"],
            "error": r["error"],
            "diff_pixels": r["diff_pixels"],
            "diff_percent": round(r["diff_percent"], 2),
            "width": r["width"],
            "height": r["height"],
            "ref": r.get("ref_b64", ""),
            "actual": r.get("actual_b64", ""),
            "diff": r.get("diff_b64", ""),
        })

    data_json = json.dumps(js_data)

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
    background: #121220; color: #e0e0e0;
    padding: 24px 32px; line-height: 1.5;
  }}
  h1 {{ font-size: 22px; font-weight: 700; margin-bottom: 4px; }}
  .subtitle {{ font-size: 13px; color: #888; margin-bottom: 16px; }}

  /* Toolbar */
  .toolbar {{
    display: flex; gap: 8px; align-items: center; flex-wrap: wrap;
    margin-bottom: 20px; padding: 10px 14px;
    background: #1e1e2e; border-radius: 8px; border: 1px solid #333;
  }}
  .toolbar label {{ font-size: 12px; color: #888; text-transform: uppercase;
                    letter-spacing: 1px; margin-right: 4px; }}
  .btn {{
    padding: 5px 12px; border-radius: 4px; border: 1px solid #444;
    background: #2a2a3e; color: #ccc; font-size: 12px; cursor: pointer;
    transition: all .15s;
  }}
  .btn:hover {{ background: #3a3a4e; color: #fff; }}
  .btn.active {{ background: #6366f1; color: #fff; border-color: #818cf8; }}
  .sep {{ width: 1px; height: 24px; background: #333; margin: 0 6px; }}
  .kbd {{ font-size: 10px; color: #666; margin-left: 2px; }}

  /* Summary */
  .summary {{
    display: flex; align-items: center; gap: 16px;
    padding: 14px 20px; border-radius: 8px; margin-bottom: 20px;
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

  /* Card */
  .card {{
    border: 3px solid #333; border-radius: 8px;
    margin-bottom: 24px; background: #1e1e2e; overflow: hidden;
  }}
  .card.pass {{ border-color: #22c55e; }}
  .card.fail {{ border-color: #ef4444; }}
  .card.error {{ border-color: #f59e0b; }}
  .card-header {{
    display: flex; align-items: center; gap: 12px; padding: 12px 16px;
    background: #2a2a3e; border-bottom: 1px solid #333; cursor: pointer;
  }}
  .card-header:hover {{ background: #323248; }}
  .badge {{
    color: #fff; font-weight: 700; padding: 3px 10px;
    border-radius: 4px; font-size: 13px;
  }}
  .badge-pass {{ background: #22c55e; }}
  .badge-fail {{ background: #ef4444; }}
  .badge-error {{ background: #f59e0b; }}
  .card-body {{ padding: 16px; }}

  /* Side-by-side grid */
  .grid3 {{ display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 12px; }}
  .grid3 img {{ max-width: 100%; height: auto; border-radius: 4px; cursor: zoom-in; }}
  .img-label {{
    font-size: 11px; color: #666; text-transform: uppercase;
    letter-spacing: 1px; margin-bottom: 4px; text-align: center;
  }}

  /* Slider mode */
  .slider-wrap {{
    position: relative; overflow: hidden; border-radius: 4px;
    cursor: col-resize; user-select: none;
  }}
  .slider-wrap img {{
    display: block; width: 100%; height: auto;
    pointer-events: none;
  }}
  .slider-actual {{
    position: absolute; top: 0; left: 0; right: 0; bottom: 0;
    overflow: hidden;
  }}
  .slider-line {{
    position: absolute; top: 0; bottom: 0; width: 3px;
    background: #fff; z-index: 10; pointer-events: none;
    box-shadow: 0 0 8px rgba(0,0,0,.5);
  }}
  .slider-label {{
    position: absolute; top: 8px; padding: 2px 8px;
    background: rgba(0,0,0,.7); color: #fff; font-size: 11px;
    border-radius: 3px; pointer-events: none; z-index: 11;
  }}
  .slider-label-l {{ left: 8px; }}
  .slider-label-r {{ right: 8px; }}

  /* Toggle flicker */
  .toggle-wrap {{ position: relative; border-radius: 4px; overflow: hidden; }}
  .toggle-wrap img {{ display: block; width: 100%; height: auto; }}
  .toggle-badge {{
    position: absolute; top: 8px; left: 8px; padding: 3px 10px;
    background: rgba(0,0,0,.7); color: #fff; font-size: 12px;
    font-weight: 600; border-radius: 4px; z-index: 5;
  }}

  /* Zoom overlay */
  .zoom-overlay {{
    display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,.9); z-index: 1000;
    cursor: zoom-out; overflow: auto;
  }}
  .zoom-overlay.active {{ display: flex; align-items: center; justify-content: center; }}
  .zoom-overlay img {{ max-width: 95vw; max-height: 95vh; border-radius: 4px; }}
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

  <div class="toolbar">
    <label>View:</label>
    <button class="btn active" onclick="setMode('side')" id="btn-side">Side-by-Side <span class="kbd">1</span></button>
    <button class="btn" onclick="setMode('slider')" id="btn-slider">Slider <span class="kbd">2</span></button>
    <button class="btn" onclick="setMode('toggle')" id="btn-toggle">Flicker <span class="kbd">3</span></button>
    <button class="btn" onclick="setMode('diff')" id="btn-diff">Diff Only <span class="kbd">4</span></button>
    <div class="sep"></div>
    <label>Filter:</label>
    <button class="btn active" onclick="setFilter('all')" id="btn-all">All</button>
    <button class="btn" onclick="setFilter('fail')" id="btn-fail-f">Failed</button>
    <button class="btn" onclick="setFilter('pass')" id="btn-pass-f">Passed</button>
  </div>

  <div id="cards"></div>

  <div class="zoom-overlay" id="zoom" onclick="closeZoom()">
    <img id="zoom-img" src="" alt="Zoom">
  </div>

  <div style="text-align:center;font-size:12px;color:#555;margin-top:32px;
              padding-top:16px;border-top:1px solid #2a2a3e;">
    Generated by visual_report.py &mdash; Riftwalker Visual Regression
  </div>

<script>
const DATA = {data_json};
let currentMode = 'side';
let currentFilter = 'all';
let flickerTimers = {{}};

function setMode(mode) {{
  currentMode = mode;
  document.querySelectorAll('.toolbar .btn').forEach(b => {{
    if (b.id.startsWith('btn-side') || b.id.startsWith('btn-slider') ||
        b.id.startsWith('btn-toggle') || b.id.startsWith('btn-diff'))
      b.classList.toggle('active', b.id === 'btn-' + mode);
  }});
  stopAllFlickers();
  renderCards();
}}

function setFilter(f) {{
  currentFilter = f;
  document.querySelectorAll('.toolbar .btn').forEach(b => {{
    if (b.id === 'btn-all' || b.id === 'btn-fail-f' || b.id === 'btn-pass-f')
      b.classList.toggle('active',
        (f === 'all' && b.id === 'btn-all') ||
        (f === 'fail' && b.id === 'btn-fail-f') ||
        (f === 'pass' && b.id === 'btn-pass-f'));
  }});
  stopAllFlickers();
  renderCards();
}}

function stopAllFlickers() {{
  Object.values(flickerTimers).forEach(clearInterval);
  flickerTimers = {{}};
}}

function filtered() {{
  return DATA.filter(d => {{
    if (currentFilter === 'fail') return !d.passed;
    if (currentFilter === 'pass') return d.passed && !d.error;
    return true;
  }});
}}

function renderCards() {{
  const container = document.getElementById('cards');
  container.innerHTML = '';
  filtered().forEach(d => container.appendChild(buildCard(d)));
}}

function buildCard(d) {{
  const card = document.createElement('div');
  card.className = 'card ' + (d.error ? 'error' : d.passed ? 'pass' : 'fail');

  const badgeClass = d.error ? 'badge-error' : d.passed ? 'badge-pass' : 'badge-fail';
  const badgeText = d.error ? 'ERROR' : d.passed ? 'PASS' : 'FAIL';
  const statusColor = d.error ? '#f59e0b' : d.passed ? '#22c55e' : '#ef4444';
  const statusText = d.error ? d.error
    : `Diff: ${{d.diff_percent}}% (${{d.diff_pixels}} px)`;
  const sizeText = (d.width && d.height) ? ` &mdash; ${{d.width}}x${{d.height}}` : '';

  const header = document.createElement('div');
  header.className = 'card-header';
  header.innerHTML = `
    <span class="badge ${{badgeClass}}">${{badgeText}}</span>
    <span style="font-weight:600;font-size:15px;">${{d.name}}</span>
    <span style="margin-left:auto;font-size:13px;">
      <span style="color:${{statusColor}}">${{statusText}}</span>${{sizeText}}
    </span>`;
  card.appendChild(header);

  if (d.error || !d.ref || !d.actual) {{ return card; }}

  const body = document.createElement('div');
  body.className = 'card-body';

  if (currentMode === 'side') {{
    body.innerHTML = buildSideView(d);
  }} else if (currentMode === 'slider') {{
    body.innerHTML = buildSliderView(d);
    requestAnimationFrame(() => initSlider(d.id));
  }} else if (currentMode === 'toggle') {{
    body.innerHTML = buildToggleView(d);
    requestAnimationFrame(() => initFlicker(d.id));
  }} else if (currentMode === 'diff') {{
    body.innerHTML = buildDiffView(d);
  }}

  card.appendChild(body);
  return card;
}}

function buildSideView(d) {{
  const diffCell = d.diff
    ? `<img src="${{d.diff}}" alt="Diff" onclick="openZoom(this.src)">`
    : '<span style="color:#666;">No diff</span>';
  return `<div class="grid3">
    <div><div class="img-label">Reference</div>
      <img src="${{d.ref}}" alt="Reference" onclick="openZoom(this.src)"></div>
    <div><div class="img-label">Actual</div>
      <img src="${{d.actual}}" alt="Actual" onclick="openZoom(this.src)"></div>
    <div><div class="img-label">Diff</div>${{diffCell}}</div>
  </div>`;
}}

function buildSliderView(d) {{
  return `<div class="slider-wrap" id="slider-${{d.id}}" style="max-width:100%;">
    <img src="${{d.ref}}" alt="Reference" style="width:100%;display:block;">
    <div class="slider-actual" id="slider-clip-${{d.id}}">
      <img src="${{d.actual}}" alt="Actual" style="width:100%;display:block;">
    </div>
    <div class="slider-line" id="slider-line-${{d.id}}"></div>
    <span class="slider-label slider-label-l">Reference</span>
    <span class="slider-label slider-label-r">Actual</span>
  </div>`;
}}

function initSlider(id) {{
  const wrap = document.getElementById('slider-' + id);
  if (!wrap) return;
  const clip = document.getElementById('slider-clip-' + id);
  const line = document.getElementById('slider-line-' + id);
  let pos = 0.5;

  function update(x) {{
    const rect = wrap.getBoundingClientRect();
    pos = Math.max(0, Math.min(1, (x - rect.left) / rect.width));
    clip.style.clipPath = `inset(0 0 0 ${{pos * 100}}%)`;
    line.style.left = (pos * 100) + '%';
  }}

  update(wrap.getBoundingClientRect().left + wrap.offsetWidth * 0.5);

  let dragging = false;
  wrap.addEventListener('mousedown', e => {{ dragging = true; update(e.clientX); }});
  document.addEventListener('mousemove', e => {{ if (dragging) update(e.clientX); }});
  document.addEventListener('mouseup', () => {{ dragging = false; }});
  wrap.addEventListener('touchstart', e => {{ update(e.touches[0].clientX); }});
  wrap.addEventListener('touchmove', e => {{ update(e.touches[0].clientX); e.preventDefault(); }});
}}

function buildToggleView(d) {{
  return `<div class="toggle-wrap" id="toggle-${{d.id}}">
    <span class="toggle-badge" id="toggle-label-${{d.id}}">Reference</span>
    <img id="toggle-img-${{d.id}}" src="${{d.ref}}" alt="Toggle" onclick="openZoom(this.src)">
  </div>
  <div style="text-align:center;margin-top:8px;font-size:12px;color:#666;">
    Flickers every 500ms &mdash; click image to zoom
  </div>`;
}}

function initFlicker(id) {{
  let showRef = true;
  const img = document.getElementById('toggle-img-' + id);
  const label = document.getElementById('toggle-label-' + id);
  const d = DATA[id];
  if (!img || !d) return;
  flickerTimers[id] = setInterval(() => {{
    showRef = !showRef;
    img.src = showRef ? d.ref : d.actual;
    label.textContent = showRef ? 'Reference' : 'Actual';
    label.style.background = showRef ? 'rgba(34,197,94,.7)' : 'rgba(99,102,241,.7)';
  }}, 500);
}}

function buildDiffView(d) {{
  if (!d.diff) return '<div style="text-align:center;color:#666;padding:20px;">No differences detected</div>';
  return `<div style="text-align:center;">
    <img src="${{d.diff}}" alt="Diff" style="max-width:100%;border-radius:4px;cursor:zoom-in;"
         onclick="openZoom(this.src)">
    <div style="font-size:12px;color:#666;margin-top:8px;">
      Red = changed pixels, Yellow = anti-aliased
    </div>
  </div>`;
}}

function openZoom(src) {{
  const overlay = document.getElementById('zoom');
  document.getElementById('zoom-img').src = src;
  overlay.classList.add('active');
}}

function closeZoom() {{
  document.getElementById('zoom').classList.remove('active');
}}

document.addEventListener('keydown', e => {{
  if (e.key === '1') setMode('side');
  else if (e.key === '2') setMode('slider');
  else if (e.key === '3') setMode('toggle');
  else if (e.key === '4') setMode('diff');
  else if (e.key === 'Escape') closeZoom();
}});

renderCards();
</script>
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
