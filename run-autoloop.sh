#!/usr/bin/env bash
set -u

PROMPT_FILE="./.claude/auto-improve.md"

while true; do
  echo "=============================="
  echo "Neue Claude-Code-Iteration: $(date)"
  echo "=============================="

  claude \
    --dangerously-skip-permissions \
    --append-system-prompt-file "$PROMPT_FILE" \
    "Analysiere den aktuellen Projektzustand und fuehre genau eine vollstaendige Verbesserungsiteration aus: Problem waehlen, implementieren, builden, testen, Fehler beheben, kurz dokumentieren."

  echo "Naechste Runde startet. Stoppen mit Ctrl+C."
  sleep 2
done
