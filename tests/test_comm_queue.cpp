#include <gtest/gtest.h>

extern "C" {
#include "comm_queue.h"
}

TEST(CommQueueTest, PushPopSingleNode)
{
   CommQueue q;
   comm_queue_init(&q);

   CommNode node;

   comm_queue_push(&q, &node);
   CommNode *out = comm_queue_pop(&q);

   EXPECT_EQ(out, &node);
}

TEST(CommQueueTest, EmptyQueueReturnsNull)
{
   CommQueue q;
   comm_queue_init(&q);

   EXPECT_EQ(comm_queue_pop(&q), nullptr);
}
