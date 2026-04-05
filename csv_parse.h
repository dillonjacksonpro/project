#ifndef CSV_PARSE_H
#define CSV_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include "orch_common.h"

int csv_path_cmp(const void *a, const void *b);
bool parse_metric_token(const char *start, const char *end, MetricValue *out);
bool parse_csv_row_line(const char *line, size_t len, CsvRow *row);

#endif
