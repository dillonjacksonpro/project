#include "logging.h"

#include <stdio.h>

static void
orch_log_vemit(MpiRank rank, size_t thread_id, bool include_thread,
               const char *scope, const char *fmt, va_list ap)
{
   flockfile(stderr);
   fprintf(stderr, "rank %d", rank);
   if (include_thread)
      fprintf(stderr, " thread %zu", thread_id);
   if (scope != NULL && scope[0] != '\0')
      fprintf(stderr, " [%s]", scope);
   fputs(": ", stderr);
   vfprintf(stderr, fmt, ap);
   fputc('\n', stderr);
   funlockfile(stderr);
}

void
orch_log_emit(MpiRank rank, const char *scope, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, 0u, false, scope, fmt, ap);
   va_end(ap);
}

void
orch_log_emit_thread(MpiRank rank, size_t thread_id, const char *scope, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, thread_id, true, scope, fmt, ap);
   va_end(ap);
}
