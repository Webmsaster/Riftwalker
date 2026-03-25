#!/usr/bin/env python3
"""
Riftwalker Parallel Playtest Runner
====================================
Launches multiple playtest instances in parallel with different configurations,
then aggregates results into a combined report.

Usage:
    python tools/run_playtests.py                     # All 12 configs (3 classes x 4 profiles)
    python tools/run_playtests.py --quick              # 4 configs (1 class x 4 profiles, 3 runs each)
    python tools/run_playtests.py --class voidwalker   # 4 profiles for one class
    python tools/run_playtests.py --profile aggressive  # 3 classes with one profile
    python tools/run_playtests.py --runs 5             # Custom run count per config
    python tools/run_playtests.py --max-parallel 4     # Limit parallel processes
"""

import subprocess
import sys
import os
import time
import argparse
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed

CLASSES = ["voidwalker", "berserker", "phantom"]
PROFILES = ["balanced", "aggressive", "defensive", "speedrun"]

def find_exe():
    """Find the Riftwalker executable."""
    candidates = [
        Path("build/Release/Riftwalker.exe"),
        Path("build/Debug/Riftwalker.exe"),
        Path("build/Riftwalker.exe"),
    ]
    for c in candidates:
        if c.exists():
            return str(c)
    print("ERROR: Riftwalker.exe not found. Build first with: cmake --build build --config Release")
    sys.exit(1)

def run_single_playtest(exe, cls, profile, runs, seed, output_dir):
    """Run a single playtest configuration. Returns (config_name, output_file, duration, returncode)."""
    config_name = f"{cls}_{profile}"
    # Use absolute path so it works regardless of working directory
    output_file = os.path.abspath(os.path.join(output_dir, f"playtest_{config_name}.log"))

    cmd = [
        os.path.abspath(exe),
        "--playtest",
        f"--playtest-runs={runs}",
        f"--playtest-class={cls}",
        f"--playtest-profile={profile}",
        f"--playtest-output={output_file}",
    ]
    if seed >= 0:
        cmd.append(f"--seed={seed}")

    start = time.time()
    # Each run can take up to 45 minutes (bots need ~40-60s per floor, 30 floors max)
    timeout_sec = runs * 2700 + 300
    try:
        result = subprocess.run(cmd, capture_output=True, timeout=timeout_sec)
        duration = time.time() - start
        return config_name, output_file, duration, result.returncode
    except subprocess.TimeoutExpired:
        duration = time.time() - start
        return config_name, output_file, duration, -1

def extract_summary(output_file):
    """Extract key metrics from a playtest log."""
    if not os.path.exists(output_file):
        return None

    data = {
        "runs": 0, "deaths": 0, "best_floor": 0,
        "zone_deaths": [0]*5, "zone_kills": [0]*5, "zone_time": [0]*5,
        "death_floors": {},
    }

    in_report = False
    in_heatmap = False
    in_zone = False

    with open(output_file, "r") as f:
        for line in f:
            line = line.strip()
            if "PLAYTEST BALANCE REPORT" in line:
                in_report = True
            if in_report:
                if line.startswith("Runs:"):
                    parts = line.split("|")
                    for p in parts:
                        p = p.strip()
                        if p.startswith("Runs:"):
                            data["runs"] = int(p.split(":")[1].strip())
                        elif p.startswith("Deaths:"):
                            data["deaths"] = int(p.split(":")[1].strip())
                        elif p.startswith("Best Floor:"):
                            data["best_floor"] = int(p.split(":")[1].strip())
                if "DEATH HEATMAP" in line:
                    in_heatmap = True
                    continue
                if in_heatmap and line.startswith("F"):
                    for token in line.split():
                        if ":" in token and token.startswith("F"):
                            floor, count = token.split(":")
                            data["death_floors"][int(floor[1:])] = int(count)
                    in_heatmap = False
                if "ZONE SURVIVAL" in line:
                    in_zone = True
                    continue
                if in_zone and line.startswith("Zone"):
                    # Zone N (Name): X deaths | Y dmg taken | Z kills | Ts | F floors played
                    parts = line.split("|")
                    zone_num = int(line.split("(")[0].replace("Zone", "").strip()) - 1
                    if 0 <= zone_num < 5:
                        for p in parts:
                            p = p.strip()
                            if "deaths" in p:
                                data["zone_deaths"][zone_num] = int(p.split()[0])
                            elif "kills" in p:
                                data["zone_kills"][zone_num] = int(p.split()[0])

    return data

