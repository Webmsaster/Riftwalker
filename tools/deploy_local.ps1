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
    if (Test-Path "$dst\assets") { Remove-Item "$dst\assets" -Recurse -Force }
    Copy-Item (Join-Path $src "assets") $dst -Recurse -Force
    Write-Host "Refreshed DLLs + assets" -ForegroundColor Green
}

$ts = (Get-Item "$dst\Riftwalker.exe").LastWriteTime
Write-Host "Installed build timestamp: $ts"
