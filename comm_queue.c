#include "comm_queue.h"

#include <mpi.h>
#include <string.h>

#include "fatal.h"
#include "glib_compat.h"

void
comm_queue_init(CommQueue *q)
{
   atomic_init(&q->stub.next, NULL);
   atomic_init(&q->head, &q->stub);
   q->tail = &q->stub;
   atomic_init(&q->producers_done, false);
}

void
comm_queue_push(CommQueue *q, CommNode *node)
{
   atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
   CommNode *prev = atomic_exchange_explicit(&q->head, node, memory_order_acq_rel);
   atomic_store_explicit(&prev->next, node, memory_order_release);
}

CommNode *
comm_queue_pop(CommQueue *q)
{
   CommNode *tail = q->tail;
   CommNode *next = atomic_load_explicit(&tail->next, memory_order_acquire);

   if (tail == &q->stub) {
      if (next == NULL)
         return NULL;
      q->tail = next;
      tail = next;
      next = atomic_load_explicit(&tail->next, memory_order_acquire);
   }
   if (next != NULL) {
      q->tail = next;
      return tail;
   }

   CommNode *head = atomic_load_explicit(&q->head, memory_order_acquire);
   if (tail != head)
      return NULL;

   comm_queue_push(q, &q->stub);
   next = atomic_load_explicit(&tail->next, memory_order_acquire);
   if (next != NULL) {
      q->tail = next;
      return tail;
   }
   return NULL;
}

void
stage_buf_flush(StageBuf *buf, FieldIndex field_idx, MpiRank dest_rank, CommQueue *q)
{
   if (buf->count == 0)
      return;

   SendBatch *batch = g_new(SendBatch, 1);
   if (batch == NULL) {
      MpiRank rank = 0;
      int rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if (rc != MPI_SUCCESS)
         fatal_no_mpi("MPI_Comm_rank failed in stage_buf_flush");
      fatal_rank(rank, "out of memory while creating send batch");
   }

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
