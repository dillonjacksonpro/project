#include "comm_queue.h"

#include <mpi.h>
#include <string.h>
#include <time.h>

#include "fatal.h"
#include "glib_compat.h"
#include "logging.h"

#if MPI_ORCH_LOGGING
static uint64_t
mono_now_ns(void)
{
   struct timespec ts;
   if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
      return 0;
   return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static MpiRank
log_rank_or_zero(void)
{
   int mpi_initialized = 0;
   if (MPI_Initialized(&mpi_initialized) != MPI_SUCCESS || !mpi_initialized)
      return 0;

   MpiRank rank = 0;
   if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS)
      return 0;
   return rank;
}
#endif

void
comm_queue_init(CommQueue *q)
{
   q->head = NULL;
   q->tail = NULL;
   q->depth = 0;
   q->max_depth = COMM_QUEUE_MAX_DEPTH;
   q->producers_done = false;
   if (pthread_mutex_init(&q->mutex, NULL) != 0)
      fatal_no_mpi("pthread_mutex_init(comm queue)");
   if (pthread_cond_init(&q->not_empty, NULL) != 0)
      fatal_no_mpi("pthread_cond_init(comm queue not_empty)");
   if (pthread_cond_init(&q->not_full, NULL) != 0)
      fatal_no_mpi("pthread_cond_init(comm queue not_full)");
}

