#include "median.h"

#include <string.h>

#include "glib_compat.h"

static void
u64_swap(MedianValue *a, MedianValue *b)
{
   MedianValue t = *a;
   *a = *b;
   *b = t;
}

static MedianValue
u64_quickselect(MedianValue *arr, size_t n, size_t k)
{
   size_t lo = 0;
   size_t hi = n - 1;
   while (lo < hi) {
      size_t mid = lo + (hi - lo) / 2;
      u64_swap(&arr[mid], &arr[hi]);
      MedianValue pivot = arr[hi];
      size_t store = lo;
      for (size_t i = lo; i < hi; i++)
         if (arr[i] <= pivot) u64_swap(&arr[store++], &arr[i]);
      u64_swap(&arr[store], &arr[hi]);
      if (store == k)
         break;
      if (k < store)
         hi = store - 1;
      else
         lo = store + 1;
   }
   return arr[k];
}

MedianValue
find_median(const MedianValue *src, size_t n)
{
   if (n == 0)
      return 0;
   if (n == 1)
      return src[0];

   MedianValue *arr = g_new(MedianValue, n);
   if (arr == NULL)
      return 0;
   memcpy(arr, src, n * sizeof(MedianValue));

   MedianValue med;
   if ((n & 1u) != 0u) {
      med = u64_quickselect(arr, n, n / 2);
   } else {
      u64_quickselect(arr, n, n / 2 - 1);
      MedianValue lo_val = arr[n / 2 - 1];
      MedianValue hi_val = arr[n / 2];
      for (size_t i = n / 2 + 1; i < n; i++)
         if (arr[i] < hi_val) hi_val = arr[i];
      med = lo_val / 2 + hi_val / 2 + ((lo_val & 1u) + (hi_val & 1u)) / 2;
   }

   g_free(arr);
   return med;
}
