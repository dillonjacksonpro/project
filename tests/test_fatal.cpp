#include <gtest/gtest.h>

extern "C" {
#include "fatal.h"
}

TEST(FatalTest, FatalNoMpiExits)
{
   ASSERT_DEATH({ fatal_no_mpi("boom"); }, "fatal: boom");
}
