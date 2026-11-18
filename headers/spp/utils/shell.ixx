module;
#include <spp/macros.hpp>

export module spp.utils.shell;
import spp.utils.types;
import std;

namespace spp::utils::shell {
  /// One argument of a shell command, quoted so the shell passes it on as it is
  /// whatever it holds. For paths and values, not for a command that is meant
  /// to be split into words, like a linker or a runner.
  SPP_EXP_FUN auto Quote(StrView arg) -> Str;

  /// Run a command through the shell, returning what "std::system" does: a wait
  /// status on POSIX and the exit code on Windows, which "sys::WIFEXITED" and
  /// "sys::WEXITSTATUS" read on both. Every command the compiler runs goes
  /// through here.
  SPP_EXP_FUN auto Run(Str const &command) -> int;

  /// What separates the directories on PATH: ";" on Windows, where a drive
  /// letter takes ":", and ":" elsewhere.
#if SPP_PLATFORM_WINDOWS
  SPP_EXP_CON inline constexpr auto PathListSeparator = ';';
#else
  SPP_EXP_CON inline constexpr auto PathListSeparator = ':';
#endif

  /// Find a program as a shell would, by searching for "name" in each directory
  /// on PATH. The first that is an executable file.
  SPP_EXP_FUN auto FindProgram(StrView name) -> Opt<Path>;

  /// A command someone wrote (a program, then its arguments, as in
  /// "[target.linkers]"), ready for the shell, or nothing when its program is
  /// not there. A path to a program on its own (with a directory) may have
  /// spaces: it comes back quoted, so the shell runs it as one word. Otherwise the first word (or a
  /// quoted run, "..." or '...') is the program: found where it says when it
  /// has a directory, or on PATH as a shell would, and the command comes back
  /// as written.
  SPP_EXP_FUN auto FindCommand(StrView command) -> Opt<Str>;

  /// The newest "name-N" on PATH, like "loongarch64-linux-gnu-gcc-14"; gcc
  /// names a versioned driver and sometimes multiple versions might exist.
  SPP_EXP_FUN auto FindNewestVersion(StrView name) -> Opt<Path>;
}
