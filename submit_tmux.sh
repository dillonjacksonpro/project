#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PATH_FILE=${1:-path.txt}
SESSION_NAME=${2:-mpi_orch_submit}

if ! command -v tmux >/dev/null 2>&1; then
	echo "Error: tmux is required" >&2
	exit 1
fi

if ! command -v sbatch >/dev/null 2>&1; then
	echo "Error: sbatch is required" >&2
	exit 1
fi

if [[ ! -f "$SCRIPT_DIR/$PATH_FILE" ]]; then
	echo "Error: path file not found: $SCRIPT_DIR/$PATH_FILE" >&2
	exit 1
fi

INPUT_PATH=$(<"$SCRIPT_DIR/$PATH_FILE")
if [[ -z "$INPUT_PATH" ]]; then
	echo "Error: path file is empty: $SCRIPT_DIR/$PATH_FILE" >&2
	exit 1
fi

if tmux has-session -t "$SESSION_NAME" 2>/dev/null; then
	echo "Error: tmux session already exists: $SESSION_NAME" >&2
	exit 1
fi

submit_cmd=$(printf './submit.slurm %q' "$INPUT_PATH")
tail_cmd=$(cat <<'EOF'
log_file=""
while [[ -z "$log_file" ]]; do
	log_file=$(find output -type f -path '*/mpi_orch.log' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n1 | cut -d' ' -f2-)
	if [[ -z "$log_file" ]]; then
		printf 'Waiting for job log file...\n'
		sleep 1
	fi
done
printf 'Tailing %s\n' "$log_file"
tail -n +1 -F "$log_file"
EOF
)

tmux new-session -d -s "$SESSION_NAME" -n submit -c "$SCRIPT_DIR" bash -lc "$submit_cmd"
tmux new-window -t "$SESSION_NAME" -n log -c "$SCRIPT_DIR" bash -lc "$tail_cmd"
tmux select-window -t "$SESSION_NAME":submit
tmux attach -t "$SESSION_NAME"