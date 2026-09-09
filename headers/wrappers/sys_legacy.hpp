#pragma once

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Included from the global module fragment of "sys.ixx", where the C headers' macros are still live. The purview
// there #undef's them so it can export its own "stdout", "errno" and friends, and once a macro is gone there is no
// way back to what it named: a "FILE *stdout = stdout;" in the purview resolves to the wrapper itself and
// self-initialises to null. These capture the real entities first, so the purview has something to bind to. This
// lives in a header rather than inline in the fragment because a global module fragment may only hold content
// that came from a preprocessor inclusion.
namespace spp_sys_legacy {
  inline auto Stderr() -> FILE* { return stderr; }
  inline auto Stdin() -> FILE* { return stdin; }
  inline auto Stdout() -> FILE* { return stdout; }
  inline auto Errno() -> int& { return errno; }
  inline auto IsDir(const unsigned int mode) -> int { return S_ISDIR(mode); }

  inline constexpr auto kORdonly = O_RDONLY;
  inline constexpr auto kORdwr = O_RDWR;
  inline constexpr auto kSeekSet = SEEK_SET;
}
