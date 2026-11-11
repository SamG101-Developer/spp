#include "test_macros.hpp"

import spp.utils.env;
import spp.utils.types;
import magic_enum;
import std;

// The environment check runs over given entries, so these never touch this process's environment.

namespace {
  auto ErrorsOf(spp::Vec<spp::StrView> const &entries) -> spp::Vec<spp::Str> {
    auto errors = spp::Vec<spp::Str>();
    const auto ok = spp::utils::env::CheckEntries(entries, errors);
    EXPECT_EQ(ok, errors.empty());
    return errors;
  }
}

TEST(TestEnv, test_every_variable_is_in_the_schema) {
  for (auto const &[key, name] : magic_enum::enum_entries<spp::utils::env::Var>()) {
    const auto found = std::count_if(
      spp::utils::env::Schema().begin(), spp::utils::env::Schema().end(),
      [key](spp::utils::env::VarSpec const &spec) { return spec.Key == key; });
    EXPECT_EQ(found, 1) << name;
  }
}

TEST(TestEnv, test_other_names_are_ignored) {
  EXPECT_TRUE(ErrorsOf({"PATH=/bin", "HOME=/root", "SPPX=1", "NOT_SPP_THING=1", "=C:=C:\\"}).empty());
}

TEST(TestEnv, test_ci_names_are_ignored) {
  EXPECT_TRUE(ErrorsOf({"SPP_CI_LOCAL_PREFIX=/x", "SPP_CI_ANYTHING_NEW=1"}).empty());
}

TEST(TestEnv, test_an_unknown_name_is_an_error_listing_the_known_ones) {
  const auto errors = ErrorsOf({"SPP_MEMORY_LIMIT=0"});
  ASSERT_EQ(errors.size(), 2u);
  EXPECT_NE(errors[0].find("'SPP_MEMORY_LIMIT'"), spp::Str::npos);
  EXPECT_NE(errors[1].find("SPP_MEMORY_LIMIT_KB"), spp::Str::npos);
}

TEST(TestEnv, test_a_name_with_no_value_is_still_checked) {
  EXPECT_EQ(ErrorsOf({"SPP_UNKNOWN"}).size(), 2u);
}

TEST(TestEnv, test_good_values_are_accepted) {
  EXPECT_TRUE(ErrorsOf({
    "SPP_BACKTRACE=0", "SPP_MEMORY_LIMIT_KB=0", "SPP_MEMORY_LIMIT_KB=1024", "SPP_NO_NETWORK_CHECK=1",
    "SPP_TEST_MODE=dev", "SPP_TEST_MODE=rel", "SPP_TEST_ONLY=", "SPP_TEST_ONLY=a b"}).empty());
}

TEST(TestEnv, test_a_count_must_be_a_whole_number) {
  for (const auto entry : {"SPP_MEMORY_LIMIT_KB=", "SPP_MEMORY_LIMIT_KB=abc", "SPP_MEMORY_LIMIT_KB=1k", "SPP_MEMORY_LIMIT_KB=-1"}) {
    EXPECT_EQ(ErrorsOf({entry}).size(), 1u) << entry;
  }
}

TEST(TestEnv, test_a_flag_must_be_zero_or_one) {
  for (const auto entry : {"SPP_NO_NETWORK_CHECK=", "SPP_NO_NETWORK_CHECK=true", "SPP_BACKTRACE=full"}) {
    EXPECT_EQ(ErrorsOf({entry}).size(), 1u) << entry;
  }
}

TEST(TestEnv, test_a_choice_must_be_listed) {
  const auto errors = ErrorsOf({"SPP_TEST_MODE=debug"});
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors[0].find("'dev', 'rel'"), spp::Str::npos);
}
