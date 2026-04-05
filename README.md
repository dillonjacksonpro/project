# MPI Orchestrator

This project builds a small MPI-based orchestrator that reads a directory of CSV job files, stripes them across ranks, and prints aggregate results.

## Main entry points

There are four ways to run it:

1. Build and run locally with the shell wrapper.
2. Submit it through SLURM.
3. Run the SLURM submit script directly for a single command-line path.
4. Wrap the submit step in tmux so you can watch submission output and the live job log side by side.

## Build

Build the binary with:

```bash
make
```

Enable progress logging at build time with:

```bash
make LOGGING=1
```

The logging build prints progress messages to stderr from all ranks.

## Logging behavior

Logging is compile-time gated and runtime sampled:

- Build with `LOGGING=1` to include logging code paths.
- By default, regular logs are randomly sampled at 10% to reduce overhead.
- Set `MPI_ORCH_LOG_SAMPLE_PCT` (0-100) to tune sampling at runtime.

Examples:

```bash
# Logging enabled, default sampling (10%)
LOGGING=1 ./run_mpi_orch.sh -i test_data -t 4

# Logging enabled, sample only 2% of regular logs
LOGGING=1 MPI_ORCH_LOG_SAMPLE_PCT=2 ./run_mpi_orch.sh -i test_data -t 4

# Logging enabled, full regular logs (no sampling)
LOGGING=1 MPI_ORCH_LOG_SAMPLE_PCT=100 ./run_mpi_orch.sh -i test_data -t 4
```

Blocking/pause visibility:

- Queue producer blocked on full queue, then resumed (with wait time).
- Queue consumer blocked on empty queue, then resumed (with wait time).
- Receiver thread waiting on incoming MPI messages (logged when wait is noticeable).

These blocking/pause logs are emitted as critical activity markers when logging is enabled so you can see stalls even when regular logs are sampled.

## Local run

The main local runner is [run_mpi_orch.sh](run_mpi_orch.sh).

Examples:

```bash
./run_mpi_orch.sh
./run_mpi_orch.sh -i test_data -t 4
LOGGING=1 ./run_mpi_orch.sh -i test_data -t 4
```

Common flags:

- `-i, --input DIR` selects the input directory.
- `-n, --nodes NUM` sets the node count used to build the node-to-core map.
- `-t, --threads NUM` sets the max threads per node.
- `-p, --partition NAME` sets the SLURM partition when submitting.

## SLURM submit

The SLURM wrapper is [submit.slurm](submit.slurm).

You can submit it directly with:

```bash
sbatch -p rss-class submit.slurm test_data
```

If you already have a path saved in a file, you can pass it by command substitution:

```bash
sbatch -p rss-class submit.slurm "$(cat path.txt)"
```

The script captures stdout and stderr into a job log under:

```text
output/run_<job_id>/mpi_orch.log
```

## tmux wrapper

The tmux launcher is [submit_tmux.sh](submit_tmux.sh).

It creates a tmux session with two windows:

- `submit`: runs the SLURM submission and shows the launcher output.
- `log`: waits for the job log file and tails it live.

Typical usage:

```bash
LOGGING=1 ./submit_tmux.sh path.txt
```

If you omit the argument, it defaults to `path.txt` in the project root.

## How the pieces connect

- [run_mpi_orch.sh](run_mpi_orch.sh) validates the input directory, builds the binary, and launches the orchestrator.
- [submit.slurm](submit.slurm) creates the SLURM allocation, captures job output, and calls [run_mpi_orch.sh](run_mpi_orch.sh).
- [submit_tmux.sh](submit_tmux.sh) wraps [submit.slurm](submit.slurm) in tmux so you can watch submission output and the job log at the same time.

## Input format

The input path must be a directory containing CSV files. The orchestrator enumerates regular files, sorts them, and stripes them across ranks.

Each CSV file is treated as:

- first line: header
- remaining lines: data rows

## Notes

- The default build keeps logging out of the binary.
- `LOGGING=1` enables logging hooks throughout the orchestrator.
- `MPI_ORCH_LOG_SAMPLE_PCT` controls runtime sampling for regular logs when logging is enabled.
- The job log location is determined by `submit.slurm`, so the tmux log window waits for the newest `output/run_<job_id>/mpi_orch.log` file and tails it.
