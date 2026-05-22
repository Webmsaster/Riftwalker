# Builds a clean Steam upload payload under steam/content/.
#
# Usage:   powershell -ExecutionPolicy Bypass -File tools\make_steam_build.ps1
#
# Produces steam/content/ containing ONLY the shippable game (exe + runtime
# DLLs + assets). Save files, test logs, screenshots and dev artifacts are
# excluded. After running, upload with steamcmd + steam/scripts/app_build.vdf.

$ErrorActionPreference = "Stop"
$root    = Split-Path -Parent $PSScriptRoot          # repo root
$relDir  = Join-Path $root "build\Release"
$content = Join-Path $root "steam\content"

Write-Host "==> Building Release..." -ForegroundColor Cyan
cmake --build (Join-Path $root "build") --config Release | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

$exe = Join-Path $relDir "Riftwalker.exe"
if (-not (Test-Path $exe)) { throw "Riftwalker.exe not found at $exe" }

Write-Host "==> Staging clean payload..." -ForegroundColor Cyan
if (Test-Path $content) { Remove-Item $content -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$content\assets" | Out-Null

# Executable + runtime DLLs
Copy-Item $exe $content -Force
Copy-Item (Join-Path $relDir "*.dll") $content -Force
# Game assets (textures / sounds / music / fonts)
Copy-Item (Join-Path $relDir "assets\*") "$content\assets" -Recurse -Force
# Docs (optional but nice to ship)
foreach ($doc in @("README.md", "CHANGELOG.md")) {
    $p = Join-Path $root $doc
    if (Test-Path $p) { Copy-Item $p $content -Force }
}

# Defensive scrub: remove anything that should never ship even if it leaked
# into build/Release (saves, backups, test logs, screenshots, dev PNGs).
$scrub = @(
    "*.dat", "*.dat.bak", "*.cfg", "*.cfg.bak", "*.log", "*.csv",
    "riftwalker_crash.txt", "screenshot_stderr.txt",
    "hud_*.png", "full_small.png", "bottom.png", "gameplay_top_crop.png", "*.pdb"
)
foreach ($pat in $scrub) {
    Get-ChildItem -Path $content -Filter $pat -File -ErrorAction SilentlyContinue | Remove-Item -Force
}
Get-ChildItem -Path $content -Filter "screenshots" -Directory -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force

$fileCount = (Get-ChildItem $content -Recurse -File | Measure-Object).Count
$sizeMb    = "{0:N1}" -f ((Get-ChildItem $content -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
Write-Host "==> Done. steam/content ready: $fileCount files, $sizeMb MB" -ForegroundColor Green
Write-Host ""
Write-Host "Next: upload with steamcmd (see docs/STEAM_RELEASE.md):" -ForegroundColor Yellow
Write-Host '  steamcmd +login <user> +run_app_build "' -NoNewline
Write-Host (Join-Path $root 'steam\scripts\app_build.vdf') -NoNewline
Write-Host '" +quit'
