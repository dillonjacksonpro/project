#include "file_discovery.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "csv_parse.h"
#include "glib_compat.h"

bool
discover_rank_jobs(const char *input_path,
                   MpiRank rank,
                   size_t mpi_size,
                   char *job_array[MAX_JOBS],
                   size_t *job_count_out)
{
   if (input_path == NULL || job_array == NULL || job_count_out == NULL || mpi_size == 0)
      return false;

   *job_count_out = 0;

   DIR *dir = opendir(input_path);
   if (dir == NULL) {
      perror("opendir");
      return false;
   }

   char **all_jobs = NULL;
   size_t all_count = 0;
   size_t all_capacity = 0;
   bool fatal_error = false;

   struct dirent *ent;
   while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.')
         continue;

      size_t base_len = strlen(input_path);
      size_t name_len = strlen(ent->d_name);
      if (base_len > SIZE_MAX - name_len - 2u) {
         fprintf(stderr, "rank %d: path length overflow for entry %s\n", rank, ent->d_name);
         continue;
      }
      bool needs_sep = base_len > 0 && input_path[base_len - 1] != '/';
      size_t full_len = base_len + (needs_sep ? 1u : 0u) + name_len;

      char *full_path = g_new(char, full_len + 1);
      if (full_path == NULL) {
         fprintf(stderr, "rank %d: out of memory building file paths\n", rank);
         continue;
      }

      memcpy(full_path, input_path, base_len);
      if (needs_sep)
         full_path[base_len] = '/';
      memcpy(full_path + base_len + (needs_sep ? 1u : 0u), ent->d_name, name_len + 1);

      struct stat ent_st;
      if (stat(full_path, &ent_st) != 0) {
         perror("stat");
         g_free(full_path);
         continue;
      }
      if (!S_ISREG(ent_st.st_mode)) {
         g_free(full_path);
         continue;
      }

      if (all_count == all_capacity) {
         size_t next_capacity = all_capacity ? all_capacity * 2u : 256u;
         if (next_capacity < all_capacity || next_capacity > SIZE_MAX / sizeof(*all_jobs)) {
            fprintf(stderr, "rank %d: file list capacity overflow\n", rank);
            g_free(full_path);
            fatal_error = true;
            break;
         }
         char **grown = g_realloc(all_jobs, next_capacity * sizeof(*all_jobs));
         if (grown == NULL) {
            fprintf(stderr, "rank %d: out of memory expanding file list\n", rank);
            g_free(full_path);
            fatal_error = true;
            break;
         }
         all_jobs = grown;
         all_capacity = next_capacity;
      }
      all_jobs[all_count++] = full_path;
   }
   closedir(dir);

   if (fatal_error) {
      for (size_t i = 0; i < all_count; i++)
         g_free(all_jobs[i]);
      g_free(all_jobs);
      return false;
   }

   if (all_count > 1)
      qsort(all_jobs, all_count, sizeof(*all_jobs), csv_path_cmp);

   for (size_t i = 0; i < all_count; i++) {
      if ((i % mpi_size) == (size_t)rank) {
         if (*job_count_out < MAX_JOBS) {
            job_array[*job_count_out] = all_jobs[i];
            (*job_count_out)++;
         } else {
            fprintf(stderr, "rank %d: job_array full at index %zu, skipping %s\n", rank, i, all_jobs[i]);
            g_free(all_jobs[i]);
         }
      } else {
         g_free(all_jobs[i]);
      }
   }

   g_free(all_jobs);
   return true;
}
