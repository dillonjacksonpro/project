#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

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
   comm_queue_destroy(&q);
}

TEST(CommQueueTest, EmptyQueueReturnsNull)
{
   CommQueue q;
   comm_queue_init(&q);

   EXPECT_EQ(comm_queue_pop(&q), nullptr);
   comm_queue_destroy(&q);
}

TEST(CommQueueTest, PopWaitReturnsNullAfterDone)
{
   CommQueue q;
   comm_queue_init(&q);

   std::atomic<bool> started{false};
   std::atomic<bool> finished{false};
   std::thread waiter([&]() {
      started.store(true, std::memory_order_release);
      CommNode *out = comm_queue_pop_wait(&q);
      EXPECT_EQ(out, nullptr);
      finished.store(true, std::memory_order_release);
   });

   while (!started.load(std::memory_order_acquire)) {
      std::this_thread::yield();
   }

   comm_queue_mark_done(&q);
   waiter.join();
   EXPECT_TRUE(finished.load(std::memory_order_acquire));
   comm_queue_destroy(&q);
}

TEST(CommQueueTest, ProducerBlocksWhenQueueIsFull)
{
   CommQueue q;
   comm_queue_init(&q);
   q.max_depth = 1;

   CommNode node1;
   CommNode node2;
   comm_queue_push(&q, &node1);

   std::atomic<bool> second_push_done{false};
   std::thread producer([&]() {
      comm_queue_push(&q, &node2);
      second_push_done.store(true, std::memory_order_release);
   });

   std::this_thread::sleep_for(std::chrono::milliseconds(20));
   EXPECT_FALSE(second_push_done.load(std::memory_order_acquire));

   CommNode *first = comm_queue_pop(&q);
   EXPECT_EQ(first, &node1);

   producer.join();
   EXPECT_TRUE(second_push_done.load(std::memory_order_acquire));

   CommNode *second = comm_queue_pop(&q);
   EXPECT_EQ(second, &node2);
   EXPECT_EQ(comm_queue_pop(&q), nullptr);
   comm_queue_mark_done(&q);
   comm_queue_destroy(&q);
}
