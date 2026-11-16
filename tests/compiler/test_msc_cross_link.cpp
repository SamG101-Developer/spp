#include "test_macros.hpp"

import spp.codegen.llvm_target;
import spp.utils.env;
import spp.utils.features;
import spp.utils.files;
import spp.utils.shell;

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

  /// "first" then "second", as PATH lists directories.
  auto PathOf(spp::Path const &first, spp::Path const &second) -> spp::Str {
    return spp::utils::files::NativeString(first) + spp::utils::files::PathListSeparator
      + spp::utils::files::NativeString(second);
  }

  /// PATH set to "value" for one scope, then put back.
  struct PathAs {
    spp::Opt<spp::Str> Saved = spp::utils::env::Get(spp::utils::env::Var::Path);
    explicit PathAs(spp::Str const &value) { spp::utils::env::Set(spp::utils::env::Var::Path, value); }
    ~PathAs() {
      if (Saved.has_value()) { spp::utils::env::Set(spp::utils::env::Var::Path, *Saved); }
      else { spp::utils::env::Unset(spp::utils::env::Var::Path); }
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
#if SPP_PLATFORM_WINDOWS
  GTEST_SKIP() << "Windows has no executable bit: every regular file counts as runnable";
#endif
  const auto first = ScratchDir();
  const auto second = ScratchDir();
  Touch(first.Root / "cross-gcc-12", true);
  Touch(second.Root / "cross-gcc-14", true);
  Touch(second.Root / "cross-gcc-9", true);
  Touch(first.Root / "cross-gcc-15", false); // not executable
  Touch(first.Root / "cross-gcc-16x", true); // not a version
  const auto path = PathAs(PathOf(first.Root, second.Root));

  const auto newest = spp::utils::files::FindNewestVersion("cross-gcc");
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(*newest, second.Root / "cross-gcc-14");
  EXPECT_FALSE(spp::utils::files::FindProgram("cross-gcc").has_value());
  EXPECT_FALSE(spp::utils::files::FindNewestVersion("other-gcc").has_value());
}

TEST(TestCrossLink, test_a_program_is_found_on_path_in_order) {
#if SPP_PLATFORM_WINDOWS
  GTEST_SKIP() << "Windows has no executable bit: every regular file counts as runnable";
#endif
  const auto first = ScratchDir();
  const auto second = ScratchDir();
  Touch(first.Root / "cross-gcc", false);
  Touch(second.Root / "cross-gcc", true);
  const auto path = PathAs(PathOf(first.Root, second.Root));

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

TEST(TestCrossLink, test_a_full_path_with_spaces_on_its_own_comes_back_quoted) {
  const auto dir = ScratchDir();
  std::filesystem::create_directories(dir.Root / "my tools");
  const auto linker = dir.Root / "my tools" / "cross-gcc";
  Touch(linker, true);

  const auto command = spp::utils::files::FindCommand(spp::utils::files::NativeString(linker));
  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(*command, spp::utils::shell::Quote(spp::utils::files::NativeString(linker)));
}

TEST(TestCrossLink, test_a_command_whose_program_is_there_comes_back_as_written) {
  const auto dir = ScratchDir();
  std::filesystem::create_directories(dir.Root / "my tools");
  Touch(dir.Root / "cross-gcc", true);
  Touch(dir.Root / "my tools" / "cross-ld", true);
  const auto path = PathAs(spp::utils::files::NativeString(dir.Root));

  // A name on PATH with arguments, and a quoted path with spaces with arguments.
  EXPECT_EQ(spp::utils::files::FindCommand("cross-gcc -fuse-ld=lld"), "cross-gcc -fuse-ld=lld");
  const auto quoted = "\"" + spp::utils::files::NativeString(dir.Root / "my tools" / "cross-ld") + "\" -static";
  EXPECT_EQ(spp::utils::files::FindCommand(quoted), quoted);
}

TEST(TestCrossLink, test_a_command_whose_program_is_not_there_is_refused) {
  const auto dir = ScratchDir();
  const auto path = PathAs(spp::utils::files::NativeString(dir.Root));

  EXPECT_FALSE(spp::utils::files::FindCommand("no-such-gcc --target=s390x-linux-gnu").has_value());
  const auto missing = spp::utils::files::NativeString(dir.Root / "missing" / "gcc");
  EXPECT_FALSE(spp::utils::files::FindCommand(missing).has_value());
  EXPECT_FALSE(spp::utils::files::FindCommand("").has_value());
}

TEST(TestCrossLink, test_a_bare_name_is_looked_for_on_path_not_in_the_current_directory) {
  const auto on_path = ScratchDir();
  const auto here = ScratchDir();
  Touch(here.Root / "local-gcc", true);
  const auto path = PathAs(spp::utils::files::NativeString(on_path.Root));

  const auto cwd = std::filesystem::current_path();
  std::filesystem::current_path(here.Root);
  const auto local = spp::utils::files::FindCommand("local-gcc");
  std::filesystem::current_path(cwd);
  EXPECT_FALSE(local.has_value());
}

TEST(TestCrossLink, test_a_non_executable_program_is_refused) {
#if SPP_PLATFORM_WINDOWS
  GTEST_SKIP() << "Windows has no executable bit: every regular file counts as runnable";
#endif
  const auto dir = ScratchDir();
  const auto linker = dir.Root / "cross-gcc";
  Touch(linker, false);
  EXPECT_FALSE(spp::utils::files::FindCommand(spp::utils::files::NativeString(linker)).has_value());
}
