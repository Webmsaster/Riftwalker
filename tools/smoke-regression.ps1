param(
    [ValidateSet("short", "long")]
    [string]$Mode = "short",
    [string]$ExePath = ".\build\Release\Riftwalker.exe",
    [int[]]$Seeds = @(),
    [ValidateRange(1, 10)]
    [int]$RepeatCount = 1
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exeFullPath = if ([System.IO.Path]::IsPathRooted($ExePath)) {
    $ExePath
} else {
    Join-Path $repoRoot $ExePath
}
if (-not (Test-Path $exeFullPath)) {
    throw "Executable not found: $exeFullPath"
}

$seedProfiles = @{
    short = @{
        1    = @{ MinCompletedFloor = 2; MaxNoProgress = 8; MaxAutoRescuePath = 0 }
        42   = @{ MinCompletedFloor = 2; MaxNoProgress = 8; MaxAutoRescuePath = 0 }
        1337 = @{ MinCompletedFloor = 2; MaxNoProgress = 8; MaxAutoRescuePath = 0 }
        9001 = @{ MinCompletedFloor = 2; MaxNoProgress = 8; MaxAutoRescuePath = 1 }
    }
    long = @{
        1    = @{ MinCompletedFloor = 5; MaxNoProgress = 21; MaxAutoRescuePath = 0; MaxFocusFloorNoProgress = 5; MaxFocusFloorAutoRescuePath = 0 }
        42   = @{ MinCompletedFloor = 5; MaxNoProgress = 23; MaxAutoRescuePath = 0; MaxFocusFloorNoProgress = 7; MaxFocusFloorAutoRescuePath = 0 }
        1337 = @{ MinCompletedFloor = 5; MaxNoProgress = 18; MaxAutoRescuePath = 2; MaxFocusFloorNoProgress = 5; MaxFocusFloorAutoRescuePath = 1 }
        9001 = @{ MinCompletedFloor = 5; MaxNoProgress = 20; MaxAutoRescuePath = 1; MaxFocusFloorNoProgress = 6; MaxFocusFloorAutoRescuePath = 1 }
    }
}

if ($Seeds.Count -eq 0) {
    $Seeds = @(1, 42, 1337, 9001)
}

$config = if ($Mode -eq "short") {
    @{
        TargetFloor = 2
        MaxRuntime = 90
        WaitTimeout = 120
        SeedBounds = $seedProfiles.short
    }
} else {
    @{
        TargetFloor = 5
        MaxRuntime = 330
        WaitTimeout = 420
        SeedBounds = $seedProfiles.long
    }
}

function Get-MatchCount {
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (-not (Test-Path $Path)) {
        return 0
    }

    $matches = Select-String -Path $Path -Pattern $Pattern
    if ($null -eq $matches) {
        return 0
    }

    return @($matches).Count
}

function Get-RunMetrics {
    param(
        [string]$Path,
        [int]$FocusFloor = 0
    )

    $resultLine = ""
    $completedFloor = 0
    $startedFloor = 0
    $runtimeSeconds = 0.0
    $resultStatus = ""
    $failureCode = 0

    if (Test-Path $Path) {
        $resultLine = (Select-String -Path $Path -Pattern "\[SMOKE\] RESULT" | Select-Object -Last 1).Line

        $levelStarts = Select-String -Path $Path -Pattern "\[SMOKE\] === LEVEL (\d+) START ==="
        if ($levelStarts) {
            $startedFloor = [int]$levelStarts[-1].Matches[0].Groups[1].Value
        }

        $levelCompletes = Select-String -Path $Path -Pattern "\[SMOKE\] LEVEL (\d+) COMPLETE!"
        if ($levelCompletes) {
            $completedFloor = [int]$levelCompletes[-1].Matches[0].Groups[1].Value
        }

        if ($resultLine -match "status=(\w+)") {
            $resultStatus = $Matches[1]
        }
        if ($resultLine -match "runtime=([0-9.]+)") {
            $runtimeSeconds = [double]$Matches[1]
        }
        if ($resultLine -match "failureCode=(-?\d+)") {
            $failureCode = [int]$Matches[1]
        } elseif ($resultLine -match "code=(-?\d+)") {
            $failureCode = [int]$Matches[1]
        }
    }

    [pscustomobject]@{
        ResultLine            = $resultLine
        ResultStatus          = $resultStatus
        FailureCode           = $failureCode
        RuntimeSeconds        = $runtimeSeconds
        CompletedFloor        = $completedFloor
        StartedFloor          = $startedFloor
        NoProgressEvents      = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_(TARGET|TARGET_ROOM|PATH|EXIT|SPAWN)\b"
        NoProgressTargetCount = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_TARGET\b"
        NoProgressTargetRoomCount = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_TARGET_ROOM\b"
        NoProgressPathCount   = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_PATH\b"
        NoProgressExitCount   = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_EXIT\b"
        NoProgressAnchors     = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] NO_PROGRESS_ANCHOR\b"
        AutoRescuePathCount   = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] AUTO_RESCUE_PATH\b"
        AutoRescueSpawnCount  = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] AUTO_RESCUE_SPAWN\b"
        AutoRescueAnchorCount = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] AUTO_RESCUE_ANCHOR\b"
        SpawnFallbackCount    = Get-MatchCount -Path $Path -Pattern "spawnFallback=1"
        LevelTimeoutCount     = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] LEVEL TIMEOUT:"
        TotalTimeoutCount     = Get-MatchCount -Path $Path -Pattern "\[SMOKE\] TOTAL TIMEOUT"
        FocusFloorNoProgressEvents = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] NO_PROGRESS_(TARGET|TARGET_ROOM|PATH|EXIT|SPAWN)\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
        FocusFloorNoProgressPathCount = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] NO_PROGRESS_PATH\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
        FocusFloorNoProgressTargetCount = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] NO_PROGRESS_TARGET\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
        FocusFloorNoProgressTargetRoomCount = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] NO_PROGRESS_TARGET_ROOM\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
        FocusFloorNoProgressExitCount = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] NO_PROGRESS_EXIT\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
        FocusFloorAutoRescuePathCount = if ($FocusFloor -gt 0) { Get-MatchCount -Path $Path -Pattern ("\[SMOKE\] AUTO_RESCUE_PATH\b.*floor={0}\b" -f $FocusFloor) } else { 0 }
    }
}

