#ifndef COMM_QUEUE_H
#define COMM_QUEUE_H

#include <stddef.h>

#include "orch_common.h"

#ifdef __cplusplus
#include <atomic>
#define ORCH_ATOMIC(T) std::atomic<T>
#else
#include <stdatomic.h>
#define ORCH_ATOMIC(T) _Atomic(T)
#endif

typedef struct CommNode {
   ORCH_ATOMIC(struct CommNode *) next;
} CommNode;

typedef struct {
   CommNode   node;
   FieldIndex field_idx;
   MpiRank    dest_rank;
   size_t     count;
   MedianValue values[MEDIAN_SEND_BUFFER_SIZE];
} SendBatch;

typedef struct {
   ORCH_ATOMIC(CommNode *) head;
   CommNode           *tail;
   CommNode            stub;
   ORCH_ATOMIC(bool)   producers_done;
} CommQueue;

typedef struct {
   MedianValue values[MEDIAN_SEND_BUFFER_SIZE];
   size_t      count;
} StageBuf;

void comm_queue_init(CommQueue *q);
void comm_queue_push(CommQueue *q, CommNode *node);
CommNode *comm_queue_pop(CommQueue *q);

void stage_buf_flush(StageBuf *buf, FieldIndex field_idx, MpiRank dest_rank, CommQueue *q);
void stage_buf_append(StageBuf *buf, MedianValue val,
                      FieldIndex field_idx, MpiRank dest_rank, CommQueue *q);

#endif
