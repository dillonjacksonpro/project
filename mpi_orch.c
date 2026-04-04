#include <fcntl.h>
/* #include <glib.h> */
#include "glib_compat.h"
#include <inttypes.h>
#include <mpi.h>
#include <omp.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_JOBS            4096
#define CSV_FIELD1_MAX      256
#define AGG_TOP_N           10
#define N_MEDIAN_FIELDS     3

/* Number of 64-bit values buffered per thread per field before pushing to
   the comms queue. 4096 values = 32 KiB — one flush per cache-warm batch. */
#define MEDIAN_SEND_BUFFER_SIZE 4096

/* MPI tags — one per median field; zero-length message on same tag = "done" */
#define TAG_MEDIAN_F0  100
#define TAG_MEDIAN_F1  101
#define TAG_MEDIAN_F2  102

/* Change MetricValue and update the two format macros to match */
typedef uint64_t MetricValue;
typedef uint64_t MedianValue;
#define PRI_METRIC  PRIu64
#define SCN_METRIC  SCNu64

typedef int      FieldIndex;
typedef int      MpiRank;
typedef int      MpiTag;
typedef int      MpiCount;

static const MpiTag MEDIAN_TAGS[N_MEDIAN_FIELDS]    = { TAG_MEDIAN_F0, TAG_MEDIAN_F1, TAG_MEDIAN_F2 };

/* ── options ─────────────────────────────────────────────────────── */
typedef struct {
   char *input_path;
   char *nodes_map;
   int   max_threads;
} Options;

/* ── aggregation ─────────────────────────────────────────────────── */
typedef struct {
   char     field1[CSV_FIELD1_MAX];
   MetricValue field2;
   MetricValue field3;
   MetricValue field4;
} CsvRow;

typedef struct {
   CsvRow   row;
   MetricValue key;
} HeapEntry;

typedef struct {
   HeapEntry entries[AGG_TOP_N];
   size_t    count;
} RowHeap;

typedef struct {
   size_t    total_lines;
   MetricValue sum_field2;
   MetricValue sum_field3;
   MetricValue sum_field4;
   RowHeap  top_field2, bot_field2;
   RowHeap  top_field3, bot_field3;
   RowHeap  top_field4, bot_field4;
} NodeAgg;

static void
row_heap_try_insert_top(RowHeap *h, const CsvRow *row, MetricValue key)
{
   if (h->count < AGG_TOP_N) {
      h->entries[h->count++] = (HeapEntry){ .row = *row, .key = key };
      return;
   }
   size_t worst = 0;
   for (size_t i = 1; i < AGG_TOP_N; i++)
      if (h->entries[i].key < h->entries[worst].key) worst = i;
   if (key > h->entries[worst].key)
      h->entries[worst] = (HeapEntry){ .row = *row, .key = key };
}

static void
row_heap_try_insert_bot(RowHeap *h, const CsvRow *row, MetricValue key)
{
   if (h->count < AGG_TOP_N) {
      h->entries[h->count++] = (HeapEntry){ .row = *row, .key = key };
      return;
   }
   size_t worst = 0;
   for (size_t i = 1; i < AGG_TOP_N; i++)
      if (h->entries[i].key > h->entries[worst].key) worst = i;
   if (key < h->entries[worst].key)
      h->entries[worst] = (HeapEntry){ .row = *row, .key = key };
}

static void
node_agg_update(NodeAgg *agg, const CsvRow *row)
{
   agg->total_lines++;
   agg->sum_field2 += row->field2;
   agg->sum_field3 += row->field3;
   agg->sum_field4 += row->field4;
   row_heap_try_insert_top(&agg->top_field2, row, row->field2);
   row_heap_try_insert_bot(&agg->bot_field2, row, row->field2);
   row_heap_try_insert_top(&agg->top_field3, row, row->field3);
   row_heap_try_insert_bot(&agg->bot_field3, row, row->field3);
   row_heap_try_insert_top(&agg->top_field4, row, row->field4);
   row_heap_try_insert_bot(&agg->bot_field4, row, row->field4);
}

