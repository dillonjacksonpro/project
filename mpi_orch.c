#include <fcntl.h>
#include <glib.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_JOBS 4086

typedef struct {
   char *input_path;
   char *nodes_map;
   int max_threads;
} Options;

static gboolean
parse_options(int *argc, char ***argv, Options *options, GError **error)
{
   GOptionEntry entries[] = {
      { "input", 'i', 0, G_OPTION_ARG_STRING, &options->input_path,
        "Input file to process", "FILE" },
      { "nodes", 'n', 0, G_OPTION_ARG_STRING, &options->nodes_map,
        "Node-to-cores mapping (e.g., node001:0-15,node002:0-15)", "MAP" },
      { "threads", 't', 0, G_OPTION_ARG_INT, &options->max_threads,
        "Maximum threads per node", "NUM" },
      { NULL }
   };

   GOptionContext *context = g_option_context_new("[FILE]");
   g_option_context_set_summary(
      context,
      "Distribute lines from an input file across MPI ranks.");
   g_option_context_add_main_entries(context, entries, NULL);

   gboolean ok = g_option_context_parse(context, argc, argv, error);
   if (!ok) {
      g_option_context_free(context);
      return FALSE;
   }

   if (options->input_path == NULL) {
      if (*argc == 2) {
         options->input_path = g_strdup((*argv)[1]);
      } else {
         if (error != NULL) {
            g_set_error(error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        "missing input file; pass --input FILE or FILE");
         }
         g_option_context_free(context);
         return FALSE;
      }
   } else if (*argc > 1) {
      if (error != NULL) {
         g_set_error(error,
                     G_OPTION_ERROR,
                     G_OPTION_ERROR_BAD_VALUE,
                     "too many positional arguments; pass a single FILE or --input FILE");
      }
      g_option_context_free(context);
      return FALSE;
   }

   g_option_context_free(context);
   return TRUE;
}

int main(int argc, char *argv[]) {
   Options options = { 0 };
   GError *parse_error = NULL;

   if (!parse_options(&argc, &argv, &options, &parse_error)) {
      if (parse_error != NULL) {
         g_printerr("%s\n", parse_error->message);
         g_error_free(parse_error);
      }
      g_free(options.input_path);
      g_free(options.nodes_map);
      return EXIT_FAILURE;
   }

   // Print parsed options
   printf("========================================\n");
   printf("Parsed configuration:\n");
   printf("========================================\n");
   printf("  Input file:      %s\n", options.input_path ? options.input_path : "(not set)");
   printf("  Node map:        %s\n", options.nodes_map ? options.nodes_map : "(not set)");
   printf("  Max threads:     %d\n", options.max_threads > 0 ? options.max_threads : 0);
   printf("========================================\n\n");

   MPI_Init(&argc, &argv);

   int size = 0;
   int rank = 0;
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   // get host name and print it
   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   printf("Rank %d of %d running on %s\n", rank, size, hostname);

   // get mpi hostname and print it
   char mpi_hostname[MPI_MAX_PROCESSOR_NAME];
   int mpi_hostname_len = 0;
   MPI_Get_processor_name(mpi_hostname, &mpi_hostname_len);
   printf("Rank %d of %d MPI hostname: %s\n", rank, size, mpi_hostname);

   int fd = open(options.input_path, O_RDONLY);
   if (fd < 0) {
      perror("open");
      g_free(options.input_path);
      g_free(options.nodes_map);
      MPI_Finalize();
      return EXIT_FAILURE;
   }

   struct stat st;
   if (fstat(fd, &st) != 0) {
      perror("fstat");
      close(fd);
      g_free(options.input_path);
      g_free(options.nodes_map);
      MPI_Finalize();
      return EXIT_FAILURE;
   }

   if (st.st_size == 0) {
      close(fd);
      g_free(options.input_path);
      g_free(options.nodes_map);
      MPI_Finalize();
      return EXIT_SUCCESS;
   }

   size_t file_size = (size_t)st.st_size;
   char *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
   if (data == MAP_FAILED) {
      perror("mmap");
      close(fd);
      g_free(options.input_path);
      g_free(options.nodes_map);
      MPI_Finalize();
      return EXIT_FAILURE;
   }

   close(fd);
   madvise(data, file_size, MADV_SEQUENTIAL);

   char *p = data;
   char *end = data + file_size;
   int line = 0;

   char * job_array[MAX_JOBS];
   size_t job_count = 0;

   // Distribute the mpi jobs across the ranks
   while (p < end) {
      char *nl = memchr(p, '\n', (size_t)(end - p));
      size_t len = nl ? (size_t)(nl - p) : (size_t)(end - p);

      if (size > 0 && (line % size) == rank) {
         job_array[job_count++] = g_strndup(p, len);
      }

      p = nl ? nl + 1 : end;
      line++;
   }

   munmap(data, file_size);

   // openmp processing of list here
   // use max threads from options for omp parallelism
   #pragma omp parallel for num_threads(options.max_threads > 0 ? options.max_threads : 1)
   for(size_t i = 0; i < job_count; i++) {
      // Process job_array[i]
      printf("Rank %d processing job: %s\n", rank, job_array[i]);
      // Simulate work
      usleep(100000);
      g_free(job_array[i]);
   }

   // barrier to ensure all ranks have completed their jobs before gathering results
   MPI_Barrier(MPI_COMM_WORLD);

   // shuffle 


   // gather results and print

   g_free(options.input_path);
   g_free(options.nodes_map);


   MPI_Finalize();

   // output results here
   return EXIT_SUCCESS;
}
