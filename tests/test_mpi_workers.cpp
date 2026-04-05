#include <gtest/gtest.h>

#include "mpi_types.h"

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

TEST(MpiWorkersTest, MedianMpiTypeMacroIsUsable)
{
   MPI_Datatype dtype = MEDIAN_MPI_TYPE;
   EXPECT_NE(dtype, MPI_DATATYPE_NULL);
}

TEST(MpiWorkersTest, ReceiverLimitCoversInitialCapacity)
{
   RecvThreadArgs recv = {0};
   recv.soa.capacity = 1u << 20;

   EXPECT_GE(MAX_RECEIVER_VALUES, recv.soa.capacity);
}
