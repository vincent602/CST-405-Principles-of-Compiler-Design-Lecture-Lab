#!/usr/bin/env bash
set -euo pipefail

INPUT_FILE="${1:-project6.cm}"
OUTPUT_FILE="${2:-project6.s}"
REPORT_FILE="PROJECT6_PERFORMANCE_METRICS.md"
COMPILE_LOG="/tmp/project6_compile.log"
EXEC_LOG="/tmp/project6_exec.log"

if [[ ! -f "$INPUT_FILE" ]]; then
  echo "Error: input file '$INPUT_FILE' not found."
  echo "Usage: ./project6_benchmark.sh [input.cm] [output.s]"
  exit 1
fi

if [[ ! -x "./minicompiler" ]]; then
  echo "minicompiler not found. Building with make..."
  make
fi

measure_command() {
  local log_file="$1"
  shift
  python3 - "$log_file" "$@" <<'PY'
import subprocess
import sys
import time

if len(sys.argv) < 3:
    print("measure_command requires logfile and command", file=sys.stderr)
    sys.exit(2)

log_file = sys.argv[1]
cmd = sys.argv[2:]

start = time.perf_counter()
proc = subprocess.run(cmd, capture_output=True, text=True)
elapsed = time.perf_counter() - start

with open(log_file, "w", encoding="utf-8") as f:
    f.write(proc.stdout)
    f.write(proc.stderr)

print(f"{elapsed:.6f}")
sys.exit(proc.returncode)
PY
}

echo "Measuring compilation time..."
compile_time="$(measure_command "$COMPILE_LOG" ./minicompiler "$INPUT_FILE" "$OUTPUT_FILE")"

exec_time="N/A"
exec_note="SPIM not installed; execution timing skipped."

if command -v spim >/dev/null 2>&1; then
  echo "Measuring generated code execution time..."
  exec_time="$(measure_command "$EXEC_LOG" spim -file "$OUTPUT_FILE")"
  exec_note="Execution completed with SPIM."
else
  : >"$EXEC_LOG"
fi

timestamp="$(date '+%Y-%m-%d %H:%M:%S %Z')"

{
  cat <<EOF
# Project 6 Performance Metrics

Generated: $timestamp

## Input and Commands
- Input source: \`$INPUT_FILE\`
- Generated assembly: \`$OUTPUT_FILE\`
- Compilation command: \`./minicompiler $INPUT_FILE $OUTPUT_FILE\`
- Execution command: \`spim -file $OUTPUT_FILE\`

## Metrics
- Compilation time (seconds): **$compile_time**
- Execution time (seconds): **$exec_time**

## Notes
- $exec_note

## Compilation Phase Detection
EOF

  for phase in 1 2 3 4 5 6; do
    if grep -q "PHASE $phase:" "$COMPILE_LOG"; then
      echo "- Phase $phase: found"
    else
      echo "- Phase $phase: NOT found"
    fi
  done

  cat <<EOF

## Full Compilation Output
\`\`\`
$(cat "$COMPILE_LOG")
\`\`\`

## Compilation Output by Phase
EOF

  for phase in 1 2 3 4 5 6; do
    next_phase=$((phase + 1))
    start_pat="PHASE $phase:"
    next_pat=""
    if [[ $phase -lt 6 ]]; then
      next_pat="PHASE $next_phase:"
    fi

    if grep -q "$start_pat" "$COMPILE_LOG"; then
      if [[ -n "$next_pat" ]]; then
        phase_text="$(awk -v s="$start_pat" -v e="$next_pat" '
          $0 ~ s {in_block=1}
          in_block && $0 ~ e {exit}
          in_block {print}
        ' "$COMPILE_LOG")"
      else
        phase_text="$(awk -v s="$start_pat" '
          $0 ~ s {in_block=1}
          in_block {print}
        ' "$COMPILE_LOG")"
      fi

      cat <<EOF

### Phase $phase
\`\`\`
$phase_text
\`\`\`
EOF
    fi
  done

  cat <<EOF

## Execution Output
\`\`\`
$(cat "$EXEC_LOG")
\`\`\`
EOF
} > "$REPORT_FILE"

echo "Benchmark complete."
echo "Compilation time: $compile_time s"
echo "Execution time:   $exec_time s"
echo "Report written to $REPORT_FILE"
