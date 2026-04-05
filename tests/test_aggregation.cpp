#include <gtest/gtest.h>

extern "C" {
#include "aggregation.h"
}

TEST(AggregationTest, UpdatesCountersAndSums)
{
   NodeAgg agg = {0};
   CsvRow row = {0};
   row.field2 = 11;
   row.field3 = 22;
   row.field4 = 33;

   node_agg_update(&agg, &row);

   EXPECT_EQ(agg.total_lines, 1u);
   EXPECT_EQ(agg.sum_field2, 11u);
   EXPECT_EQ(agg.sum_field3, 22u);
   EXPECT_EQ(agg.sum_field4, 33u);
}

TEST(AggregationTest, MergeAddsTotals)
{
   NodeAgg a = {0};
   NodeAgg b = {0};
   CsvRow r1 = {0};
   CsvRow r2 = {0};
   r1.field2 = 10; r1.field3 = 20; r1.field4 = 30;
   r2.field2 = 1;  r2.field3 = 2;  r2.field4 = 3;

   node_agg_update(&a, &r1);
   node_agg_update(&b, &r2);
   node_agg_merge(&a, &b);

   EXPECT_EQ(a.total_lines, 2u);
   EXPECT_EQ(a.sum_field2, 11u);
   EXPECT_EQ(a.sum_field3, 22u);
   EXPECT_EQ(a.sum_field4, 33u);
}