static void
node_agg_merge(NodeAgg *dst, const NodeAgg *src)
{
   dst->total_lines += src->total_lines;
   dst->sum_field2  += src->sum_field2;
   dst->sum_field3  += src->sum_field3;
   dst->sum_field4  += src->sum_field4;
   for (size_t i = 0; i < src->top_field2.count; i++)
      row_heap_try_insert_top(&dst->top_field2, &src->top_field2.entries[i].row, src->top_field2.entries[i].key);
   for (size_t i = 0; i < src->bot_field2.count; i++)
      row_heap_try_insert_bot(&dst->bot_field2, &src->bot_field2.entries[i].row, src->bot_field2.entries[i].key);
   for (size_t i = 0; i < src->top_field3.count; i++)
      row_heap_try_insert_top(&dst->top_field3, &src->top_field3.entries[i].row, src->top_field3.entries[i].key);
   for (size_t i = 0; i < src->bot_field3.count; i++)
      row_heap_try_insert_bot(&dst->bot_field3, &src->bot_field3.entries[i].row, src->bot_field3.entries[i].key);
   for (size_t i = 0; i < src->top_field4.count; i++)
      row_heap_try_insert_top(&dst->top_field4, &src->top_field4.entries[i].row, src->top_field4.entries[i].key);
   for (size_t i = 0; i < src->bot_field4.count; i++)
      row_heap_try_insert_bot(&dst->bot_field4, &src->bot_field4.entries[i].row, src->bot_field4.entries[i].key);
}

/* ── median — quickselect O(n) average ───────────────────────────── */
static void
u64_swap(MedianValue *a, MedianValue *b) { MedianValue t = *a; *a = *b; *b = t; }

static MedianValue
u64_quickselect(MedianValue *arr, size_t n, size_t k)
{
   size_t lo = 0, hi = n - 1;
   while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      u64_swap(&arr[mid], &arr[hi]);
      MedianValue pivot = arr[hi];
      size_t store = lo;
      for (size_t i = lo; i < hi; i++)
         if (arr[i] <= pivot) u64_swap(&arr[store++], &arr[i]);
      u64_swap(&arr[store], &arr[hi]);
      if (store == k)     break;
      else if (k < store) hi = store - 1;
      else                lo = store + 1;
   }
   return arr[k];
}

static MedianValue
find_median(const MedianValue *src, size_t n)
{
   if (n == 0) return 0;
   if (n == 1) return src[0];
   MedianValue *arr = g_new(MedianValue, n);
   memcpy(arr, src, n * sizeof(MedianValue));
   MedianValue med;
   if (n & 1) {
      med = u64_quickselect(arr, n, n / 2);
   } else {
      /* after placing (n/2-1)-th, upper partition [n/2..n-1] >= arr[n/2-1];
         its minimum is the (n/2)-th order statistic                          */
      u64_quickselect(arr, n, n / 2 - 1);
      MedianValue lo_val = arr[n / 2 - 1];
      MedianValue hi_val = arr[n / 2];
      for (size_t i = n / 2 + 1; i < n; i++)
         if (arr[i] < hi_val) hi_val = arr[i];
      med = lo_val / 2 + hi_val / 2 + ((lo_val & 1) + (hi_val & 1)) / 2;
   }
   g_free(arr);
   return med;
}

/* ── MPSC lock-free queue (Vyukov) ───────────────────────────────── */
/* Intrusive: next pointer is the first field of every node.
   The stub embedded in CommQueue is a plain CommNode (no values array),
   so sizeof(CommQueue) stays small regardless of MEDIAN_SEND_BUFFER_SIZE. */

typedef struct CommNode {
   _Atomic(struct CommNode *) next;
} CommNode;

/* SendBatch: CommNode MUST be the first member so the cast
   (CommNode *) <-> (SendBatch *) is valid.                   */
typedef struct {
   CommNode node;
   FieldIndex field_idx;
   MpiRank    dest_rank;
   size_t     count;
   MedianValue values[MEDIAN_SEND_BUFFER_SIZE];
} SendBatch;

typedef struct {
   _Atomic(CommNode *) head;
   CommNode           *tail;          /* consumer-only, no atomic needed */
   CommNode            stub;          /* sentinel — never cast to SendBatch */
   _Atomic bool        producers_done;
} CommQueue;

