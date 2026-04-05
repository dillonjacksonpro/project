#ifndef FATAL_H
#define FATAL_H

#include "orch_common.h"

void fatal_no_mpi(const char *msg);
void fatal_rank(MpiRank rank, const char *msg);
void fatal_rank_errno(MpiRank rank, const char *context);
void fatal_rank_mpi(MpiRank rank, int mpi_rc, const char *context);

#endif
