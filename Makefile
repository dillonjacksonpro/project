SHELL := /bin/bash

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
TEST_BIN_DIR := $(BUILD_DIR)/tests/bin
TARGET := mpi_orch

CC := mpicc
CXX := g++
MPICXX := mpicxx

LOGGING ?= 0
LOGGING_DEFINE := -DMPI_ORCH_LOGGING=$(LOGGING)

SRC := \
	$(SRC_DIR)/mpi_orch.c \
	$(SRC_DIR)/logging.c \
	$(SRC_DIR)/glib_compat.c \
	$(SRC_DIR)/orch_common.c \
	$(SRC_DIR)/aggregation.c \
	$(SRC_DIR)/median.c \
	$(SRC_DIR)/csv_parse.c \
	$(SRC_DIR)/options.c \
	$(SRC_DIR)/file_discovery.c \
	$(SRC_DIR)/fatal.c \
	$(SRC_DIR)/comm_queue.c \
	$(SRC_DIR)/mpi_workers.c

CFLAGS := -std=c11 -O0 -g3 -fno-omit-frame-pointer -pipe \
          -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
          -Wconversion -Wsign-conversion -Wvla \
		  -fopenmp -I$(SRC_DIR) $(LOGGING_DEFINE)

CXXFLAGS := -std=c++17 -O0 -g3 -Wall -Wextra -Wpedantic -I$(SRC_DIR)
TEST_CXXFLAGS := $(CXXFLAGS) -Wno-missing-field-initializers

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

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(SRC)
	@set -euo pipefail; \
	mkdir -p $(BIN_DIR); \
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
	rm -rf $(BUILD_DIR) mpi_orch

$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

$(TEST_BIN_DIR)/test_aggregation: tests/test_aggregation.cpp $(SRC_DIR)/aggregation.c $(SRC_DIR)/orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/aggregation.c -o $(TEST_BIN_DIR)/aggregation.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_aggregation.cpp $(TEST_BIN_DIR)/aggregation.o $(TEST_BIN_DIR)/orch_common.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_median: tests/test_median.cpp $(SRC_DIR)/median.c $(SRC_DIR)/glib_compat.c $(SRC_DIR)/orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/median.c -o $(TEST_BIN_DIR)/median.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_median.cpp $(TEST_BIN_DIR)/median.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_csv_parse: tests/test_csv_parse.cpp $(SRC_DIR)/csv_parse.c $(SRC_DIR)/glib_compat.c $(SRC_DIR)/orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/csv_parse.c -o $(TEST_BIN_DIR)/csv_parse.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_csv_parse.cpp $(TEST_BIN_DIR)/csv_parse.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_options: tests/test_options.cpp $(SRC_DIR)/options.c $(SRC_DIR)/glib_compat.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/options.c -o $(TEST_BIN_DIR)/options.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_options.cpp $(TEST_BIN_DIR)/options.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_file_discovery: tests/test_file_discovery.cpp $(SRC_DIR)/file_discovery.c $(SRC_DIR)/csv_parse.c $(SRC_DIR)/glib_compat.c $(SRC_DIR)/orch_common.c | $(TEST_BIN_DIR)
	@set -euo pipefail; \
	if pkg-config --exists gtest gtest_main; then \
		GTEST_FLAGS="$$(pkg-config --cflags --libs gtest gtest_main)"; \
	elif printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lgtest -lgtest_main -pthread >/dev/null 2>&1; then \
		GTEST_FLAGS="-lgtest -lgtest_main -pthread"; \
	else \
		echo "GoogleTest not found, skipping $@"; \
		exit 0; \
	fi; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/file_discovery.c -o $(TEST_BIN_DIR)/file_discovery.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/csv_parse.c -o $(TEST_BIN_DIR)/csv_parse.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(CXX) $(TEST_CXXFLAGS) tests/test_file_discovery.cpp $(TEST_BIN_DIR)/file_discovery.o $(TEST_BIN_DIR)/csv_parse.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_fatal: tests/test_fatal.cpp $(SRC_DIR)/fatal.c | $(TEST_BIN_DIR)
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
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_fatal.cpp $(TEST_BIN_DIR)/fatal.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_comm_queue: tests/test_comm_queue.cpp $(SRC_DIR)/comm_queue.c $(SRC_DIR)/fatal.c $(SRC_DIR)/glib_compat.c | $(TEST_BIN_DIR)
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
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/comm_queue.c -o $(TEST_BIN_DIR)/comm_queue.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_comm_queue.cpp $(TEST_BIN_DIR)/comm_queue.o $(TEST_BIN_DIR)/fatal.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

$(TEST_BIN_DIR)/test_mpi_workers: tests/test_mpi_workers.cpp $(SRC_DIR)/mpi_workers.c $(SRC_DIR)/comm_queue.c $(SRC_DIR)/fatal.c $(SRC_DIR)/glib_compat.c $(SRC_DIR)/orch_common.c | $(TEST_BIN_DIR)
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
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/mpi_workers.c -o $(TEST_BIN_DIR)/mpi_workers.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/comm_queue.c -o $(TEST_BIN_DIR)/comm_queue.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/fatal.c -o $(TEST_BIN_DIR)/fatal.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/glib_compat.c -o $(TEST_BIN_DIR)/glib_compat.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/orch_common.c -o $(TEST_BIN_DIR)/orch_common.o; \
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $(SRC_DIR)/logging.c -o $(TEST_BIN_DIR)/logging.o; \
	$(MPICXX) $(TEST_CXXFLAGS) tests/test_mpi_workers.cpp $(TEST_BIN_DIR)/mpi_workers.o $(TEST_BIN_DIR)/comm_queue.o $(TEST_BIN_DIR)/fatal.o $(TEST_BIN_DIR)/glib_compat.o $(TEST_BIN_DIR)/orch_common.o $(TEST_BIN_DIR)/logging.o $$GTEST_FLAGS -pthread -o $@

test: $(TEST_TARGETS)
	@set -euo pipefail; \
	for t in $(TEST_TARGETS); do \
		if [[ -x $$t ]]; then \
			echo "Running $$t"; \
			$$t; \
		fi; \
	done
