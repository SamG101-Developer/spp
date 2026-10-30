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
}
