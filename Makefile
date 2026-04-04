SHELL := /bin/bash

CC := mpicc
TARGET := mpi_orch
SRC := mpi_orch.c

PKG_CONFIG ?= pkg-config
GCC_MODULE ?= gcc
MPI_MODULE ?= mpi
GLIB_MODULE ?= glib

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
	have_pkg=1; \
	have_glib=1; \
	command -v gcc >/dev/null 2>&1 || have_gcc=0; \
	command -v $(CC) >/dev/null 2>&1 || have_mpicc=0; \
	command -v $(PKG_CONFIG) >/dev/null 2>&1 || have_pkg=0; \
	if [[ $$have_pkg -eq 1 ]]; then \
		$(PKG_CONFIG) --exists glib-2.0 >/dev/null 2>&1 || have_glib=0; \
	fi; \
	if [[ $$have_gcc -eq 0 || $$have_mpicc -eq 0 || $$have_pkg -eq 0 || $$have_glib -eq 0 ]]; then \
		if ! command -v module >/dev/null 2>&1 && [[ -f /etc/profile.d/modules.sh ]]; then \
			source /etc/profile.d/modules.sh; \
		fi; \
		if command -v module >/dev/null 2>&1; then \
			if [[ $$have_gcc -eq 0 ]]; then \
				echo "Loading module: $(GCC_MODULE)"; \
				module load $(GCC_MODULE) || true; \
			fi; \
			if [[ $$have_mpicc -eq 0 ]]; then \
				echo "Loading module: $(MPI_MODULE)"; \
				module load $(MPI_MODULE) || true; \
			fi; \
			if [[ $$have_pkg -eq 0 || $$have_glib -eq 0 ]]; then \
				echo "Loading module: $(GLIB_MODULE)"; \
				module load $(GLIB_MODULE) || true; \
			fi; \
		fi; \
	fi; \
	missing=0; \
	if ! command -v gcc >/dev/null 2>&1; then \
		echo "Missing build tool: gcc (or load compiler module, e.g. $(GCC_MODULE))"; \
		missing=1; \
	fi; \
	if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Missing build tool: $(CC) (or load MPI module, e.g. $(MPI_MODULE))"; \
		missing=1; \
	fi; \
	if ! command -v $(PKG_CONFIG) >/dev/null 2>&1; then \
		echo "Missing build tool: $(PKG_CONFIG) (or load GLib/pkg-config module, e.g. $(GLIB_MODULE))"; \
		missing=1; \
	elif ! $(PKG_CONFIG) --exists glib-2.0; then \
		echo "Missing pkg-config package: glib-2.0 (install GLib dev package or load $(GLIB_MODULE))"; \
		missing=1; \
	fi; \
	if ! printf 'int main(void){return 0;}\n' | $(CC) -x c -fopenmp -o /dev/null - >/dev/null 2>&1; then \
		echo "Missing OpenMP support for $(CC) (load compiler/MPI modules with OpenMP, e.g. $(GCC_MODULE) and $(MPI_MODULE))"; \
		missing=1; \
	fi; \
	if [[ $$missing -ne 0 ]]; then \
		exit 1; \
	fi; \
	GLIB_CFLAGS="$$($(PKG_CONFIG) --cflags glib-2.0)"; \
	GLIB_LIBS="$$($(PKG_CONFIG) --libs glib-2.0)"; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) $$GLIB_CFLAGS $< -o $@ $$GLIB_LIBS

clean:
	rm -f $(TARGET)