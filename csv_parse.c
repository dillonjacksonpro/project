#include "csv_parse.h"

#include <string.h>

#include "glib_compat.h"

int
csv_path_cmp(const void *a, const void *b)
{
   const char *const *sa = (const char *const *)a;
   const char *const *sb = (const char *const *)b;
   return strcmp(*sa, *sb);
}

bool
parse_metric_token(const char *start, const char *end, MetricValue *out)
{
   if (start == NULL || end == NULL || out == NULL || start >= end)
      return false;

   MetricValue value = 0;
   for (const char *p = start; p < end; p++) {
      if (*p < '0' || *p > '9')
         return false;
      MetricValue digit = (MetricValue)(*p - '0');
      if (value > (UINT64_MAX - digit) / 10)
         return false;
      value = value * 10 + digit;
   }

   *out = value;
   return true;
}

bool
parse_csv_row_line(const char *line, size_t len, CsvRow *row)
{
   if (line == NULL || row == NULL || len == 0)
      return false;

   const char *end = line + len;
   const char *c1 = memchr(line, ',', len);
   if (c1 == NULL)
      return false;

   size_t rem_after_c1 = (size_t)(end - (c1 + 1));
   const char *c2 = memchr(c1 + 1, ',', rem_after_c1);
   if (c2 == NULL)
      return false;

   size_t rem_after_c2 = (size_t)(end - (c2 + 1));
   const char *c3 = memchr(c2 + 1, ',', rem_after_c2);
   if (c3 == NULL)
      return false;

   size_t field1_len = (size_t)(c1 - line);
   if (field1_len == 0 || field1_len >= CSV_FIELD1_MAX)
      return false;

   g_strlcpy(row->field1, line, field1_len + 1);
   return parse_metric_token(c1 + 1, c2, &row->field2) &&
          parse_metric_token(c2 + 1, c3, &row->field3) &&
          parse_metric_token(c3 + 1, end, &row->field4);
}
