#!/usr/bin/env python3
"""
Riftwalker Release Packager
============================
Creates a distributable release folder with all necessary files.

Usage:
    python tools/package_release.py              # Package from default build
    python tools/package_release.py --output dist # Custom output directory
"""

import os
import shutil
import argparse
from pathlib import Path

def package_release(build_dir="build/Release", output_dir="dist/Riftwalker"):
    """Create a release package."""
    build = Path(build_dir)
    out = Path(output_dir)

    if not build.exists():
        print(f"ERROR: Build directory not found: {build}")
        print("Run: cmake --build build --config Release")
        return False

    exe = build / "Riftwalker.exe"
    if not exe.exists():
        print(f"ERROR: Riftwalker.exe not found in {build}")
        return False

    # Clean output
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    print(f"Packaging Riftwalker release to {out}")

    # 1. Copy executable
    shutil.copy2(exe, out / "Riftwalker.exe")
    print(f"  [OK] Riftwalker.exe ({exe.stat().st_size // 1024} KB)")

    # 2. Copy SDL2 DLLs
    dlls = ["SDL2.dll", "SDL2_image.dll", "SDL2_mixer.dll", "SDL2_ttf.dll",
            "libpng16-16.dll", "zlib1.dll", "libjpeg-9.dll",
            "libfreetype-6.dll", "libharfbuzz-0.dll", "libbrotlidec.dll",
            "libbrotlicommon.dll", "libogg-0.dll", "libvorbis-0.dll",
            "libvorbisfile-3.dll", "libmpg123-0.dll", "libFLAC.dll",
            "libopusfile-0.dll", "libopus-0.dll"]
    dll_count = 0
    for dll in dlls:
        src = build / dll
        if src.exists():
            shutil.copy2(src, out / dll)
            dll_count += 1
    print(f"  [OK] {dll_count} DLLs")

    # 3. Copy assets
    assets_src = Path("assets")
    if assets_src.exists():
        shutil.copytree(assets_src, out / "assets",
                       ignore=shutil.ignore_patterns("*.psd", "*.ai", "*.xcf"))
        asset_count = sum(1 for _ in (out / "assets").rglob("*") if _.is_file())
        print(f"  [OK] {asset_count} asset files")
    else:
        print("  [WARN] No assets directory found")

    # 4. Verify critical files
    critical = [
        out / "Riftwalker.exe",
        out / "SDL2.dll",
        out / "assets/fonts/default.ttf",
    ]
    for f in critical:
        if not f.exists():
            print(f"  [WARN] Missing critical file: {f}")

    # Calculate total size
    total_size = sum(f.stat().st_size for f in out.rglob("*") if f.is_file())
    print(f"\nRelease package: {out}")
    print(f"Total size: {total_size // 1024 // 1024} MB")
    print(f"Ready to distribute!")

    return True

def main():
    parser = argparse.ArgumentParser(description="Package Riftwalker for release")
    parser.add_argument("--output", default="dist/Riftwalker", help="Output directory")
    parser.add_argument("--build", default="build/Release", help="Build directory")
    args = parser.parse_args()

    os.chdir(Path(__file__).parent.parent)
    package_release(args.build, args.output)

if __name__ == "__main__":
    main()
