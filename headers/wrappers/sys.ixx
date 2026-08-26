module;
#include <spp/macros-platforms.hpp>

#if SPP_PLATFORM_WINDOWS && !defined(_CRT_NONSTDC_NO_WARNINGS)
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if SPP_PLATFORM_WINDOWS
#include <direct.h>
#include <io.h>
#include <stddef.h>

#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#endif

#define LEGACY_STDERR stderr
#define LEGACY_STDIN stdin
#define LEGACY_STDOUT stdout
#define LEGACY_ERRNO errno
#define LEGACY_S_ISDIR S_ISDIR

#undef stderr
#undef stdin
#undef stdout
#undef errno
#undef S_ISDIR
#undef O_RDONLY
#undef O_RDWR
#undef SEEK_SET

export module sys;

export namespace sys {
#if SPP_PLATFORM_WINDOWS
  // The CRT has none of these under their POSIX names: mode_t and
  // ssize_t it never declares, and strcasecmp it spells _stricmp.
  // They live here rather than in the global module fragment, which
  // may hold preprocessor directives and nothing else.
  using mode_t = unsigned short;
  using ssize_t = ::ptrdiff_t;

  inline auto strcasecmp(const char *const lhs, const char *const rhs) -> int {
    return ::_stricmp(lhs, rhs);
  }
#else
  using ::ssize_t;
  using ::strcasecmp;
#endif

  using ::close;
  using ::chdir;
  using ::fdopen;
  using ::fileno;
  using ::isatty;
  using ::open;
  using ::read;
  using ::rmdir;
  using ::stat;
  using ::write;

  FILE *stdout = LEGACY_STDOUT;
  FILE *stdin = LEGACY_STDIN;
  FILE *stderr = LEGACY_STDERR;
  constexpr auto O_RDONLY = 0;
  constexpr auto O_RDWR = 2;
  constexpr auto SEEK_SET = static_cast<short>(0);
  int errno = LEGACY_ERRNO;
  std::function<int(mode_t)> S_ISDIR = [](const mode_t mode) {
    return LEGACY_S_ISDIR(mode);
  };
}
