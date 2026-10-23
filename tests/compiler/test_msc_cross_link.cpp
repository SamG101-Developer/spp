#include <stdlib.h>  // setenv, unsetenv
#include "test_macros.hpp"

import spp.codegen.llvm_target;
import spp.utils.features;
import spp.utils.files;

// How a foreign target's linker is found: its GNU cross gcc, spelled from the target folder, else the newest versioned
// one on PATH, unless [target.linkers] names one.

namespace {
  /// A directory of its own under the test's temporary root, removed when the test ends.
  struct ScratchDir {
    spp::Path Root = std::filesystem::temp_directory_path()
      / ("spp_cross_link_" + std::to_string(std::random_device()()));
    ScratchDir() { std::filesystem::create_directories(Root); }
    ~ScratchDir() { std::filesystem::remove_all(Root); }
  };

  /// An empty file at "path", executable or not.
  auto Touch(spp::Path const &path, const bool executable) -> void {
    std::ofstream(path).put('\n');
    std::filesystem::permissions(path, executable
      ? std::filesystem::perms::owner_all
      : std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
  }

  /// PATH set to "value" for one scope, then put back.
  struct PathAs {
    spp::Opt<spp::Str> Saved;
    explicit PathAs(spp::Str const &value) {
      if (auto const *const old = std::getenv("PATH"); old != nullptr) { Saved = old; }
      setenv("PATH", value.c_str(), 1);
    }
    ~PathAs() {
      if (Saved.has_value()) { setenv("PATH", Saved->c_str(), 1); }
      else { unsetenv("PATH"); }
    }
  };
}

TEST(TestCrossLink, test_gnu_triple_is_the_folder_but_for_gnus_own_spellings) {
  EXPECT_EQ(spp::codegen::GnuTripleOf("s390x-linux-gnu"), "s390x-linux-gnu");
  EXPECT_EQ(spp::codegen::GnuTripleOf("arm-linux-gnueabihf"), "arm-linux-gnueabihf");
  EXPECT_EQ(spp::codegen::GnuTripleOf("mips64el-linux-gnuabi64"), "mips64el-linux-gnuabi64");
  EXPECT_EQ(spp::codegen::GnuTripleOf("i386-linux-gnu"), "i686-linux-gnu");
  EXPECT_EQ(spp::codegen::GnuTripleOf("sparcv9-linux-gnu"), "sparc64-linux-gnu");
  // Only the architecture is respelled, not a match elsewhere in the name.
  EXPECT_EQ(spp::codegen::GnuTripleOf("x86_64-linux-gnux32"), "x86_64-linux-gnux32");
}

TEST(TestCrossLink, test_the_newest_versioned_program_is_found) {
  const auto first = ScratchDir();
  const auto second = ScratchDir();
  Touch(first.Root / "cross-gcc-12", true);
  Touch(second.Root / "cross-gcc-14", true);
  Touch(second.Root / "cross-gcc-9", true);
  Touch(first.Root / "cross-gcc-15", false); // not executable
  Touch(first.Root / "cross-gcc-16x", true); // not a version
  const auto path = PathAs(
    spp::utils::files::NativeString(first.Root) + ":" + spp::utils::files::NativeString(second.Root));

  const auto newest = spp::utils::files::FindNewestVersion("cross-gcc");
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(*newest, second.Root / "cross-gcc-14");
  EXPECT_FALSE(spp::utils::files::FindProgram("cross-gcc").has_value());
  EXPECT_FALSE(spp::utils::files::FindNewestVersion("other-gcc").has_value());
}

TEST(TestCrossLink, test_a_program_is_found_on_path_in_order) {
  const auto first = ScratchDir();
  const auto second = ScratchDir();
  Touch(first.Root / "cross-gcc", false);
  Touch(second.Root / "cross-gcc", true);
  const auto path = PathAs(
    spp::utils::files::NativeString(first.Root) + ":" + spp::utils::files::NativeString(second.Root));

  const auto found = spp::utils::files::FindProgram("cross-gcc");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(*found, second.Root / "cross-gcc"); // the first is not executable
}

TEST(TestCrossLink, test_target_linkers_name_a_command_per_folder) {
  const auto dir = ScratchDir();
  const auto config = dir.Root / "spp.toml";
  std::ofstream(config) << "[project]\nname = \"p\"\nversion = \"0.1.0\"\nbuild = \"exe\"\n\n"
    << "[target.linkers]\nloongarch64-linux-gnu = \"loongarch64-linux-gnu-gcc-14\"\n"
    << "s390x-linux-gnu = \"clang --target=s390x-linux-gnu\"\n";

  auto errors = spp::Vec<spp::Str>();
  ASSERT_TRUE(spp::utils::features::Load(config, errors)) << (errors.empty() ? "" : errors.front());
  EXPECT_EQ(spp::utils::features::Linker("loongarch64-linux-gnu"), "loongarch64-linux-gnu-gcc-14");
  EXPECT_EQ(spp::utils::features::Linker("s390x-linux-gnu"), "clang --target=s390x-linux-gnu");
  EXPECT_FALSE(spp::utils::features::Linker("mips-linux-gnu").has_value());
  spp::utils::features::Reset();
  EXPECT_FALSE(spp::utils::features::Linker("loongarch64-linux-gnu").has_value());
}

TEST(TestCrossLink, test_a_linker_that_is_not_a_string_is_refused) {
  const auto dir = ScratchDir();
  const auto config = dir.Root / "spp.toml";
  std::ofstream(config) << "[project]\nname = \"p\"\nversion = \"0.1.0\"\nbuild = \"exe\"\n\n"
    << "[target.linkers]\ns390x-linux-gnu = 3\n";

  auto errors = spp::Vec<spp::Str>();
  EXPECT_FALSE(spp::utils::features::Load(config, errors));
  ASSERT_EQ(errors.size(), 1U);
  EXPECT_NE(errors.front().find("[target.linkers]"), spp::Str::npos) << errors.front();
  spp::utils::features::Reset();
}
