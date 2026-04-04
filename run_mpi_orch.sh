#!/bin/bash

# ============================================================================
# run_mpi_orch.sh - Main entry point for MPI orchestration
# ============================================================================
# Can be run directly or called from SLURM script
#
# Usage:
#   ./run_mpi_orch.sh [OPTIONS]
#
# Options:
#   -i, --input DIR          Input directory containing job CSV files (default: input)
#   -n, --nodes NUM          Number of nodes (default: from SLURM or 1)
#   -t, --threads NUM        Max threads per node (default: from SLURM or 16)
#   -p, --partition NAME     SLURM partition to target (default: current/default)
#   -h, --help              Show this help message
#
# ============================================================================

set -e

# ============================================================================
# Configuration - Defaults (can be overridden by args or SLURM env)
# ============================================================================

INPUT_FILE="input"
NUM_NODES=${SLURM_NNODES:-1}
MAX_THREADS_PER_NODE=${SLURM_CPUS_PER_TASK:-16}
SLURM_NODELIST=${SLURM_NODELIST:-"localhost"}
PARTITION=${SLURM_JOB_PARTITION:-""}

# ============================================================================
# Parse command-line arguments
# ============================================================================

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--input)
            INPUT_FILE="$2"
            shift 2
            ;;
        -n|--nodes)
            NUM_NODES="$2"
            shift 2
            ;;
        -t|--threads)
            MAX_THREADS_PER_NODE="$2"
            shift 2
            ;;
        -p|--partition)
            PARTITION="$2"
            shift 2
            ;;
        -h|--help)
            cat << 'EOF'
MPI Orchestration Runner

Usage: run_mpi_orch.sh [OPTIONS]

Options:
    -i, --input DIR        Input directory containing job CSV files (default: input)
    -n, --nodes NUM        Number of nodes (default: from SLURM or 1)
    -t, --threads NUM      Max threads per node (default: from SLURM or 16)
    -p, --partition NAME   SLURM partition to target (default: current/default)
    -h, --help             Show this help message

Examples:
    # Use defaults
    ./run_mpi_orch.sh

    # Custom input directory
    ./run_mpi_orch.sh -i /path/to/jobs_dir

    # Simulate 4-node cluster with 8 threads each
    ./run_mpi_orch.sh -i /path/to/jobs_dir -n 4 -t 8

    # Force a specific partition
    ./run_mpi_orch.sh -i /path/to/jobs_dir -p compute

    # Via SLURM (automatic):
    sbatch -p compute submit.slurm /path/to/jobs_dir

EOF
            exit 0
            ;;
        *)
            echo "Error: Unknown option: $1" >&2
            echo "Use -h or --help for usage information" >&2
            exit 1
            ;;
    esac
done

# ============================================================================
# Function: Build node-to-cores mapping
# ============================================================================

build_node_map() {
    local nodes=$1
    local threads=$2
    local node_list=$3
    
    # Convert SLURM node list to array
    local node_array=()
    
    if [[ "$node_list" =~ \[.*\] ]]; then
        # Handle range format: node[001-004]
        local prefix=${node_list%\[*}
        local range_part=${node_list#*\[}
        range_part=${range_part%\]}
        
        if [[ "$range_part" =~ ^([0-9]+)-([0-9]+)$ ]]; then
            local start=${BASH_REMATCH[1]}
            local end=${BASH_REMATCH[2]}
            for ((i=start; i<=end; i++)); do
                node_array+=("${prefix}$(printf "%03d" $i)")
            done
        fi
    else
        # Handle comma-separated format or single node
        if [[ "$node_list" == *","* ]]; then
            IFS=',' read -ra node_array <<< "$node_list"
        else
            # Single node
            node_array=("$node_list")
        fi
    fi
    
    # If node array is empty, use default
    if [[ ${#node_array[@]} -eq 0 ]]; then
        node_array=("localhost")
    fi
    
    # Build the map string: "node1:0-15,node2:0-15,..."
    local map_str=""
    for i in "${!node_array[@]}"; do
        local node=${node_array[$i]}
        local cores="0-$((threads - 1))"
        
        if [[ -z "$map_str" ]]; then
            map_str="${node}:${cores}"
        else
            map_str="${map_str},${node}:${cores}"
        fi
    done
    
    echo "$map_str"
}

# ============================================================================
# Function: Validate input directory
# ============================================================================

validate_input_file() {
    local file="$1"

    if [[ ! -e "$file" ]]; then
        echo "Error: Input path not found: $file" >&2
        exit 1
    fi

    if [[ ! -d "$file" ]]; then
        echo "Error: Input path is not a directory: $file" >&2
        exit 1
    fi

    if [[ ! -r "$file" || ! -x "$file" ]]; then
        echo "Error: Input directory is not accessible (need read+execute): $file" >&2
        exit 1
    fi

    echo "✓ Input directory validated: $file"
}

# ============================================================================
# Main execution
# ============================================================================

echo "=========================================="
echo "MPI Orchestration Job"
echo "=========================================="
echo "Job ID: ${SLURM_JOB_ID:-local}"
echo "Nodes: $NUM_NODES (max $MAX_THREADS_PER_NODE threads each)"
echo "Node list: $SLURM_NODELIST"
echo "Partition: ${PARTITION:-default}"
echo "Input path: $INPUT_FILE"
echo ""

# Validate input directory
validate_input_file "$INPUT_FILE"

# Build node-to-cores mapping
NODE_MAP=$(build_node_map "$NUM_NODES" "$MAX_THREADS_PER_NODE" "$SLURM_NODELIST")
echo "Node-to-cores mapping:"
echo "  $NODE_MAP"
echo ""

# ============================================================================
# Build the binary if needed
# ============================================================================

echo "=========================================="
echo "Building binary (if needed)..."
echo "=========================================="

if [[ -f "Makefile" ]]; then
    make clean
    make
    echo "✓ Build successful"
else
    echo "⚠ No Makefile found, assuming binary is pre-built"
fi

echo ""

# ============================================================================
# Run the MPI binary
# ============================================================================

echo "=========================================="
echo "Running mpi_orch..."
echo "=========================================="
echo ""

# Launch MPI job
srun ${PARTITION:+--partition=$PARTITION} \
    --mpi=pmi2 \
    --cpus-per-task=$MAX_THREADS_PER_NODE \
    --ntasks=$NUM_NODES \
    ./mpi_orch \
    --input "$INPUT_FILE" \
    --nodes "$NODE_MAP" \
    --threads "$MAX_THREADS_PER_NODE"

EXIT_CODE=$?

echo ""
echo "=========================================="
echo "Job completed with exit code: $EXIT_CODE"
echo "=========================================="

exit $EXIT_CODE
