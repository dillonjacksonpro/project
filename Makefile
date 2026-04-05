SHELL := /bin/bash

CC := mpicc
CXX := g++
MPICXX := mpicxx
TARGET := mpi_orch
SRC := mpi_orch.c glib_compat.c orch_common.c aggregation.c median.c csv_parse.c options.c file_discovery.c fatal.c comm_queue.c mpi_workers.c


CFLAGS := -std=c11 -O0 -g3 -fno-omit-frame-pointer -pipe \
          -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
          -Wconversion -Wsign-conversion -Wvla \
          -fopenmp

CXXFLAGS := -std=c++17 -O0 -g3 -Wall -Wextra -Wpedantic -I.
TEST_CXXFLAGS := $(CXXFLAGS) -Wno-missing-field-initializers

TEST_BIN_DIR := tests/bin
TEST_TARGETS := \
	$(TEST_BIN_DIR)/test_aggregation \
	$(TEST_BIN_DIR)/test_median \
	$(TEST_BIN_DIR)/test_csv_parse \
	$(TEST_BIN_DIR)/test_options \
	$(TEST_BIN_DIR)/test_file_discovery \
	$(TEST_BIN_DIR)/test_fatal \
	$(TEST_BIN_DIR)/test_comm_queue \
	$(TEST_BIN_DIR)/test_mpi_workers

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	@set -euo pipefail; \
	missing=0; \
	if ! command -v gcc >/dev/null 2>&1; then \
		echo "Missing build tool: gcc"; \
		missing=1; \
	fi; \
	if ! command -v $(CC) >/dev/null 2>&1; then \
		echo "Missing build tool: $(CC)"; \
		missing=1; \
	fi; \
	if ! printf 'int main(void){return 0;}\n' | $(CC) -x c -fopenmp -o /dev/null - >/dev/null 2>&1; then \
		echo "Missing OpenMP support for $(CC)"; \
		missing=1; \
	fi; \
	if [[ $$missing -ne 0 ]]; then \
		exit 1; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) $(SRC) -o $@

clean:
	rm -f $(TARGET) $(TEST_TARGETS) $(TEST_BIN_DIR)/*.o

$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

$(TEST_BIN_DIR)/test_aggregation: tests/test_aggregation.cpp aggregation.c orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c aggregation.c -o $(TEST_BIN_DIR)/aggregation.o; \
	$(CC) -std=c11 -I. -c orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_aggregation.cpp $(TEST_BIN_DIR)/aggregation.o $(TEST_BIN_DIR)/orch_common.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_median: tests/test_median.cpp median.c glib_compat.c orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c median.c -o $(TEST_BIN_DIR)/median.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -std=c11 -I. -c orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_median.cpp $(TEST_BIN_DIR)/median.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_csv_parse: tests/test_csv_parse.cpp csv_parse.c glib_compat.c orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c csv_parse.c -o $(TEST_BIN_DIR)/csv_parse.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -std=c11 -I. -c orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_csv_parse.cpp $(TEST_BIN_DIR)/csv_parse.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_options: tests/test_options.cpp options.c glib_compat.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c options.c -o $(TEST_BIN_DIR)/options.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_options.cpp $(TEST_BIN_DIR)/options.o $(TEST_BIN_DIR)/glib_compat.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_file_discovery: tests/test_file_discovery.cpp file_discovery.c csv_parse.c glib_compat.c orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c file_discovery.c -o $(TEST_BIN_DIR)/file_discovery.o; \
	$(CC) -std=c11 -I. -c csv_parse.c -o $(TEST_BIN_DIR)/csv_parse.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -std=c11 -I. -c orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_file_discovery.cpp $(TEST_BIN_DIR)/file_discovery.o $(TEST_BIN_DIR)/csv_parse.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_fatal: tests/test_fatal.cpp fatal.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if ! command -v $(MPICXX) >/dev/null 2>&1; then \
		echo "mpicxx not found, skipping $@"; \
		exit 0; \
	fi; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(MPICXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_fatal.cpp $(TEST_BIN_DIR)/fatal.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_comm_queue: tests/test_comm_queue.cpp comm_queue.c fatal.c glib_compat.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if ! command -v $(MPICXX) >/dev/null 2>&1; then \
		echo "mpicxx not found, skipping $@"; \
		exit 0; \
	fi; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(MPICXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c comm_queue.c -o $(TEST_BIN_DIR)/comm_queue.o; \
	$(CC) -std=c11 -I. -c fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_comm_queue.cpp $(TEST_BIN_DIR)/comm_queue.o $(TEST_BIN_DIR)/fatal.o $(TEST_BIN_DIR)/glib_compat.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_mpi_workers: tests/test_mpi_workers.cpp mpi_workers.c comm_queue.c fatal.c glib_compat.c orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if ! command -v $(MPICXX) >/dev/null 2>&1; then \
		echo "mpicxx not found, skipping $@"; \
		exit 0; \
	fi; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(MPICXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -std=c11 -I. -c mpi_workers.c -o $(TEST_BIN_DIR)/mpi_workers.o; \
	$(CC) -std=c11 -I. -c comm_queue.c -o $(TEST_BIN_DIR)/comm_queue.o; \
	$(CC) -std=c11 -I. -c fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(CC) -std=c11 -I. -c glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -std=c11 -I. -c orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_mpi_workers.cpp $(TEST_BIN_DIR)/mpi_workers.o $(TEST_BIN_DIR)/comm_queue.o $(TEST_BIN_DIR)/fatal.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $$GTEST_FLAGS -pthread -o $@

test: $(TEST_TARGETS)
	@set -euo pipefail; \
	for t in $(TEST_TARGETS); do \
		if [[ -x $$t ]]; then \
			echo "Running $$t"; \
			$$t; \
		fi; \
	done