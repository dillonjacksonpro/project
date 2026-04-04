# SLURM Submission Script for MPI Orchestration

This script (`submit.slurm`) automates job submission and resource management for the `mpi_orch` binary.

## Features

- **Module Management**: Automatically checks and loads required MPI and OpenMP modules
- **Node-to-Core Mapping**: Generates and passes node/core availability to the binary
- **Input File Validation**: Ensures the target file exists and is readable
- **Automatic Build**: Rebuilds the binary before execution
- **Resource Configuration**: Specifies nodes, threads, and time limits

## Usage

### Basic submission:
```bash
sbatch submit.slurm <input_file>
```

### Examples:

```bash
# Submit with a specific input file
sbatch submit.slurm data/input.txt

# Submit with default working directory (.)
sbatch submit.slurm
```

## Customization

Edit the SLURM directives at the top of the script to adjust:

```bash
#SBATCH --nodes=2              # Number of compute nodes
#SBATCH --ntasks-per-node=1    # MPI tasks per node
#SBATCH --cpus-per-task=16     # OpenMP threads per task
#SBATCH --time=00:10:00        # Maximum wall-clock time
#SBATCH --job-name=mpi_orch    # Job name
#SBATCH --output=...           # Output log file
```

## Configuration Sections

### 1. Required Modules
Edit the `REQUIRED_MODULES` array to specify which modules your system needs:
```bash
REQUIRED_MODULES=("intel/19.0" "intel-mkl/19.0" "impi")
```

Common modules:
- **MPI**: `impi`, `openmpi`, `mpich`
- **Compilers**: `intel`, `gcc`, `pgi`
- **Libraries**: `intel-mkl`, `fftw`, `hdf5`

### 2. Resource Allocation

The script automatically:
- Allocates `$SLURM_NNODES` compute nodes
- Assigns `$SLURM_CPUS_PER_TASK` threads per node
- Generates a node map: `node001:0-15,node002:0-15,...`

### 3. Node-to-Core Mapping

The mapping string passed to the binary shows which logical cores (0-N) are available on each node:
```
node001:0-15,node002:0-15,node003:0-15,node004:0-15
```

This is passed via `--nodes` argument to `mpi_orch`.

## What the Script Does

1. **Module Check** → Loads MPI, OpenMP, and related modules
2. **Validation** → Verifies input file exists and is readable
3. **Node Mapping** → Parses SLURM nodelist and creates core availability map
4. **Build** → Runs `make clean && make` to rebuild binary
5. **Execution** → Launches MPI job with `srun`:
   ```bash
   srun --mpi=pmi2 ./mpi_orch --input <FILE> --nodes <MAP> --threads <N>
   ```

## Monitoring Jobs

```bash
# View job status
squeue -j <JOBID>

# View job details
scontrol show job <JOBID>

# View output log (while running or after completion)
tail -f mpi_orch_*.out
```

## Example Output

```
==========================================
SLURM MPI Orchestration Job
==========================================
Job ID: 12345
Nodes: 4 (max 16 threads each)
Node list: node[001-004]
Input file: data/input.txt

==========================================
Checking and loading required modules...
==========================================
✓ Loading module: intel/19.0
✓ Loading module: intel-mkl/19.0
✓ Loading module: impi

✓ Input file validated: data/input.txt

Node-to-cores mapping:
  node001:0-15,node002:0-15,node003:0-15,node004:0-15

```

## Troubleshooting

### Module not found
```
✗ Module not available: impi
```
**Solution**: Check available modules with `module avail` and update `REQUIRED_MODULES`

### Input file not found
```
Error: Input file not found: data/input.txt
```
**Solution**: Verify file path and ensure it exists before submission

### Build fails
If the binary fails to build, check:
- GLib is installed: `pkg-config --modversion glib-2.0`
- MPI headers are available: Check loaded modules
- Makefile is present and valid

### Job exceeds time limit
Increase `--time=HH:MM:SS` in the SLURM directives

## Advanced Modifications

### Custom pre-processing
Add before `srun` call:
```bash
# Example: set OpenMP affinity
export OMP_PROC_BIND=close
export OMP_PLACES=cores
```

### Environment variables for the binary
```bash
export BINARY_CUSTOM_VAR=value
srun ./mpi_orch ...
```

### Pin tasks to specific cores
Modify `srun` arguments:
```bash
srun --cpu-bind=cores ./mpi_orch ...
```

## Notes

- The script runs `make clean && make` each time. Comment this out to skip rebuilds.
- Default job name is `mpi_orch` (change via `-J` flag when submitting).
- Output logs are written to `mpi_orch_<JOBID>.out` and `mpi_orch_<JOBID>.err`.
