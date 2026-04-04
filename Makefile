SHELL := /bin/bash

CC := mpicc
TARGET := mpi_orch
SRC := mpi_orch.c glib_compat.c

GCC_MODULE ?= gcc
MPI_MODULES ?= mpi openmpi/4.1.1_gcc_9.5.0

CFLAGS := -std=c11 -O0 -g3 -fno-omit-frame-pointer -pipe \
          -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
          -Wconversion -Wsign-conversion -Wvla \
          -fopenmp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	@set -euo pipefail; \
	have_gcc=1; \
	have_mpicc=1; \
	command -v gcc >/dev/null 2>&1 || have_gcc=0; \
	command -v $(CC) >/dev/null 2>&1 || have_mpicc=0; \
	if [[ $$have_gcc -eq 0 || $$have_mpicc -eq 0 ]]; then \
		if ! command -v module >/dev/null 2>&1 && [[ -f /etc/profile.d/modules.sh ]]; then \
			source /etc/profile.d/modules.sh; \
		fi; \
		if command -v module >/dev/null 2>&1; then \
			if [[ $$have_gcc -eq 0 ]]; then \
				echo "Loading module: $(GCC_MODULE)"; \
				module load $(GCC_MODULE) || true; \
			fi; \
			if [[ $$have_mpicc -eq 0 ]]; then \
				for mpi_module in $(MPI_MODULES); do \
					echo "Loading module: $$mpi_module"; \
					module load $$mpi_module && break || true; \
				done; \
			fi; \
		fi; \
	fi; \
	missing=0; \
	if ! command -v gcc >/dev/null 2>&1; then \
		echo "Missing build tool: gcc (or load compiler module, e.g. $(GCC_MODULE))"; \
		missing=1; \
	fi; \
	if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Missing build tool: $(CC) (or load an MPI module, e.g. $(word 1,$(MPI_MODULES)) or openmpi/4.1.1_gcc_9.5.0)"; \
		missing=1; \
	fi; \
	if ! printf 'int main(void){return 0;}\n' | $(CC) -x c -fopenmp -o /dev/null - >/dev/null 2>&1; then \
		echo "Missing OpenMP support for $(CC) (load compiler/MPI modules with OpenMP, e.g. $(GCC_MODULE) and one of: $(MPI_MODULES))"; \
		missing=1; \
	fi; \
	if [[ $$missing -ne 0 ]]; then \
		exit 1; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) $(SRC) -o $@

clean:
	rm -f $(TARGET)