void
comm_queue_destroy(CommQueue *q)
{
   if (pthread_cond_destroy(&q->not_full) != 0)
      fatal_no_mpi("pthread_cond_destroy(comm queue not_full)");
   if (pthread_cond_destroy(&q->not_empty) != 0)
      fatal_no_mpi("pthread_cond_destroy(comm queue not_empty)");
   if (pthread_mutex_destroy(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_destroy(comm queue)");
}

void
comm_queue_push(CommQueue *q, CommNode *node)
{
   if (pthread_mutex_lock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_lock(comm queue push)");

#if MPI_ORCH_LOGGING
   bool blocked = false;
   size_t wait_loops = 0;
   uint64_t wait_start_ns = 0;
#endif

   while (q->depth >= q->max_depth && !q->producers_done) {
#if MPI_ORCH_LOGGING
      if (!blocked) {
         wait_start_ns = mono_now_ns();
         blocked = true;
         ORCH_LOG_ALWAYS(log_rank_or_zero(), "block",
                         "queue producer blocked: depth=%zu/%zu", q->depth, q->max_depth);
      }
      wait_loops++;
   #endif
      if (pthread_cond_wait(&q->not_full, &q->mutex) != 0)
         fatal_no_mpi("pthread_cond_wait(comm queue not_full)");
   }

   #if MPI_ORCH_LOGGING
   if (blocked) {
      uint64_t wait_end_ns = mono_now_ns();
      uint64_t waited_ns = (wait_end_ns >= wait_start_ns) ? (wait_end_ns - wait_start_ns) : 0;
      ORCH_LOG_ALWAYS(log_rank_or_zero(), "block",
                      "queue producer resumed after %.3f ms (%zu wakeups)",
                      (double)waited_ns / 1.0e6, wait_loops);
   }
#endif

   atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
   if (q->tail != NULL) {
      atomic_store_explicit(&q->tail->next, node, memory_order_release);
   } else {
      q->head = node;
   }
   q->tail = node;
   q->depth++;

   if (pthread_cond_signal(&q->not_empty) != 0)
      fatal_no_mpi("pthread_cond_signal(comm queue not_empty)");
   if (pthread_mutex_unlock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_unlock(comm queue push)");
}

CommNode *
comm_queue_pop(CommQueue *q)
{
   if (pthread_mutex_lock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_lock(comm queue pop)");

   if (q->head == NULL) {
      if (pthread_mutex_unlock(&q->mutex) != 0)
         fatal_no_mpi("pthread_mutex_unlock(comm queue pop empty)");
      return NULL;
   }

   CommNode *node = q->head;
   q->head = atomic_load_explicit(&node->next, memory_order_acquire);
   if (q->head == NULL)
      q->tail = NULL;
   q->depth--;

   if (pthread_cond_signal(&q->not_full) != 0)
      fatal_no_mpi("pthread_cond_signal(comm queue not_full)");
   if (pthread_mutex_unlock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_unlock(comm queue pop)");
   return node;
}

CommNode *
comm_queue_pop_wait(CommQueue *q)
{
   if (pthread_mutex_lock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_lock(comm queue pop_wait)");

#if MPI_ORCH_LOGGING
   bool blocked = false;
   size_t wait_loops = 0;
   uint64_t wait_start_ns = 0;
#endif

   while (q->head == NULL && !q->producers_done) {
#if MPI_ORCH_LOGGING
      if (!blocked) {
         wait_start_ns = mono_now_ns();
         blocked = true;
         ORCH_LOG_ALWAYS(log_rank_or_zero(), "block", "queue consumer blocked: queue empty");
      }
      wait_loops++;
#endif
      if (pthread_cond_wait(&q->not_empty, &q->mutex) != 0)
         fatal_no_mpi("pthread_cond_wait(comm queue not_empty)");
   }

#if MPI_ORCH_LOGGING
   if (blocked) {
      uint64_t wait_end_ns = mono_now_ns();
      uint64_t waited_ns = (wait_end_ns >= wait_start_ns) ? (wait_end_ns - wait_start_ns) : 0;
      ORCH_LOG_ALWAYS(log_rank_or_zero(), "block",
                      "queue consumer resumed after %.3f ms (%zu wakeups)",
                      (double)waited_ns / 1.0e6, wait_loops);
   }
#endif

   if (q->head == NULL && q->producers_done) {
      if (pthread_mutex_unlock(&q->mutex) != 0)
         fatal_no_mpi("pthread_mutex_unlock(comm queue pop_wait done)");
      return NULL;
   }

   CommNode *node = q->head;
   q->head = atomic_load_explicit(&node->next, memory_order_acquire);
   if (q->head == NULL)
      q->tail = NULL;
   q->depth--;

   if (pthread_cond_signal(&q->not_full) != 0)
      fatal_no_mpi("pthread_cond_signal(comm queue not_full)");
   if (pthread_mutex_unlock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_unlock(comm queue pop_wait)");
   return node;
}

void
comm_queue_mark_done(CommQueue *q)
{
   if (pthread_mutex_lock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_lock(comm queue mark_done)");
   q->producers_done = true;
   if (pthread_cond_broadcast(&q->not_empty) != 0)
      fatal_no_mpi("pthread_cond_broadcast(comm queue not_empty)");
   if (pthread_cond_broadcast(&q->not_full) != 0)
      fatal_no_mpi("pthread_cond_broadcast(comm queue not_full)");
   if (pthread_mutex_unlock(&q->mutex) != 0)
      fatal_no_mpi("pthread_mutex_unlock(comm queue mark_done)");
}

void
stage_buf_flush(StageBuf *buf, FieldIndex field_idx, MpiRank dest_rank, CommQueue *q)
{
   if (buf->count == 0)
      return;

   MpiRank rank = 0;
   {
      int mpi_initialized = 0;
      if (MPI_Initialized(&mpi_initialized) == MPI_SUCCESS && mpi_initialized) {
         int rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
         if (rc != MPI_SUCCESS)
            rank = 0;
      }
   }

   SendBatch *batch = g_new(SendBatch, 1);
   if (batch == NULL) {
      int mpi_initialized = 0;
      if (MPI_Initialized(&mpi_initialized) == MPI_SUCCESS && mpi_initialized) {
         int rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
         if (rc == MPI_SUCCESS) {
            fatal_rank(rank, "out of memory while creating send batch");
         }
      }
      fatal_no_mpi("out of memory while creating send batch");
   }

   ORCH_LOG(rank, "queue", "flushing field %d to rank %d with %zu values",
            field_idx, dest_rank, buf->count);

   atomic_init(&batch->node.next, NULL);
   memcpy(batch->values, buf->values, buf->count * sizeof(MedianValue));
   batch->count = buf->count;
   batch->field_idx = field_idx;
   batch->dest_rank = dest_rank;
   comm_queue_push(q, &batch->node);
   buf->count = 0;
}

void
stage_buf_append(StageBuf *buf, MedianValue val,
                 FieldIndex field_idx, MpiRank dest_rank, CommQueue *q)
{
   if (buf->count >= MEDIAN_SEND_BUFFER_SIZE) {
      stage_buf_flush(buf, field_idx, dest_rank, q);
      if (buf->count >= MEDIAN_SEND_BUFFER_SIZE)
         fatal_no_mpi("stage buffer did not flush as expected");
   }

   buf->values[buf->count++] = val;
   if (buf->count == MEDIAN_SEND_BUFFER_SIZE)
      stage_buf_flush(buf, field_idx, dest_rank, q);
}
