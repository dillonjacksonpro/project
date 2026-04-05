#ifndef FILE_DISCOVERY_H
#define FILE_DISCOVERY_H

#include <stdbool.h>
#include <stddef.h>

#include "orch_common.h"

bool discover_rank_jobs(const char *input_path,
                        MpiRank rank,
                        size_t mpi_size,
                        char *job_array[MAX_JOBS],
                        size_t *job_count_out);

#endif
