#include <gtest/gtest.h>

extern "C" {
#include "options.h"
}

TEST(OptionsTest, AcceptsPositionalDirectory)
{
   int argc = 2;
   char arg0[] = "prog";
   char arg1[] = "test_data";
   char *argv[] = {arg0, arg1, nullptr};
   char **argvp = argv;

   Options opts = {0};
   GError *err = nullptr;

   ASSERT_TRUE(parse_options(&argc, &argvp, &opts, &err));
   EXPECT_STREQ(opts.input_path, "test_data");
   EXPECT_EQ(opts.max_threads, 0);
   EXPECT_EQ(err, nullptr);

   options_free(&opts);
}

TEST(OptionsTest, RejectsMissingInput)
{
   int argc = 1;
   char arg0[] = "prog";
   char *argv[] = {arg0, nullptr};
   char **argvp = argv;

   Options opts = {0};
   GError *err = nullptr;

   EXPECT_FALSE(parse_options(&argc, &argvp, &opts, &err));
   ASSERT_NE(err, nullptr);
   EXPECT_NE(err->message, nullptr);

   g_error_free(err);
   options_free(&opts);
}
