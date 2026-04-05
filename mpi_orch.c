#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
/* #include <glib.h> */
#include "glib_compat.h"
#include "aggregation.h"
#include "comm_queue.h"
#include "csv_parse.h"
#include "fatal.h"
#include "file_discovery.h"
#include "median.h"
#include "mpi_workers.h"
#include "options.h"
#include "orch_common.h"
#include <limits.h>
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

/* ── helpers ─────────────────────────────────────────────────────── */
static inline MpiRank
median_receiver(FieldIndex field_idx, size_t mpi_size)
{
   return (MpiRank)(((size_t)field_idx) % mpi_size);
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
      options_free(&options);
      return EXIT_FAILURE;
   }

   printf("========================================\n");
   printf("Parsed configuration:\n");
   printf("========================================\n");
   printf("  Input path:  %s\n", options.input_path ? options.input_path : "(not set)");
   printf("  Node map:    %s\n", options.nodes_map  ? options.nodes_map  : "(not set)");
   printf("  Max threads: %d\n", options.max_threads > 0 ? options.max_threads : 0);
   printf("========================================\n\n");

   /* comms thread and receiver threads make concurrent MPI calls */
   int provided;
   int init_rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
   if (init_rc != MPI_SUCCESS)
      fatal_no_mpi("MPI_Init_thread failed");
   if (provided < MPI_THREAD_MULTIPLE) {
      fprintf(stderr, "MPI_THREAD_MULTIPLE not supported (got %d)\n", provided);
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
   }

   int mpi_size_raw = 0;
   MpiRank rank = 0;
   {
      int rc = MPI_Comm_size(MPI_COMM_WORLD, &mpi_size_raw);
      if (rc != MPI_SUCCESS)
         fatal_no_mpi("MPI_Comm_size failed");
      rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if (rc != MPI_SUCCESS)
         fatal_no_mpi("MPI_Comm_rank failed");
   }
   if (mpi_size_raw <= 0)
      fatal_no_mpi("invalid MPI world size");
   size_t mpi_size = (size_t)mpi_size_raw;

   char hostname[256];
   if (gethostname(hostname, sizeof(hostname)) != 0)
      g_strlcpy(hostname, "unknown-host", sizeof(hostname));
   hostname[sizeof(hostname) - 1] = '\0';
   printf("Rank %d/%zu on %s\n", rank, mpi_size, hostname);

   /* subtract 1 core for the dedicated comms thread */
   int avail_cores = options.max_threads > 0 ? options.max_threads : omp_get_max_threads();
   size_t n_threads = (size_t)(avail_cores > 1 ? avail_cores - 1 : 1);
   if (n_threads > (size_t)INT_MAX)
      fatal_rank(rank, "thread count too large for OpenMP runtime");

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
      a->source_rank  = rank;
      a->soa.capacity = 1u << 20; /* 1M values initial */
      a->soa.count    = 0;
      a->soa.data     = g_new(MedianValue, a->soa.capacity);
      if (a->soa.data == NULL)
         fatal_rank(rank, "out of memory allocating receiver buffer");
      recv_field_map[n_recv_threads] = fi;
      if (pthread_create(&recv_threads[n_recv_threads], NULL, recv_thread_func, a) != 0)
         fatal_rank_errno(rank, "pthread_create(receiver)");
      n_recv_threads++;
   }

   /* ── init comms queue and launch comms thread ────────────────── */
   CommQueue     *queue = g_new(CommQueue, 1);
   CommThreadArgs comms_args;
   pthread_t      comms_thread;

   if (queue == NULL)
      fatal_rank(rank, "out of memory allocating comm queue");

   comm_queue_init(queue);
   comms_args.queue = queue;
   for (FieldIndex fi = 0; fi < N_MEDIAN_FIELDS; fi++)
      comms_args.dest_ranks[fi] = dest_ranks[fi];

   if (pthread_create(&comms_thread, NULL, comms_thread_func, &comms_args) != 0)
      fatal_rank_errno(rank, "pthread_create(comms)");

   /* ── scan input directory and stripe paths across ranks ──────── */
   char  *job_array[MAX_JOBS];
   size_t job_count = 0;

   if (!discover_rank_jobs(options.input_path, rank, mpi_size, job_array, &job_count)) {
      options_free(&options);
      (void)MPI_Finalize();
      return EXIT_FAILURE;
   }

   /* ── OMP parallel parse + aggregate + stream median values ──── */
   NodeAgg *thread_agg = g_new0(NodeAgg, n_threads);
   if (thread_agg == NULL)
      fatal_rank(rank, "out of memory allocating thread aggregators");

   #pragma omp parallel num_threads((int)n_threads)
   {
      size_t thread_count = (size_t)omp_get_num_threads();
      size_t thread_id    = (size_t)omp_get_thread_num();
      NodeAgg *my_agg       = &thread_agg[thread_id];

      /* 3 staging buffers, one per median field — heap to avoid large stack frames */
      StageBuf *stage = g_new0(StageBuf, N_MEDIAN_FIELDS);
      if (stage == NULL)
         fatal_rank(rank, "out of memory allocating stage buffers");

      size_t chunk_size = (job_count + (size_t)thread_count - 1) / (size_t)thread_count;
      size_t start      = (size_t)thread_id * chunk_size;
      size_t end_job    = start + chunk_size < job_count ? start + chunk_size : job_count;

      for (size_t i = start; i < end_job; i++) {
         int job_fd = open(job_array[i], O_RDONLY);
         if (job_fd < 0) { perror("open"); g_free(job_array[i]); continue; }

         struct stat job_st;
         if (fstat(job_fd, &job_st) != 0) { perror("fstat"); close(job_fd); g_free(job_array[i]); continue; }
         if (job_st.st_size <= 0)     { close(job_fd); g_free(job_array[i]); continue; }
         if ((uintmax_t)job_st.st_size > (uintmax_t)SIZE_MAX) {
            fprintf(stderr, "rank %d: file too large to map: %s\n", rank, job_array[i]);
            close(job_fd);
            g_free(job_array[i]);
            continue;
         }

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

         while (cursor < file_end) {
            /* Advance past current line regardless of parse outcome */
            const char *line_nl = memchr(cursor, '\n', (size_t)(file_end - cursor));
            char *next = line_nl ? (char *)line_nl + 1 : file_end;
            const char *line_end = line_nl ? line_nl : file_end;

            /* Handle CRLF line endings without copying line buffers. */
            if (line_end > cursor && line_end[-1] == '\r')
               line_end--;

            CsvRow row;
            size_t line_len = (size_t)(line_end - cursor);

            if (parse_csv_row_line(cursor, line_len, &row)) {
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
   if (pthread_join(comms_thread, NULL) != 0)
      fatal_rank_errno(rank, "pthread_join(comms)"); /* all MPI_Sends complete */

   /* node-level aggregation */
   for (size_t t = 1; t < n_threads; t++)
      node_agg_merge(&thread_agg[0], &thread_agg[t]);

   /* wait for receiver threads — SOAs are complete after join */
   for (size_t i = 0; i < n_recv_threads; i++)
      if (pthread_join(recv_threads[i], NULL) != 0)
         fatal_rank_errno(rank, "pthread_join(receiver)");

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
   if (all_payloads == NULL)
      fatal_rank(rank, "out of memory allocating gather payload buffer");
   {
      int rc = MPI_Gather(&my_payload,  sizeof(GatherPayload), MPI_BYTE,
                          all_payloads, sizeof(GatherPayload), MPI_BYTE,
                          0, MPI_COMM_WORLD);
      if (rc != MPI_SUCCESS)
         fatal_rank_mpi(rank, rc, "MPI_Gather");
   }

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

   options_free(&options);
   {
      int rc = MPI_Finalize();
      if (rc != MPI_SUCCESS)
         fatal_no_mpi("MPI_Finalize failed");
   }
   return EXIT_SUCCESS;
}
