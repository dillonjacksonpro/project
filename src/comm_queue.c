#include "comm_queue.h"

#include <mpi.h>
#include <string.h>

#include "fatal.h"
#include "glib_compat.h"
#include "logging.h"

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
   int mpi_initialized = 0;
   if (q->depth >= q->max_depth && MPI_Initialized(&mpi_initialized) == MPI_SUCCESS && mpi_initialized) {
      MpiRank rank = 0;
      if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS)
         ORCH_LOG(rank, "queue", "queue full, waiting at depth %zu/%zu", q->depth, q->max_depth);
   }
#endif

   while (q->depth >= q->max_depth && !q->producers_done) {
      if (pthread_cond_wait(&q->not_full, &q->mutex) != 0)
         fatal_no_mpi("pthread_cond_wait(comm queue not_full)");
   }

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

   while (q->head == NULL && !q->producers_done) {
      if (pthread_cond_wait(&q->not_empty, &q->mutex) != 0)
         fatal_no_mpi("pthread_cond_wait(comm queue not_empty)");
   }

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
