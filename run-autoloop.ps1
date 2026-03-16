$PromptFile = ".\.claude\auto-improve.md"

while ($true) {
    Write-Host "=============================="
    Write-Host "Neue Claude-Code-Iteration: $(Get-Date)"
    Write-Host "=============================="

    claude --dangerously-skip-permissions --append-system-prompt-file $PromptFile -p "Analysiere den aktuellen Projektzustand und fuehre genau eine vollstaendige Verbesserungsiteration aus: Problem waehlen, implementieren, builden, testen, Fehler beheben, kurz dokumentieren."

    Write-Host "Naechste Runde startet. Stoppen mit Strg+C."
    Start-Sleep -Seconds 2
}