static void
comm_queue_init(CommQueue *q)
{
   atomic_init(&q->stub.next,      NULL);
   atomic_init(&q->head,           &q->stub);
   q->tail = &q->stub;
   atomic_init(&q->producers_done, false);
}

/* wait-free: one exchange + one store */
static void
comm_queue_push(CommQueue *q, CommNode *node)
{
   atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
   CommNode *prev = atomic_exchange_explicit(&q->head, node, memory_order_acq_rel);
   atomic_store_explicit(&prev->next, node, memory_order_release);
}

/* lock-free: returns NULL when empty or during a producer mid-push gap */
static CommNode *
comm_queue_pop(CommQueue *q)
{
   CommNode *tail = q->tail;
   CommNode *next = atomic_load_explicit(&tail->next, memory_order_acquire);

   if (tail == &q->stub) {
      if (next == NULL) return NULL;
      q->tail = next;
      tail    = next;
      next    = atomic_load_explicit(&tail->next, memory_order_acquire);
   }
   if (next != NULL) {
      q->tail = next;
      return tail;
   }
   CommNode *head = atomic_load_explicit(&q->head, memory_order_acquire);
   if (tail != head) return NULL;       /* producer mid-push; caller retries  */
   comm_queue_push(q, &q->stub);        /* re-insert stub to close the cycle  */
   next = atomic_load_explicit(&tail->next, memory_order_acquire);
   if (next != NULL) {
      q->tail = next;
      return tail;
   }
   return NULL;
}

/* ── per-thread staging buffer ───────────────────────────────────── */
typedef struct {
   MedianValue values[MEDIAN_SEND_BUFFER_SIZE];
   size_t    count;
} StageBuf;

static void
stage_buf_flush(StageBuf *buf, FieldIndex field_idx, MpiRank dest_rank, CommQueue *q)
{
   if (buf->count == 0) return;
   SendBatch *batch = g_new(SendBatch, 1);
   atomic_init(&batch->node.next, NULL);
   memcpy(batch->values, buf->values, (size_t)buf->count * sizeof(MedianValue));
   batch->count     = buf->count;
   batch->field_idx = field_idx;
   batch->dest_rank = dest_rank;
   comm_queue_push(q, &batch->node);
   buf->count = 0;
}

static inline void
stage_buf_append(StageBuf *buf, MedianValue val,
                 FieldIndex field_idx, MpiRank dest_rank, CommQueue *q)
{
   buf->values[buf->count++] = val;
   if (buf->count == MEDIAN_SEND_BUFFER_SIZE)
      stage_buf_flush(buf, field_idx, dest_rank, q);
}

/* ── comms thread ────────────────────────────────────────────────── */
typedef struct {
   CommQueue *queue;
   MpiRank    dest_ranks[N_MEDIAN_FIELDS];
} CommThreadArgs;

static void *
comms_thread_func(void *arg)
{
   CommThreadArgs *a = arg;

   for (;;) {
      CommNode *node = comm_queue_pop(a->queue);
      if (node != NULL) {
         SendBatch *b = (SendBatch *)node;
         MPI_Send(b->values, (int)b->count, MPI_UNSIGNED_LONG_LONG,
                  b->dest_rank, MEDIAN_TAGS[b->field_idx], MPI_COMM_WORLD);
         g_free(b);
         continue;
      }
      if (atomic_load_explicit(&a->queue->producers_done, memory_order_acquire)) {
         /* drain any items that raced in just before the flag was set */
         while ((node = comm_queue_pop(a->queue)) != NULL) {
            SendBatch *b = (SendBatch *)node;
            MPI_Send(b->values, (int)b->count, MPI_UNSIGNED_LONG_LONG,
                     b->dest_rank, MEDIAN_TAGS[b->field_idx], MPI_COMM_WORLD);
            g_free(b);
         }
         break;
      }
      sched_yield();
   }

   /* send zero-length "done" sentinel on each field's tag */
   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
      MPI_Send(NULL, 0, MPI_UNSIGNED_LONG_LONG,
               a->dest_ranks[fi], MEDIAN_TAGS[fi], MPI_COMM_WORLD);

   return NULL;
}

