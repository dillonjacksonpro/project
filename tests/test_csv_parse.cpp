#include <gtest/gtest.h>

extern "C" {
#include "csv_parse.h"
}

TEST(CsvParseTest, ParsesValidLine)
{
   CsvRow row = {0};
   const char *line = "alice,10,20,30";
   ASSERT_TRUE(parse_csv_row_line(line, 14, &row));
   EXPECT_STREQ(row.field1, "alice");
   EXPECT_EQ(row.field2, 10u);
   EXPECT_EQ(row.field3, 20u);
   EXPECT_EQ(row.field4, 30u);
}

TEST(CsvParseTest, RejectsMalformedLine)
{
   CsvRow row = {0};
   const char *line = "alice,10,20";
   EXPECT_FALSE(parse_csv_row_line(line, 11, &row));
}

TEST(CsvParseTest, RejectsNonNumericMetric)
{
   CsvRow row = {0};
   const char *line = "alice,10,xx,30";
   EXPECT_FALSE(parse_csv_row_line(line, 14, &row));
}
