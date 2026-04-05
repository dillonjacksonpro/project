#include "logging.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

/* Default to 10% sampled logs; override with MPI_ORCH_LOG_SAMPLE_PCT=0..100. */
static unsigned g_sample_per_mille = 100u;
static pthread_once_t g_log_cfg_once = PTHREAD_ONCE_INIT;
static _Atomic uint64_t g_log_counter = 0;

static uint64_t
mix64(uint64_t x)
{
   x ^= x >> 30;
   x *= UINT64_C(0xbf58476d1ce4e5b9);
   x ^= x >> 27;
   x *= UINT64_C(0x94d049bb133111eb);
   x ^= x >> 31;
   return x;
}

static void
orch_log_init_cfg(void)
{
   const char *pct_env = getenv("MPI_ORCH_LOG_SAMPLE_PCT");
   if (pct_env == NULL || pct_env[0] == '\0')
      return;

   char *end = NULL;
   long pct = strtol(pct_env, &end, 10);
   if (end == pct_env || *end != '\0')
      return;
   if (pct < 0)
      pct = 0;
   if (pct > 100)
      pct = 100;
   g_sample_per_mille = (unsigned)pct * 10u;
}

static bool
orch_log_should_emit_sampled(void)
{
   pthread_once(&g_log_cfg_once, orch_log_init_cfg);

   if (g_sample_per_mille >= 1000u)
      return true;
   if (g_sample_per_mille == 0u)
      return false;

   uint64_t seq = atomic_fetch_add_explicit(&g_log_counter, 1u, memory_order_relaxed);
   uint64_t rnd = mix64(seq + UINT64_C(0x9e3779b97f4a7c15));
   return (rnd % 1000u) < g_sample_per_mille;
}

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
   if (!orch_log_should_emit_sampled())
      return;

   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, 0u, false, scope, fmt, ap);
   va_end(ap);
}

void
orch_log_emit_thread(MpiRank rank, size_t thread_id, const char *scope, const char *fmt, ...)
{
   if (!orch_log_should_emit_sampled())
      return;

   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, thread_id, true, scope, fmt, ap);
   va_end(ap);
}

void
orch_log_emit_always(MpiRank rank, const char *scope, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, 0u, false, scope, fmt, ap);
   va_end(ap);
}

void
orch_log_emit_thread_always(MpiRank rank, size_t thread_id, const char *scope, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   orch_log_vemit(rank, thread_id, true, scope, fmt, ap);
   va_end(ap);
}