/* ── receiver thread ─────────────────────────────────────────────── */
typedef struct {
   MedianValue *data;
   size_t    count;
   size_t    capacity;
} MedianSoa;

typedef struct {
   MpiTag    tag;   /* listens on this MPI tag only */
   size_t    size;  /* number of "done" sentinels to wait for (= mpi_size) */
   MedianSoa soa;   /* built here; read by main thread after pthread_join   */
} RecvThreadArgs;

static void *
recv_thread_func(void *arg)
{
   RecvThreadArgs *a = arg;
   size_t dones = 0;

   while (dones < a->size) {
      MPI_Status status;
      MPI_Probe(MPI_ANY_SOURCE, a->tag, MPI_COMM_WORLD, &status);

      MpiCount mpi_count = 0;
      MPI_Get_count(&status, MPI_UNSIGNED_LONG_LONG, &mpi_count);
      size_t count = (size_t)mpi_count;

      if (count == 0) {
         MPI_Recv(NULL, 0, MPI_UNSIGNED_LONG_LONG,
                  status.MPI_SOURCE, a->tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         dones++;
      } else {
         if (a->soa.count + count > a->soa.capacity) {
            while (a->soa.capacity < a->soa.count + count)
               a->soa.capacity *= 2;
            a->soa.data = g_realloc(a->soa.data,
                                    a->soa.capacity * sizeof(MedianValue));
         }
         MPI_Recv(a->soa.data + a->soa.count, mpi_count, MPI_UNSIGNED_LONG_LONG,
                  status.MPI_SOURCE, a->tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         a->soa.count += count;
      }
   }
   return NULL;
}

/* ── gather payload (fixed-size, sent with MPI_Gather) ───────────── */
typedef struct {
   NodeAgg  agg;
   MedianValue medians[N_MEDIAN_FIELDS];
   bool     has_median[N_MEDIAN_FIELDS];
} GatherPayload;

/* ── helpers ─────────────────────────────────────────────────────── */
static inline MpiRank
median_receiver(FieldIndex field_idx, size_t mpi_size)
{
   return (MpiRank)(((size_t)field_idx) % mpi_size);
}

/* ── option parsing ──────────────────────────────────────────────── */
static gboolean
parse_options(int *argc, char ***argv, Options *options, GError **error)
{
   GOptionEntry entries[] = {
      { "input",   'i', 0, G_OPTION_ARG_STRING, &options->input_path,
        "Input file to process", "FILE" },
      { "nodes",   'n', 0, G_OPTION_ARG_STRING, &options->nodes_map,
        "Node-to-cores mapping (e.g., node001:0-15,node002:0-15)", "MAP" },
      { "threads", 't', 0, G_OPTION_ARG_INT,    &options->max_threads,
        "Maximum threads per node", "NUM" },
      { NULL }
   };

   GOptionContext *context = g_option_context_new("[FILE]");
   g_option_context_set_summary(context,
      "Distribute lines from an input file across MPI ranks.");
   g_option_context_add_main_entries(context, entries, NULL);

   gboolean ok = g_option_context_parse(context, argc, argv, error);
   if (!ok) { g_option_context_free(context); return FALSE; }

   if (options->input_path == NULL) {
      if (*argc == 2) {
         options->input_path = g_strdup((*argv)[1]);
      } else {
         if (error)
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "missing input file; pass --input FILE or FILE");
         g_option_context_free(context);
         return FALSE;
      }
   } else if (*argc > 1) {
      if (error)
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "too many positional arguments; pass a single FILE or --input FILE");
      g_option_context_free(context);
      return FALSE;
   }

   g_option_context_free(context);
   return TRUE;
}

