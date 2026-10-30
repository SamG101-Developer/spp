#include "test_macros.hpp"

// gtest brings in <stdlib.h>, whose macros would rewrite the "sys::" names below.
#undef WEXITSTATUS
#undef WIFEXITED

import std;
import sys;

// A status from "std::system" read the same way on every platform. Both shells take "exit N".

TEST(TestSys, test_a_clean_exit_decodes_to_zero) {
  const auto status = std::system("exit 0");
  EXPECT_TRUE(sys::WIFEXITED(status));
  EXPECT_EQ(sys::WEXITSTATUS(status), 0);
}

TEST(TestSys, test_an_exit_code_decodes_to_itself) {
  const auto status = std::system("exit 7");
  EXPECT_TRUE(sys::WIFEXITED(status));
  EXPECT_EQ(sys::WEXITSTATUS(status), 7);
}

TEST(TestSys, test_an_exit_code_with_clear_low_bits_is_not_a_pass) {
  // 128 has its low seven bits clear, which reads as "exited with 0" if a
  // Windows exit code is decoded as a POSIX wait status.
  const auto status = std::system("exit 128");
  EXPECT_TRUE(sys::WIFEXITED(status));
  EXPECT_EQ(sys::WEXITSTATUS(status), 128);
}
