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
#   -i, --input FILE         Input file to process (default: input.txt)
#   -n, --nodes NUM          Number of nodes (default: from SLURM or 1)
#   -t, --threads NUM        Max threads per node (default: from SLURM or 16)
#   -h, --help              Show this help message
#
# ============================================================================

set -e

# ============================================================================
# Configuration - Defaults (can be overridden by args or SLURM env)
# ============================================================================

INPUT_FILE="input.txt"
NUM_NODES=${SLURM_NNODES:-1}
MAX_THREADS_PER_NODE=${SLURM_CPUS_PER_TASK:-16}
SLURM_NODELIST=${SLURM_NODELIST:-"localhost"}

# Required modules
REQUIRED_MODULES=("intel/19.0" "intel-mkl/19.0" "impi")

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
        -h|--help)
            cat << 'EOF'
MPI Orchestration Runner

Usage: run_mpi_orch.sh [OPTIONS]

Options:
  -i, --input FILE       Input file to process (default: input.txt)
  -n, --nodes NUM        Number of nodes (default: from SLURM or 1)
  -t, --threads NUM      Max threads per node (default: from SLURM or 16)
  -h, --help             Show this help message

Examples:
  # Use defaults
  ./run_mpi_orch.sh

  # Custom input file
  ./run_mpi_orch.sh -i mydata.txt

  # Simulate 4-node cluster with 8 threads each
  ./run_mpi_orch.sh -i data.txt -n 4 -t 8

  # Via SLURM (automatic):
  sbatch submit.slurm mydata.txt

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
# Function: Check and load modules
# ============================================================================

check_and_load_modules() {
    local module_list=("$@")
    
    echo "=========================================="
    echo "Checking and loading required modules..."
    echo "=========================================="
    
    for module in "${module_list[@]}"; do
        if module avail "$module" &>/dev/null; then
            echo "✓ Loading module: $module"
            module load "$module" 2>/dev/null || {
                echo "  ⚠ Warning: Failed to load $module"
            }
        else
            echo "✗ Module not available: $module"
        fi
    done
    
    echo ""
    echo "Currently loaded modules:"
    module list 2>&1 | grep -v "^$" || echo "  (none)"
    echo ""
}

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
# Function: Validate input file
# ============================================================================

validate_input_file() {
    local file="$1"
    
    if [[ ! -f "$file" ]]; then
        echo "Error: Input file not found: $file" >&2
        exit 1
    fi
    
    if [[ ! -r "$file" ]]; then
        echo "Error: Input file is not readable: $file" >&2
        exit 1
    fi
    
    echo "✓ Input file validated: $file"
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
echo "Input file: $INPUT_FILE"
echo ""

# Check and load required modules
check_and_load_modules "${REQUIRED_MODULES[@]}"

# Validate input file
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
srun --mpi=pmi2 \
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
