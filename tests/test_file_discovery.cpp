#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unistd.h>

extern "C" {
#include "file_discovery.h"
#include "glib_compat.h"
}

TEST(FileDiscoveryTest, StripesSortedFilesByRank)
{
   char template_dir[] = "/tmp/mpi_orch_test_XXXXXX";
   char *dir = mkdtemp(template_dir);
   ASSERT_NE(dir, nullptr);

   std::ofstream(std::string(dir) + "/b.csv") << "h\n1,2,3,4\n";
   std::ofstream(std::string(dir) + "/a.csv") << "h\n1,2,3,4\n";
   std::ofstream(std::string(dir) + "/c.csv") << "h\n1,2,3,4\n";

   char *job_array[MAX_JOBS] = {0};
   size_t job_count = 0;

   ASSERT_TRUE(discover_rank_jobs(dir, 1, 2, job_array, &job_count));
   ASSERT_EQ(job_count, 1u);
   EXPECT_NE(strstr(job_array[0], "/b.csv"), nullptr);

   for (size_t i = 0; i < job_count; i++)
      g_free(job_array[i]);

   std::remove((std::string(dir) + "/a.csv").c_str());
   std::remove((std::string(dir) + "/b.csv").c_str());
   std::remove((std::string(dir) + "/c.csv").c_str());
   rmdir(dir);
}
