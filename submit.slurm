#!/bin/bash
#SBATCH --job-name=mpi_orch
#SBATCH --partition=rss-class
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=4
#SBATCH --time=00:10:00
#SBATCH --output=mpi_orch_%j.out
#SBATCH --error=mpi_orch_%j.err

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

if [[ -z "${SLURM_JOB_ID:-}" ]]; then
	exec sbatch "$0" "$@"
fi

# Get input file from argument or use default
INPUT_FILE="${1:-input.txt}"

echo "SLURM partition: ${SLURM_JOB_PARTITION:-unknown}"

# Call the main runner script, passing argument and inheriting SLURM env vars
./run_mpi_orch.sh --input "$INPUT_FILE"

exit $?
