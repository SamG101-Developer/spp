#include "test_macros.hpp"

// gtest brings in <stdlib.h>, whose macros would rewrite the "sys::" names below.
#undef WEXITSTATUS
#undef WIFEXITED

import spp.utils.files;
import spp.utils.shell;
import spp.utils.types;
import std;
import sys;

namespace {
  /// A directory of its own under the test's temporary root, with a space and
  /// other characters a shell cares about in its name. Removed when the test ends.
  struct ScratchDir {
    spp::Path Root = std::filesystem::temp_directory_path()
      / ("spp shell $x " + std::to_string(std::random_device()()));
    ScratchDir() { std::filesystem::create_directories(Root); }
    ~ScratchDir() { std::filesystem::remove_all(Root); }
  };

  /// A script at "<dir>/a b" that exits with "code" if it is given exactly one
  /// argument, and with 99 otherwise, so a word split in the wrong place shows.
  auto ScriptIn(spp::Path const &dir, const int code) -> spp::Str {
#if SPP_PLATFORM_WINDOWS
    const auto script = dir / "a b.cmd";
    std::ofstream(script)
      << "@if \"%~1\"==\"\" exit /b 99\r\n@if not \"%~2\"==\"\" exit /b 99\r\n@exit /b " << code << "\r\n";
#else
    const auto script = dir / "a b";
    std::ofstream(script) << "#!/bin/sh\n[ \"$#\" = 1 ] || exit 99\nexit " << code << "\n";
    std::filesystem::permissions(script, std::filesystem::perms::owner_all);
#endif
    return spp::utils::files::NativeString(script);
  }

  auto ExitCodeOf(spp::Str const &command) -> int {
    const auto status = spp::utils::shell::Run(command);
    return sys::WIFEXITED(status) ? sys::WEXITSTATUS(status) : -1;
  }
}

TEST(TestShell, test_quote_keeps_an_argument_whole) {
#if SPP_PLATFORM_WINDOWS
  EXPECT_EQ(spp::utils::shell::Quote("plain"), "\"plain\"");
  EXPECT_EQ(spp::utils::shell::Quote("C:\\a b\\c.exe"), "\"C:\\a b\\c.exe\"");
  EXPECT_EQ(spp::utils::shell::Quote("C:\\a b\\"), "\"C:\\a b\\\\\"");
  EXPECT_EQ(spp::utils::shell::Quote("a\"b"), "\"a\\\"b\"");
  EXPECT_EQ(spp::utils::shell::Quote("a\\\"b"), "\"a\\\\\\\"b\"");
#else
  EXPECT_EQ(spp::utils::shell::Quote("plain"), "'plain'");
  EXPECT_EQ(spp::utils::shell::Quote("/a b/c"), "'/a b/c'");
  EXPECT_EQ(spp::utils::shell::Quote("it's"), "'it'\\''s'");
  EXPECT_EQ(spp::utils::shell::Quote("$(touch x) `y` ;&|"), "'$(touch x) `y` ;&|'");
#endif
}

TEST(TestShell, test_run_gives_the_exit_code_of_an_unquoted_command) {
  EXPECT_EQ(ExitCodeOf("exit 0"), 0);
  EXPECT_EQ(ExitCodeOf("exit 7"), 7);
}

TEST(TestShell, test_a_quoted_program_alone_runs) {
  // Given no argument, the script exits 99: this is that it ran at all.
  const auto dir = ScratchDir();
  EXPECT_EQ(ExitCodeOf(spp::utils::shell::Quote(ScriptIn(dir.Root, 7))), 99);
}

TEST(TestShell, test_a_quoted_program_with_a_quoted_argument_runs) {
  // The line "cmd /c" would strip the wrong quotes from without "Run"'s outer pair.
  const auto dir = ScratchDir();
  const auto command = spp::utils::shell::Quote(ScriptIn(dir.Root, 7)) + " " + spp::utils::shell::Quote("one word");
  EXPECT_EQ(ExitCodeOf(command), 7);
}
