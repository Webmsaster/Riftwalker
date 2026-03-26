# visual-regression.ps1 — Multi-point visual regression tests
#
# Usage:
#   visual-regression.ps1 -ExePath <game.exe> -DiffPath <visual_diff.exe> -RefDir <ref_dir>
#   visual-regression.ps1 ... -Update    # Capture new baseline references
#   visual-regression.ps1 ... -Threshold 0.2  # Custom diff threshold
#
# Uses --visual-test to capture 5 screenshots across game states,
# then compares each against reference baselines.

param(
    [Parameter(Mandatory=$true)] [string]$ExePath,
    [Parameter(Mandatory=$true)] [string]$DiffPath,
    [Parameter(Mandatory=$true)] [string]$RefDir,
    [switch]$Update,
    [float]$Threshold = 0.1
)

$ErrorActionPreference = "Stop"
$gameDir = Split-Path $ExePath -Parent
$testDir = Join-Path (Join-Path $gameDir "screenshots") "visual_test"

# Clean old test screenshots
if (Test-Path $testDir) {
    Remove-Item "$testDir\*.png" -ErrorAction SilentlyContinue
}

# Run the visual test mode
Write-Host "Running visual test (captures 5 game states)..."
$proc = Start-Process -FilePath $ExePath -ArgumentList "--visual-test" -WorkingDirectory $gameDir -PassThru -NoNewWindow
$proc | Wait-Process -Timeout 60 -ErrorAction SilentlyContinue
if (!$proc.HasExited) { $proc.Kill(); Write-Host "WARNING: Game did not exit in time" }

# Find captured screenshots
$screenshots = Get-ChildItem "$testDir\*.png" -ErrorAction SilentlyContinue | Sort-Object Name
if ($screenshots.Count -eq 0) {
    Write-Error "No screenshots captured! Visual test mode may have failed."
    exit 2
}
Write-Host "Captured $($screenshots.Count) screenshots"

# Update mode: copy all screenshots as new references
if ($Update) {
    if (!(Test-Path $RefDir)) { New-Item -ItemType Directory -Path $RefDir -Force | Out-Null }
    foreach ($s in $screenshots) {
        $refFile = Join-Path $RefDir $s.Name
        Copy-Item $s.FullName $refFile -Force
        Write-Host "  Reference saved: $($s.Name)"
    }
    Write-Host "Baseline updated with $($screenshots.Count) references."
    exit 0
}

# Compare mode: diff each screenshot against reference
$passed = 0
$failed = 0
$skipped = 0
$diffDir = Join-Path $testDir "diffs"
if (!(Test-Path $diffDir)) { New-Item -ItemType Directory -Path $diffDir -Force | Out-Null }

foreach ($s in $screenshots) {
    $refFile = Join-Path $RefDir $s.Name
    $diffFile = Join-Path $diffDir "diff_$($s.Name)"

    if (!(Test-Path $refFile)) {
        Write-Host "  SKIP: $($s.Name) (no reference baseline)"
        $skipped++
        continue
    }

    # Dynamic screens (gameplay, debug, pause) have timers/particles that vary per run
    # Allow up to 5% diff for those, 0.1% for static screens (menu, class select)
    $maxDiff = "0.1"
    if ($s.Name -match "gameplay|debug_overlay|pause") { $maxDiff = "5.0" }

    & $DiffPath $refFile $s.FullName $diffFile --threshold $Threshold --max-diff $maxDiff
    $code = $LASTEXITCODE

    if ($code -eq 0) {
        Write-Host "  PASS: $($s.Name)"
        $passed++
    } elseif ($code -eq 1) {
        Write-Host "  FAIL: $($s.Name) — see $diffFile"
        $failed++
    } else {
        Write-Host "  ERROR: $($s.Name) — visual_diff returned $code"
        $failed++
    }
}

# Summary
Write-Host ""
Write-Host "=== Visual Regression Summary ==="
Write-Host "  Passed:  $passed"
Write-Host "  Failed:  $failed"
Write-Host "  Skipped: $skipped"
Write-Host "  Total:   $($screenshots.Count)"

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "FAIL: $failed visual regression(s) detected!"
    Write-Host "Run with -Update to accept changes as new baseline."
    exit 1
} elseif ($skipped -eq $screenshots.Count) {
    Write-Host ""
    Write-Host "No baselines found. Run with -Update to create initial references."
    exit 0
} else {
    Write-Host ""
    Write-Host "PASS: All visual tests passed."
    exit 0
}
