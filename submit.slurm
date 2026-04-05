#!/bin/bash
#SBATCH --job-name=mpi_orch
#SBATCH --partition=rss-class
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=4
#SBATCH --time=00:10:00
#SBATCH --output=output/run_%j/mpi_orch.out
#SBATCH --error=output/run_%j/mpi_orch.err

# ============================================================================
# submit.slurm - SLURM wrapper for run_mpi_orch.sh
# ============================================================================
# Allocates SLURM resources and passes them to the main bash script
#
# Usage:
#   sbatch submit.slurm [INPUT_FILE]
#
# The SLURM environment variables (SLURM_NNODES, SLURM_CPUS_PER_TASK, etc.)
# are automatically passed to run_mpi_orch.sh
#
# ============================================================================

set -e

# Ensure output directory exists for this job
mkdir -p "output/run_${SLURM_JOB_ID}"
OUTPUT_DIR="output/run_${SLURM_JOB_ID}"

EXPECTED_JOB_NAME="mpi_orch"

if [[ -z "${SLURM_JOB_ID:-}" || "${SLURM_JOB_NAME:-}" != "$EXPECTED_JOB_NAME" ]]; then
	echo "submit.slurm: submitting fresh batch job allocation"
	exec sbatch "$0" "$@"
fi

# Get input file from argument or use default
INPUT_FILE="${1:-input.txt}"

echo "SLURM partition: ${SLURM_JOB_PARTITION:-unknown}"
echo "Output directory: $OUTPUT_DIR"

# Call the main runner script, passing argument and inheriting SLURM env vars
./run_mpi_orch.sh --input "$INPUT_FILE" --output "$OUTPUT_DIR"

exit $?
