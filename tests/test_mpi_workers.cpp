#include <gtest/gtest.h>

extern "C" {
#include "mpi_workers.h"
}

TEST(MpiWorkersTest, StructShapesAreUsable)
{
   CommThreadArgs comm = {0};
   RecvThreadArgs recv = {0};

   EXPECT_EQ(comm.queue, nullptr);
   EXPECT_EQ(recv.soa.count, 0u);
   EXPECT_EQ(recv.soa.capacity, 0u);
}
