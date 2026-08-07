#!/bin/bash
# Elitism-default campaign driver (experiments/2026-08-07-elitism/PLAN.md).
# One argument: the host's seed arm, "lo" or "hi". Phases run sequentially so
# the FRETISH arm's jobs=4 never co-schedules with the TLSF arm's jobs=1 --
# their wall_time_s rows would otherwise be contaminated by each other, and
# wall time is criterion 4 of the decision rule.
#
# Survives ssh/session loss under nohup.
set -u
cd "$HOME/projects/counter"

ARM="${1:?usage: campaign_driver.sh lo|hi}"
case "$ARM" in
  lo) FRET_SEEDS=$(seq 0 74);   TLSF_SEEDS=$(seq 0 19)  ;;
  hi) FRET_SEEDS=$(seq 75 149); TLSF_SEEDS=$(seq 20 39) ;;
  *)  echo "unknown arm: $ARM" >&2; exit 2 ;;
esac

LOG=campaign-elitism.log
echo "[driver] arm=$ARM host=$(hostname) head=$(git rev-parse --short HEAD) start $(date -u +%FT%TZ)" >> "$LOG"

echo "[driver] fret start $(date -u +%FT%TZ)" >> "$LOG"
python3 scripts/run_experiments.py --profile elitism-fret \
    --seeds $FRET_SEEDS \
    >> elitism-fret-run.log 2>&1
echo "[driver] fret done rc=$? $(date -u +%FT%TZ)" >> "$LOG"

echo "[driver] tlsf start $(date -u +%FT%TZ)" >> "$LOG"
python3 scripts/run_experiments.py --profile elitism-tlsf \
    --seeds $TLSF_SEEDS \
    >> elitism-tlsf-run.log 2>&1
echo "[driver] tlsf done rc=$? $(date -u +%FT%TZ)" >> "$LOG"

echo "[driver] CAMPAIGN COMPLETE $(date -u +%FT%TZ)" >> "$LOG"
