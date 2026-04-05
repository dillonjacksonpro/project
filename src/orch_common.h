#ifndef ORCH_COMMON_H
#define ORCH_COMMON_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_JOBS            30000
#define CSV_FIELD1_MAX      256
#define AGG_TOP_N           10
#define N_MEDIAN_FIELDS     3

/* Number of 64-bit values buffered per thread per field before pushing to
   the comms queue. */
#define MEDIAN_SEND_BUFFER_SIZE 4096

/* Maximum queued send batches before producer threads block. */
#define COMM_QUEUE_MAX_DEPTH 128

/* Hard cap for values retained by a single median receiver thread. */
#define MAX_RECEIVER_VALUES ((size_t)1 << 30)

/* MPI tags — one per median field; zero-length message on same tag = "done" */
#define TAG_MEDIAN_F0  100
#define TAG_MEDIAN_F1  101
#define TAG_MEDIAN_F2  102

typedef uint64_t MetricValue;
typedef uint64_t MedianValue;
#define PRI_METRIC  PRIu64
#define SCN_METRIC  SCNu64

typedef int      FieldIndex;
typedef int      MpiRank;
typedef int      MpiTag;
typedef int      MpiCount;

typedef struct {
   char        field1[CSV_FIELD1_MAX];
   MetricValue field2;
   MetricValue field3;
   MetricValue field4;
} CsvRow;

typedef struct {
   CsvRow      row;
   MetricValue key;
} HeapEntry;

typedef struct {
   HeapEntry entries[AGG_TOP_N];
   size_t    count;
} RowHeap;

typedef struct {
   size_t      total_lines;
   MetricValue sum_field2;
   MetricValue sum_field3;
   MetricValue sum_field4;
   RowHeap     top_field2;
   RowHeap     bot_field2;
   RowHeap     top_field3;
   RowHeap     bot_field3;
   RowHeap     top_field4;
   RowHeap     bot_field4;
} NodeAgg;

typedef struct {
   NodeAgg     agg;
   MedianValue medians[N_MEDIAN_FIELDS];
   bool        has_median[N_MEDIAN_FIELDS];
} GatherPayload;

extern const MpiTag MEDIAN_TAGS[N_MEDIAN_FIELDS];

#endif