$outDir = Join-Path $repoRoot ("build\smoke-regression\" + $Mode)
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$failures = @()
$summary = New-Object System.Collections.Generic.List[string]
$summaryRows = New-Object System.Collections.Generic.List[object]

for ($runIndex = 1; $runIndex -le $RepeatCount; $runIndex++) {
foreach ($seed in $Seeds) {
    $attempt = 0
    $maxAttempts = 2
    $ok = $false
    $reason = "PASS"
    $metrics = $null
    $seedLogName = if ($RepeatCount -gt 1) {
        "smoke_seed{0}_run{1:D2}.log" -f $seed, $runIndex
    } else {
        "smoke_seed{0}.log" -f $seed
    }
    $seedLog = Join-Path $outDir $seedLogName
    $seedBounds = if ($config.SeedBounds.ContainsKey($seed)) {
        $config.SeedBounds[$seed]
    } else {
        @{
            MinCompletedFloor = $config.TargetFloor
            MaxNoProgress = [int]::MaxValue
            MaxAutoRescuePath = [int]::MaxValue
            MaxFocusFloorNoProgress = [int]::MaxValue
            MaxFocusFloorAutoRescuePath = [int]::MaxValue
        }
    }
    if (-not $seedBounds.ContainsKey("MaxFocusFloorNoProgress")) {
        $seedBounds.MaxFocusFloorNoProgress = [int]::MaxValue
    }
    if (-not $seedBounds.ContainsKey("MaxFocusFloorAutoRescuePath")) {
        $seedBounds.MaxFocusFloorAutoRescuePath = [int]::MaxValue
    }

    while ($attempt -lt $maxAttempts -and -not $ok) {
        $attempt++
        $logPath = Join-Path $repoRoot "smoke_test.log"
        if (Test-Path $logPath) {
            Remove-Item $logPath -Force
        }

        $args = @(
            "--smoke-regression",
            "--seed=$seed",
            "--smoke-target-floor=$($config.TargetFloor)",
            "--smoke-max-runtime=$($config.MaxRuntime)"
        )

        $proc = Start-Process -FilePath $exeFullPath -ArgumentList $args -WorkingDirectory $repoRoot -PassThru
        $timedOut = $false
        try {
            Wait-Process -Id $proc.Id -Timeout $config.WaitTimeout -ErrorAction Stop
        } catch {
            $timedOut = $true
        }

        if ($timedOut -and -not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
        }

        if (Test-Path $logPath) {
            Copy-Item $logPath $seedLog -Force
        }

        $metrics = Get-RunMetrics -Path $seedLog -FocusFloor $config.TargetFloor

        $bannedMatches = @()
        if (Test-Path $seedLog) {
            $bannedMatches = Select-String -Path $seedLog -Pattern "spawnFallback=1|NO_PROGRESS_SPAWN|AUTO_RESCUE_SPAWN|genValidated=0"
        }

        $ok = $true
        $reason = "PASS"

        if ($timedOut) {
            $ok = $false
            $reason = "process_timeout"
        } elseif ($proc.ExitCode -ne 0) {
            $ok = $false
            $reason = "exit_code_$($proc.ExitCode)"
        } elseif ([string]::IsNullOrWhiteSpace($metrics.ResultLine) -or $metrics.ResultStatus -ne "PASS") {
            $ok = $false
            $reason = "missing_pass_result"
        } elseif ($bannedMatches.Count -gt 0) {
            $ok = $false
            $reason = "banned_signal"
        } elseif ($metrics.CompletedFloor -lt $seedBounds.MinCompletedFloor) {
            $ok = $false
            $reason = "progress_floor_$($metrics.CompletedFloor)_lt_$($seedBounds.MinCompletedFloor)"
        } elseif ($metrics.NoProgressEvents -gt $seedBounds.MaxNoProgress) {
            $ok = $false
            $reason = "no_progress_$($metrics.NoProgressEvents)_gt_$($seedBounds.MaxNoProgress)"
        } elseif ($metrics.AutoRescuePathCount -gt $seedBounds.MaxAutoRescuePath) {
            $ok = $false
            $reason = "auto_rescue_path_$($metrics.AutoRescuePathCount)_gt_$($seedBounds.MaxAutoRescuePath)"
        } elseif ($metrics.FocusFloorNoProgressEvents -gt $seedBounds.MaxFocusFloorNoProgress) {
            $ok = $false
            $reason = "focus_floor_no_progress_$($metrics.FocusFloorNoProgressEvents)_gt_$($seedBounds.MaxFocusFloorNoProgress)"
        } elseif ($metrics.FocusFloorAutoRescuePathCount -gt $seedBounds.MaxFocusFloorAutoRescuePath) {
            $ok = $false
            $reason = "focus_floor_auto_rescue_path_$($metrics.FocusFloorAutoRescuePathCount)_gt_$($seedBounds.MaxFocusFloorAutoRescuePath)"
        }

        if (-not $ok) {
            if ($reason -eq "exit_code_-1073741510" -and $attempt -lt $maxAttempts) {
                Start-Sleep -Seconds 1
                continue
            }
            break
        }
    }

    if ($ok) {
        $summary.Add((
            "PASS seed={0} run={1} mode={2} attempts={3} floors={4} started={5} runtime={6:N1}s noProgress={7} focusFloorNoProgress={8} autoRescuePath={9} focusFloorAutoRescuePath={10} bounds[minFloor={11},maxNoProgress={12},maxAutoRescuePath={13},maxFocusFloorNoProgress={14},maxFocusFloorAutoRescuePath={15}] result={16}" -f
            $seed, $runIndex, $Mode, $attempt, $metrics.CompletedFloor, $metrics.StartedFloor, $metrics.RuntimeSeconds,
            $metrics.NoProgressEvents, $metrics.FocusFloorNoProgressEvents, $metrics.AutoRescuePathCount, $metrics.FocusFloorAutoRescuePathCount,
            $seedBounds.MinCompletedFloor, $seedBounds.MaxNoProgress, $seedBounds.MaxAutoRescuePath,
            $seedBounds.MaxFocusFloorNoProgress, $seedBounds.MaxFocusFloorAutoRescuePath,
            $metrics.ResultLine
        ))
    } else {
        $summary.Add((
            "FAIL seed={0} run={1} mode={2} attempts={3} reason={4} floors={5} started={6} runtime={7:N1}s noProgress={8} focusFloorNoProgress={9} autoRescuePath={10} focusFloorAutoRescuePath={11} bounds[minFloor={12},maxNoProgress={13},maxAutoRescuePath={14},maxFocusFloorNoProgress={15},maxFocusFloorAutoRescuePath={16}] result={17}" -f
            $seed, $runIndex, $Mode, $attempt, $reason,
            $metrics.CompletedFloor, $metrics.StartedFloor, $metrics.RuntimeSeconds,
            $metrics.NoProgressEvents, $metrics.FocusFloorNoProgressEvents, $metrics.AutoRescuePathCount, $metrics.FocusFloorAutoRescuePathCount,
            $seedBounds.MinCompletedFloor, $seedBounds.MaxNoProgress, $seedBounds.MaxAutoRescuePath,
            $seedBounds.MaxFocusFloorNoProgress, $seedBounds.MaxFocusFloorAutoRescuePath,
            $metrics.ResultLine
        ))
        $failures += $seed
    }

    $summaryRows.Add([pscustomobject]@{
        run_index           = $runIndex
        seed                = $seed
        mode                = $Mode
        status              = if ($ok) { "PASS" } else { "FAIL" }
        attempts            = $attempt
        reason              = $reason
        log_file            = $seedLogName
        completed_floor     = if ($metrics) { $metrics.CompletedFloor } else { 0 }
        started_floor       = if ($metrics) { $metrics.StartedFloor } else { 0 }
        runtime_seconds     = if ($metrics) { $metrics.RuntimeSeconds } else { 0.0 }
        no_progress_events  = if ($metrics) { $metrics.NoProgressEvents } else { 0 }
        no_progress_target  = if ($metrics) { $metrics.NoProgressTargetCount } else { 0 }
        no_progress_target_room = if ($metrics) { $metrics.NoProgressTargetRoomCount } else { 0 }
        no_progress_path    = if ($metrics) { $metrics.NoProgressPathCount } else { 0 }
        no_progress_exit    = if ($metrics) { $metrics.NoProgressExitCount } else { 0 }
        no_progress_anchors = if ($metrics) { $metrics.NoProgressAnchors } else { 0 }
        auto_rescue_path    = if ($metrics) { $metrics.AutoRescuePathCount } else { 0 }
        auto_rescue_spawn   = if ($metrics) { $metrics.AutoRescueSpawnCount } else { 0 }
        auto_rescue_anchor  = if ($metrics) { $metrics.AutoRescueAnchorCount } else { 0 }
        spawn_fallback      = if ($metrics) { $metrics.SpawnFallbackCount } else { 0 }
        level_timeout_count = if ($metrics) { $metrics.LevelTimeoutCount } else { 0 }
        total_timeout_count = if ($metrics) { $metrics.TotalTimeoutCount } else { 0 }
        focus_floor         = $config.TargetFloor
        focus_floor_no_progress = if ($metrics) { $metrics.FocusFloorNoProgressEvents } else { 0 }
        focus_floor_no_progress_target = if ($metrics) { $metrics.FocusFloorNoProgressTargetCount } else { 0 }
        focus_floor_no_progress_target_room = if ($metrics) { $metrics.FocusFloorNoProgressTargetRoomCount } else { 0 }
        focus_floor_no_progress_path = if ($metrics) { $metrics.FocusFloorNoProgressPathCount } else { 0 }
        focus_floor_no_progress_exit = if ($metrics) { $metrics.FocusFloorNoProgressExitCount } else { 0 }
        focus_floor_auto_rescue_path = if ($metrics) { $metrics.FocusFloorAutoRescuePathCount } else { 0 }
        failure_code        = if ($metrics) { $metrics.FailureCode } else { 0 }
        min_completed_floor = $seedBounds.MinCompletedFloor
        max_no_progress     = $seedBounds.MaxNoProgress
        max_auto_rescue     = $seedBounds.MaxAutoRescuePath
        max_focus_floor_no_progress = $seedBounds.MaxFocusFloorNoProgress
        max_focus_floor_auto_rescue = $seedBounds.MaxFocusFloorAutoRescuePath
        result              = if ($metrics) { $metrics.ResultLine } else { "" }
    })
}
}

$summaryTextName = if ($RepeatCount -gt 1) { "summary_repeat{0}.txt" -f $RepeatCount } else { "summary.txt" }
$summaryCsvName = if ($RepeatCount -gt 1) { "summary_repeat{0}.csv" -f $RepeatCount } else { "summary.csv" }
$baselineCsvName = if ($RepeatCount -gt 1) { "baseline_repeat{0}.csv" -f $RepeatCount } else { "baseline.csv" }
$baselineTextName = if ($RepeatCount -gt 1) { "baseline_repeat{0}.txt" -f $RepeatCount } else { "baseline.txt" }

$summaryPath = Join-Path $outDir $summaryTextName
$summary | Set-Content -Path $summaryPath
$summaryRows | Export-Csv -NoTypeInformation -Path (Join-Path $outDir $summaryCsvName)

$baselineRows = foreach ($seedGroup in ($summaryRows | Group-Object seed | Sort-Object Name)) {
    $rows = @($seedGroup.Group)
    [pscustomobject]@{
        seed                           = [int]$seedGroup.Name
        runs                           = $rows.Count
        min_completed_floor            = ($rows | Measure-Object -Property completed_floor -Minimum).Minimum
        max_started_floor              = ($rows | Measure-Object -Property started_floor -Maximum).Maximum
        max_runtime_seconds            = ($rows | Measure-Object -Property runtime_seconds -Maximum).Maximum
        max_no_progress_events         = ($rows | Measure-Object -Property no_progress_events -Maximum).Maximum
        max_no_progress_target         = ($rows | Measure-Object -Property no_progress_target -Maximum).Maximum
        max_no_progress_target_room    = ($rows | Measure-Object -Property no_progress_target_room -Maximum).Maximum
        max_no_progress_path           = ($rows | Measure-Object -Property no_progress_path -Maximum).Maximum
        max_no_progress_exit           = ($rows | Measure-Object -Property no_progress_exit -Maximum).Maximum
        max_auto_rescue_path           = ($rows | Measure-Object -Property auto_rescue_path -Maximum).Maximum
        max_focus_floor_no_progress    = ($rows | Measure-Object -Property focus_floor_no_progress -Maximum).Maximum
        max_focus_floor_target         = ($rows | Measure-Object -Property focus_floor_no_progress_target -Maximum).Maximum
        max_focus_floor_target_room    = ($rows | Measure-Object -Property focus_floor_no_progress_target_room -Maximum).Maximum
        max_focus_floor_path           = ($rows | Measure-Object -Property focus_floor_no_progress_path -Maximum).Maximum
        max_focus_floor_exit           = ($rows | Measure-Object -Property focus_floor_no_progress_exit -Maximum).Maximum
        max_focus_floor_auto_rescue    = ($rows | Measure-Object -Property focus_floor_auto_rescue_path -Maximum).Maximum
        max_level_timeout_count        = ($rows | Measure-Object -Property level_timeout_count -Maximum).Maximum
        max_total_timeout_count        = ($rows | Measure-Object -Property total_timeout_count -Maximum).Maximum
        all_passed                     = if (($rows | Where-Object { $_.status -ne 'PASS' }).Count -eq 0) { 1 } else { 0 }
    }
}

$baselinePath = Join-Path $outDir $baselineCsvName
$baselineRows | Export-Csv -NoTypeInformation -Path $baselinePath
$baselineLines = foreach ($row in $baselineRows) {
    "seed={0} runs={1} minFloor={2} maxRuntime={3:N1}s maxNoProgress={4} maxPath={5} maxTarget={6} maxTargetRoom={7} maxExit={8} maxAutoRescuePath={9} maxFocusFloorNoProgress={10} maxFocusFloorAutoRescuePath={11} passed={12}" -f `
        $row.seed, $row.runs, $row.min_completed_floor, $row.max_runtime_seconds, $row.max_no_progress_events, `
        $row.max_no_progress_path, $row.max_no_progress_target, $row.max_no_progress_target_room, $row.max_no_progress_exit, `
        $row.max_auto_rescue_path, $row.max_focus_floor_no_progress, $row.max_focus_floor_auto_rescue, $row.all_passed
}
$baselineLines | Set-Content -Path (Join-Path $outDir $baselineTextName)
$summary | ForEach-Object { Write-Output $_ }

if ($failures.Count -gt 0) {
    exit 1
}

exit 0
