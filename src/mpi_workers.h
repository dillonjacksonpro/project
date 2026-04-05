#ifndef MPI_WORKERS_H
#define MPI_WORKERS_H

#include <stddef.h>

#include "comm_queue.h"
#include "orch_common.h"

typedef struct {
   CommQueue *queue;
   MpiRank    dest_ranks[N_MEDIAN_FIELDS];
} CommThreadArgs;

typedef struct {
   MedianValue *data;
   size_t       count;
   size_t       capacity;
} MedianSoa;

typedef struct {
   MpiTag    tag;
   size_t    size;
   MpiRank   source_rank;
   MedianSoa soa;
} RecvThreadArgs;

void *comms_thread_func(void *arg);
void *recv_thread_func(void *arg);

#endif
