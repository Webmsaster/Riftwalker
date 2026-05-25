# Updates the local install (%LOCALAPPDATA%\Programs\Riftwalker) with the
# freshly built Release binary so the desktop / Start Menu icon always
# launches the current version.
#
# Usage:   powershell -ExecutionPolicy Bypass -File tools\deploy_local.ps1
#          powershell -ExecutionPolicy Bypass -File tools\deploy_local.ps1 -Assets   # also refresh assets/DLLs
#
# Pass -Assets after changing textures/sounds-data/DLLs. For a pure code
# change (most rebuilds) the default exe-only copy is enough and fast.

param([switch]$Assets)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $root "build\Release"
$dst  = "$env:LOCALAPPDATA\Programs\Riftwalker"

$exe = Join-Path $src "Riftwalker.exe"
if (-not (Test-Path $exe)) { throw "No build found at $exe - build Release first." }
if (-not (Test-Path $dst)) { throw "No install found at $dst - run the install step first." }

# Don't copy onto a running exe.
Get-Process Riftwalker -ErrorAction SilentlyContinue | Stop-Process -Force

Copy-Item $exe $dst -Force
Write-Host "Updated Riftwalker.exe -> $dst" -ForegroundColor Green

if ($Assets) {
    Copy-Item (Join-Path $src "*.dll") $dst -Force
    # Copy assets from the REPO (authoritative source), not from build/Release.
    # build/Release/assets is a build-time snapshot that goes stale whenever
    # assets are regenerated outside a fresh build (e.g. tools/gen_music_v3.py
    # or --dump-sfx). Pulling straight from the repo avoids the stale-cache
    # gotcha where deploy silently ships old assets.
    $repoAssets = Join-Path $root "assets"
    if (Test-Path "$dst\assets") { Remove-Item "$dst\assets" -Recurse -Force }
    Copy-Item $repoAssets $dst -Recurse -Force
    Write-Host "Refreshed DLLs + assets (from repo)" -ForegroundColor Green
}

$ts = (Get-Item "$dst\Riftwalker.exe").LastWriteTime
Write-Host "Installed build timestamp: $ts"