/* ── main ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
   Options options = { 0 };
   GError *parse_error = NULL;

   if (!parse_options(&argc, &argv, &options, &parse_error)) {
      if (parse_error) {
         g_printerr("%s\n", parse_error->message);
         g_error_free(parse_error);
      }
      g_free(options.input_path);
      g_free(options.nodes_map);
      return EXIT_FAILURE;
   }

   printf("========================================\n");
   printf("Parsed configuration:\n");
   printf("========================================\n");
   printf("  Input file:  %s\n", options.input_path ? options.input_path : "(not set)");
   printf("  Node map:    %s\n", options.nodes_map  ? options.nodes_map  : "(not set)");
   printf("  Max threads: %d\n", options.max_threads > 0 ? options.max_threads : 0);
   printf("========================================\n\n");

   /* comms thread and receiver threads make concurrent MPI calls */
   int provided;
   MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
   if (provided < MPI_THREAD_MULTIPLE) {
      fprintf(stderr, "MPI_THREAD_MULTIPLE not supported (got %d)\n", provided);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
   }

   int mpi_size_raw = 0;
   MpiRank rank = 0;
   MPI_Comm_size(MPI_COMM_WORLD, &mpi_size_raw);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   size_t mpi_size = (size_t)mpi_size_raw;

   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   printf("Rank %d/%zu on %s\n", rank, mpi_size, hostname);

   /* subtract 1 core for the dedicated comms thread */
   int avail_cores = options.max_threads > 0 ? options.max_threads : omp_get_max_threads();
   size_t n_threads = (size_t)(avail_cores > 1 ? avail_cores - 1 : 1);

   /* ── determine median receiver rank for each field ───────────── */
   MpiRank dest_ranks[N_MEDIAN_FIELDS];
   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
      dest_ranks[fi] = median_receiver(fi, mpi_size);

   /* ── launch receiver thread(s) for fields this rank owns ─────── */
   RecvThreadArgs recv_args[N_MEDIAN_FIELDS];
   pthread_t      recv_threads[N_MEDIAN_FIELDS];
   FieldIndex     recv_field_map[N_MEDIAN_FIELDS]; /* recv_threads[i] handles field recv_field_map[i] */
   size_t         n_recv_threads = 0;

   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++) {
      if (dest_ranks[fi] != rank) continue;
      RecvThreadArgs *a = &recv_args[n_recv_threads];
      a->tag          = MEDIAN_TAGS[fi];
      a->size         = mpi_size;
      a->soa.capacity = 1u << 20; /* 1M values initial */
      a->soa.count    = 0;
      a->soa.data     = g_new(MedianValue, a->soa.capacity);
      recv_field_map[n_recv_threads] = fi;
      pthread_create(&recv_threads[n_recv_threads], NULL, recv_thread_func, a);
      n_recv_threads++;
   }

   /* ── init comms queue and launch comms thread ────────────────── */
   CommQueue     *queue = g_new(CommQueue, 1);
   CommThreadArgs comms_args;
   pthread_t      comms_thread;

   comm_queue_init(queue);
   comms_args.queue = queue;
   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
      comms_args.dest_ranks[fi] = dest_ranks[fi];

   pthread_create(&comms_thread, NULL, comms_thread_func, &comms_args);

   /* ── open and mmap the job-index file ────────────────────────── */
   int fd = open(options.input_path, O_RDONLY);
   if (fd < 0) {
      perror("open");
      g_free(options.input_path); g_free(options.nodes_map);
      MPI_Finalize(); return EXIT_FAILURE;
   }
   struct stat st;
   if (fstat(fd, &st) != 0) {
      perror("fstat"); close(fd);
      g_free(options.input_path); g_free(options.nodes_map);
      MPI_Finalize(); return EXIT_FAILURE;
   }
   if (st.st_size == 0) {
      close(fd);
      g_free(options.input_path); g_free(options.nodes_map);
      MPI_Finalize(); return EXIT_SUCCESS;
   }

   size_t idx_size = (size_t)st.st_size;
   char *idx_data  = mmap(NULL, idx_size, PROT_READ, MAP_PRIVATE, fd, 0);
   if (idx_data == MAP_FAILED) {
      perror("mmap"); close(fd);
      g_free(options.input_path); g_free(options.nodes_map);
      MPI_Finalize(); return EXIT_FAILURE;
   }
   close(fd);
   madvise(idx_data, idx_size, MADV_SEQUENTIAL);

   /* ── distribute job paths across ranks ───────────────────────── */
   char  *job_array[MAX_JOBS];
   size_t job_count = 0;
   {
      char *idx_cursor = idx_data;
      char *idx_end = idx_data + idx_size;
      size_t line = 0;

      /* skip header */
      char *header_nl = memchr(idx_cursor, '\n', (size_t)(idx_end - idx_cursor));
      idx_cursor = header_nl ? header_nl + 1 : idx_end;

      while (idx_cursor < idx_end) {
         char *line_nl = memchr(idx_cursor, '\n', (size_t)(idx_end - idx_cursor));
         size_t len = line_nl ? (size_t)(line_nl - idx_cursor) : (size_t)(idx_end - idx_cursor);
         if (mpi_size > 0 && (line % mpi_size) == (size_t)rank) {
            if (job_count < MAX_JOBS)
               job_array[job_count++] = g_strndup(idx_cursor, len);
            else
               fprintf(stderr, "rank %d: job_array full at line %zu, skipping\n", rank, line);
         }
         idx_cursor = line_nl ? line_nl + 1 : idx_end;
         line++;
      }
   }
   munmap(idx_data, idx_size);

   /* ── OMP parallel parse + aggregate + stream median values ──── */
   NodeAgg *thread_agg = g_new0(NodeAgg, n_threads);

   #pragma omp parallel num_threads((int)n_threads)
   {
      size_t thread_count = (size_t)omp_get_num_threads();
      size_t thread_id    = (size_t)omp_get_thread_num();
      NodeAgg *my_agg       = &thread_agg[thread_id];

      /* 3 staging buffers, one per median field — heap to avoid large stack frames */
      StageBuf *stage = g_new0(StageBuf, N_MEDIAN_FIELDS);

      size_t chunk_size = (job_count + (size_t)thread_count - 1) / (size_t)thread_count;
      size_t start      = (size_t)thread_id * chunk_size;
      size_t end_job    = start + chunk_size < job_count ? start + chunk_size : job_count;

      for (size_t i = start; i < end_job; i++) {
         int job_fd = open(job_array[i], O_RDONLY);
         if (job_fd < 0) { perror("open"); g_free(job_array[i]); continue; }

         struct stat job_st;
         if (fstat(job_fd, &job_st) != 0) { perror("fstat"); close(job_fd); g_free(job_array[i]); continue; }
         if (job_st.st_size == 0)     { close(job_fd); g_free(job_array[i]); continue; }

         size_t file_size = (size_t)job_st.st_size;
         char *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, job_fd, 0);
         if (file_data == MAP_FAILED)  { perror("mmap"); close(job_fd); g_free(job_array[i]); continue; }
         close(job_fd);
         madvise(file_data, file_size, MADV_SEQUENTIAL);

         char *cursor = file_data;
         char *file_end = file_data + file_size;

         /* skip header */
         char *header_nl = memchr(cursor, '\n', (size_t)(file_end - cursor));
         cursor = header_nl ? header_nl + 1 : file_end;

         char field1_buf[CSV_FIELD1_MAX];
         MetricValue f2, f3, f4;

         while (cursor < file_end) {
            /* Advance past current line regardless of parse outcome */
            const char *line_nl = memchr(cursor, '\n', (size_t)(file_end - cursor));
            char *next = line_nl ? (char *)line_nl + 1 : file_end;

            if (sscanf(cursor, "%255[^,],%" SCN_METRIC ",%" SCN_METRIC ",%" SCN_METRIC,
                       field1_buf, &f2, &f3, &f4) == 4) {
               CsvRow row = { .field2 = f2, .field3 = f3, .field4 = f4 };
               g_strlcpy(row.field1, field1_buf, CSV_FIELD1_MAX);
               node_agg_update(my_agg, &row);

               /* stream each field value to its median receiver via comms thread */
               stage_buf_append(&stage[0], row.field2, 0, dest_ranks[0], queue);
               stage_buf_append(&stage[1], row.field3, 1, dest_ranks[1], queue);
               stage_buf_append(&stage[2], row.field4, 2, dest_ranks[2], queue);
            }
            cursor = next;
         }

         munmap(file_data, file_size);
         g_free(job_array[i]);
      }

      /* flush any partial staging buffers before this thread exits */
      for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
         stage_buf_flush(&stage[fi], fi, dest_ranks[fi], queue);
      g_free(stage);

   } /* implicit OMP barrier — all parse threads done, all items in queue */

   /* signal comms thread: no more producers */
   atomic_store_explicit(&queue->producers_done, true, memory_order_release);
   pthread_join(comms_thread, NULL); /* all MPI_Sends complete, done sentinels sent */

   /* node-level aggregation */
   for (size_t t = 1; t < n_threads; t++)
      node_agg_merge(&thread_agg[0], &thread_agg[t]);

   /* wait for receiver threads — SOAs are complete after join */
   for (size_t i = 0; i < n_recv_threads; i++)
      pthread_join(recv_threads[i], NULL);

   /* compute medians for fields this rank received */
   MedianValue my_medians[N_MEDIAN_FIELDS] = {0};
   bool has_median[N_MEDIAN_FIELDS] = {false};
   for (size_t i = 0; i < n_recv_threads; i++) {
      FieldIndex fi   = recv_field_map[i];
      my_medians[fi]  = find_median(recv_args[i].soa.data, recv_args[i].soa.count);
      has_median[fi]  = true;
      g_free(recv_args[i].soa.data);
   }

   /* build gather payload — copy agg before freeing thread_agg */
   GatherPayload my_payload = { .agg = thread_agg[0] };
   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++) {
      my_payload.medians[fi]    = my_medians[fi];
      my_payload.has_median[fi] = has_median[fi];
   }

   g_free(thread_agg);
   g_free(queue);

   /* ── gather to root ──────────────────────────────────────────── */
   /* allocate on all ranks — MPI_Gather recvbuf is implementation-defined on
      non-root when NULL; a single-element dummy avoids undefined behaviour.  */
   GatherPayload *all_payloads = g_new(GatherPayload, rank == 0 ? mpi_size : 1);
   MPI_Gather(&my_payload,  sizeof(GatherPayload), MPI_BYTE,
              all_payloads, sizeof(GatherPayload), MPI_BYTE,
              0, MPI_COMM_WORLD);

   if (rank == 0) {
      NodeAgg  final_agg             = all_payloads[0].agg;
      MedianValue medians[N_MEDIAN_FIELDS] = {0};

      for (size_t r = 0; r < mpi_size; r++) {
         if (r > 0) node_agg_merge(&final_agg, &all_payloads[r].agg);
         for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
            if (all_payloads[r].has_median[fi])
               medians[fi] = all_payloads[r].medians[fi];
      }
      g_free(all_payloads);

      const char *names[N_MEDIAN_FIELDS] = { "field2", "field3", "field4" };
      MetricValue sums[N_MEDIAN_FIELDS]  = {
         final_agg.sum_field2, final_agg.sum_field3, final_agg.sum_field4
      };
      RowHeap *tops[N_MEDIAN_FIELDS] = {
         &final_agg.top_field2, &final_agg.top_field3, &final_agg.top_field4
      };
      RowHeap *bots[N_MEDIAN_FIELDS] = {
         &final_agg.bot_field2, &final_agg.bot_field3, &final_agg.bot_field4
      };

      printf("========================================\n");
      printf("Results\n");
      printf("========================================\n");
      printf("Total lines: %zu\n\n", final_agg.total_lines);

      for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++) {
         double avg = final_agg.total_lines
            ? (double)sums[fi] / (double)final_agg.total_lines : 0.0;
         printf("%s:\n",                    names[fi]);
         printf("  Sum:    %" PRI_METRIC "\n",  sums[fi]);
         printf("  Avg:    %.4f\n",             avg);
         printf("  Median: %" PRI_METRIC "\n",  medians[fi]);
         printf("  Top %d:\n", AGG_TOP_N);
         for (size_t j = 0; j < tops[fi]->count; j++)
            printf("    key=%" PRI_METRIC " name=%s\n",
                   tops[fi]->entries[j].key, tops[fi]->entries[j].row.field1);
         printf("  Bot %d:\n", AGG_TOP_N);
         for (size_t j = 0; j < bots[fi]->count; j++)
            printf("    key=%" PRI_METRIC " name=%s\n",
                   bots[fi]->entries[j].key, bots[fi]->entries[j].row.field1);
         printf("\n");
      }
      printf("========================================\n");
   } else {
      g_free(all_payloads);
   }

   g_free(options.input_path);
   g_free(options.nodes_map);
   MPI_Finalize();
   return EXIT_SUCCESS;
}
