#include "fatal.h"

#include <errno.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
fatal_no_mpi(const char *msg)
{
   fprintf(stderr, "fatal: %s\n", msg);
   exit(EXIT_FAILURE);
}

void
fatal_rank(MpiRank rank, const char *msg)
{
   fprintf(stderr, "rank %d: %s\n", rank, msg);
   MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

void
fatal_rank_errno(MpiRank rank, const char *context)
{
   fprintf(stderr, "rank %d: %s: %s\n", rank, context, strerror(errno));
   MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

void
fatal_rank_mpi(MpiRank rank, int mpi_rc, const char *context)
{
   char err[MPI_MAX_ERROR_STRING];
   int  err_len = 0;
   MPI_Error_string(mpi_rc, err, &err_len);
   fprintf(stderr, "rank %d: %s failed: %.*s\n", rank, context, err_len, err);
   MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}