def generate_combined_report(results, output_dir):
    """Create a combined report from all playtest results."""
    report_path = os.path.join(output_dir, "COMBINED_REPORT.txt")
    with open(report_path, "w") as f:
        f.write("=" * 70 + "\n")
        f.write("  RIFTWALKER PARALLEL PLAYTEST - COMBINED REPORT\n")
        f.write(f"  Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("=" * 70 + "\n\n")

        # Summary table
        f.write(f"{'Config':<25} {'Runs':>5} {'Deaths':>7} {'Best':>5} {'Time':>8} {'Status':>8}\n")
        f.write("-" * 65 + "\n")

        total_runs = 0
        total_deaths = 0
        best_overall = 0
        all_death_floors = {}

        for config_name, output_file, duration, returncode in results:
            summary = extract_summary(output_file)
            status = "OK" if returncode == 0 else ("TIMEOUT" if returncode == -1 else f"ERR:{returncode}")

            if summary:
                total_runs += summary["runs"]
                total_deaths += summary["deaths"]
                best_overall = max(best_overall, summary["best_floor"])
                for floor, count in summary["death_floors"].items():
                    all_death_floors[floor] = all_death_floors.get(floor, 0) + count

                f.write(f"{config_name:<25} {summary['runs']:>5} {summary['deaths']:>7} "
                        f"{summary['best_floor']:>5} {duration:>7.0f}s {status:>8}\n")
            else:
                f.write(f"{config_name:<25} {'?':>5} {'?':>7} {'?':>5} {duration:>7.0f}s {status:>8}\n")

        f.write("-" * 65 + "\n")
        f.write(f"{'TOTAL':<25} {total_runs:>5} {total_deaths:>7} {best_overall:>5}\n")

        # Combined death heatmap
        f.write("\n\nDEATH HEATMAP (all configs combined):\n")
        f.write("-" * 50 + "\n")
        if all_death_floors:
            max_deaths = max(all_death_floors.values())
            for floor in sorted(all_death_floors.keys()):
                count = all_death_floors[floor]
                bar = "#" * int(count / max(max_deaths, 1) * 30)
                zone = (floor - 1) // 6 + 1
                f.write(f"  F{floor:>2} (Z{zone}): {count:>3} {bar}\n")
        else:
            f.write("  (no deaths recorded)\n")

        # Per-zone analysis
        f.write("\n\nZONE DIFFICULTY ANALYSIS:\n")
        f.write("-" * 50 + "\n")
        zone_names = ["Fractured Threshold", "Shifting Depths", "Resonant Core",
                       "Entropy Cascade", "Sovereign's Domain"]
        zone_deaths = [0] * 5
        for floor, count in all_death_floors.items():
            z = min((floor - 1) // 6, 4)
            zone_deaths[z] += count

        for z in range(5):
            if zone_deaths[z] > 0 or z < 3:
                bar = "#" * min(zone_deaths[z], 40)
                f.write(f"  Zone {z+1} ({zone_names[z]:<22}): {zone_deaths[z]:>3} deaths {bar}\n")

        # Balance recommendations
        f.write("\n\nBALANCE OBSERVATIONS:\n")
        f.write("-" * 50 + "\n")
        if best_overall < 6:
            f.write("  ! Zone 1 may be too hard — bots can't even reach Zone Boss\n")
        elif best_overall < 12:
            f.write("  ! Difficulty spikes in Zone 2 — consider smoother scaling\n")
        if total_deaths > 0:
            deadliest_floor = max(all_death_floors, key=all_death_floors.get) if all_death_floors else 0
            if deadliest_floor > 0:
                z = (deadliest_floor - 1) // 6 + 1
                f.write(f"  ! Deadliest floor: F{deadliest_floor} (Zone {z}) with {all_death_floors[deadliest_floor]} deaths\n")
        if total_runs > 0:
            death_rate = total_deaths / total_runs * 100
            f.write(f"  Overall death rate: {death_rate:.0f}%\n")

        f.write("\n" + "=" * 70 + "\n")

    return report_path

def main():
    parser = argparse.ArgumentParser(description="Riftwalker Parallel Playtest Runner")
    parser.add_argument("--quick", action="store_true", help="Quick mode: 4 profiles x 1 class, 3 runs each")
    parser.add_argument("--class", dest="cls", choices=CLASSES + ["all"], default="all",
                        help="Lock to one class (default: all)")
    parser.add_argument("--profile", choices=PROFILES + ["all"], default="all",
                        help="Lock to one profile (default: all)")
    parser.add_argument("--runs", type=int, default=5, help="Runs per configuration (default: 5)")
    parser.add_argument("--seed", type=int, default=-1, help="Fixed seed for reproducibility")
    parser.add_argument("--max-parallel", type=int, default=4, help="Max parallel processes (default: 4)")
    parser.add_argument("--output-dir", default="playtest_results", help="Output directory")
    args = parser.parse_args()

    exe = find_exe()
    print(f"Using: {exe}")

    # Build config matrix
    classes = [args.cls] if args.cls != "all" else CLASSES
    profiles = [args.profile] if args.profile != "all" else PROFILES

    if args.quick:
        classes = ["voidwalker"]
        args.runs = 3

    configs = [(c, p) for c in classes for p in profiles]
    print(f"Running {len(configs)} configurations x {args.runs} runs each")
    print(f"Max parallel: {args.max_parallel}")

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # Launch all playtests in parallel
    start_time = time.time()
    results = []

    with ProcessPoolExecutor(max_workers=args.max_parallel) as executor:
        futures = {}
        for cls, profile in configs:
            future = executor.submit(
                run_single_playtest, exe, cls, profile, args.runs, args.seed, args.output_dir
            )
            futures[future] = f"{cls}_{profile}"

        for future in as_completed(futures):
            config_name = futures[future]
            try:
                result = future.result()
                config_name, output_file, duration, returncode = result
                status = "OK" if returncode == 0 else ("TIMEOUT" if returncode == -1 else f"ERR:{returncode}")
                print(f"  [{status}] {config_name} - {duration:.0f}s")
                results.append(result)
            except Exception as e:
                print(f"  [FAIL] {config_name}: {e}")
                results.append((config_name, "", 0, -2))

    total_time = time.time() - start_time

    # Generate combined report
    report_path = generate_combined_report(results, args.output_dir)
    print(f"\nAll done in {total_time:.0f}s")
    print(f"Combined report: {report_path}")
    print(f"Individual logs: {args.output_dir}/playtest_*.log")

    # Print combined report to stdout
    with open(report_path, "r") as f:
        print(f.read())

if __name__ == "__main__":
    main()
