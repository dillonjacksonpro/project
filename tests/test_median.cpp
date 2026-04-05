#include <gtest/gtest.h>

extern "C" {
#include "median.h"
}

TEST(MedianTest, HandlesOddLength)
{
   MedianValue values[] = {9, 1, 5};
   EXPECT_EQ(find_median(values, 3), 5u);
}

TEST(MedianTest, HandlesEvenLength)
{
   MedianValue values[] = {10, 2, 8, 4};
   EXPECT_EQ(find_median(values, 4), 6u);
}

TEST(MedianTest, HandlesEmptyInput)
{
   EXPECT_EQ(find_median(nullptr, 0), 0u);
}
