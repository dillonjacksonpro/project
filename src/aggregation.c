#include "aggregation.h"

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

void
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

void
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
