#include "mpi_workers.h"

#include <limits.h>
#include <mpi.h>
#include <sched.h>

#include "fatal.h"
#include "glib_compat.h"

void *
comms_thread_func(void *arg)
{
   CommThreadArgs *a = arg;
   MpiRank rank = 0;
   {
      int rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if (rc != MPI_SUCCESS)
         fatal_no_mpi("MPI_Comm_rank failed in comms thread");
   }

   for (;;) {
      CommNode *node = comm_queue_pop(a->queue);
      if (node != NULL) {
         SendBatch *b = (SendBatch *)node;
         if (b->field_idx < 0 || b->field_idx >= N_MEDIAN_FIELDS)
            fatal_rank(rank, "invalid field index in send batch");
         if (b->count > (size_t)INT_MAX)
            fatal_rank(rank, "send batch exceeds MPI count range");
         int rc = MPI_Send(b->values, (int)b->count, MPI_UNSIGNED_LONG_LONG,
                           b->dest_rank, MEDIAN_TAGS[b->field_idx], MPI_COMM_WORLD);
         if (rc != MPI_SUCCESS)
            fatal_rank_mpi(rank, rc, "MPI_Send(batch)");
         g_free(b);
         continue;
      }

      if (atomic_load_explicit(&a->queue->producers_done, memory_order_acquire)) {
         while ((node = comm_queue_pop(a->queue)) != NULL) {
            SendBatch *b = (SendBatch *)node;
            if (b->field_idx < 0 || b->field_idx >= N_MEDIAN_FIELDS)
               fatal_rank(rank, "invalid field index in drained send batch");
            if (b->count > (size_t)INT_MAX)
               fatal_rank(rank, "drained send batch exceeds MPI count range");
            int rc = MPI_Send(b->values, (int)b->count, MPI_UNSIGNED_LONG_LONG,
                              b->dest_rank, MEDIAN_TAGS[b->field_idx], MPI_COMM_WORLD);
            if (rc != MPI_SUCCESS)
               fatal_rank_mpi(rank, rc, "MPI_Send(drain)");
            g_free(b);
         }
         break;
      }
      sched_yield();
   }

   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++) {
      int rc = MPI_Send(NULL, 0, MPI_UNSIGNED_LONG_LONG,
                        a->dest_ranks[fi], MEDIAN_TAGS[fi], MPI_COMM_WORLD);
      if (rc != MPI_SUCCESS)
         fatal_rank_mpi(rank, rc, "MPI_Send(done)");
   }

   return NULL;
}

void *
recv_thread_func(void *arg)
{
   RecvThreadArgs *a = arg;
   size_t dones = 0;

   while (dones < a->size) {
      MPI_Status status;
      int rc = MPI_Probe(MPI_ANY_SOURCE, a->tag, MPI_COMM_WORLD, &status);
      if (rc != MPI_SUCCESS)
         fatal_rank_mpi(a->source_rank, rc, "MPI_Probe");

      MpiCount mpi_count = 0;
      rc = MPI_Get_count(&status, MPI_UNSIGNED_LONG_LONG, &mpi_count);
      if (rc != MPI_SUCCESS)
         fatal_rank_mpi(a->source_rank, rc, "MPI_Get_count");
      if (mpi_count < 0)
         fatal_rank(a->source_rank, "MPI_Get_count returned MPI_UNDEFINED");
      size_t count = (size_t)mpi_count;

      if (count == 0) {
         rc = MPI_Recv(NULL, 0, MPI_UNSIGNED_LONG_LONG,
                       status.MPI_SOURCE, a->tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         if (rc != MPI_SUCCESS)
            fatal_rank_mpi(a->source_rank, rc, "MPI_Recv(done)");
         dones++;
      } else {
         if (count > SIZE_MAX - a->soa.count)
            fatal_rank(a->source_rank, "receiver count overflow");
         size_t needed = a->soa.count + count;

         if (needed > a->soa.capacity) {
            while (a->soa.capacity < needed) {
               if (a->soa.capacity > SIZE_MAX / 2)
                  fatal_rank(a->source_rank, "receiver capacity overflow");
               a->soa.capacity *= 2;
            }
            if (a->soa.capacity > SIZE_MAX / sizeof(MedianValue))
               fatal_rank(a->source_rank, "receiver allocation size overflow");
            MedianValue *grown = g_realloc(a->soa.data,
                                           a->soa.capacity * sizeof(MedianValue));
            if (grown == NULL)
               fatal_rank(a->source_rank, "out of memory while growing receiver buffer");
            a->soa.data = grown;
         }

         rc = MPI_Recv(a->soa.data + a->soa.count, mpi_count, MPI_UNSIGNED_LONG_LONG,
                       status.MPI_SOURCE, a->tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         if (rc != MPI_SUCCESS)
            fatal_rank_mpi(a->source_rank, rc, "MPI_Recv(data)");
         a->soa.count += count;
      }
   }
   return NULL;
}
