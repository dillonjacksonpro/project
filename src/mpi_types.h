#ifndef MPI_TYPES_H
#define MPI_TYPES_H

#include <mpi.h>

#include "orch_common.h"

#if defined(MPI_UINT64_T)
#define MEDIAN_MPI_TYPE MPI_UINT64_T
#else
#define MEDIAN_MPI_TYPE MPI_UNSIGNED_LONG_LONG
_Static_assert(sizeof(MedianValue) == sizeof(unsigned long long),
               "MedianValue must match MPI fallback type unsigned long long");
#endif

#endif
