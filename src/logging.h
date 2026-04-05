#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>

#include "orch_common.h"

#if defined(__GNUC__) || defined(__clang__)
#define ORCH_PRINTF_ATTR(pos_fmt, pos_args) __attribute__((format(printf, pos_fmt, pos_args)))
#else
#define ORCH_PRINTF_ATTR(pos_fmt, pos_args)
#endif


void orch_log_emit(MpiRank rank, const char *scope, const char *fmt, ...) ORCH_PRINTF_ATTR(3, 4);
void orch_log_emit_thread(MpiRank rank, size_t thread_id, const char *scope, const char *fmt, ...) ORCH_PRINTF_ATTR(4, 5);

#if MPI_ORCH_LOGGING
#define ORCH_LOG(rank, scope, ...) orch_log_emit((rank), (scope), __VA_ARGS__)
#define ORCH_LOG_THREAD(rank, thread_id, scope, ...) \
   orch_log_emit_thread((rank), (thread_id), (scope), __VA_ARGS__)
#else
#define ORCH_LOG(...) ((void)0)
#define ORCH_LOG_THREAD(...) ((void)0)
#endif

#endif