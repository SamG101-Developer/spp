module;
#include <spp/macros-platforms.hpp>

#if SPP_PLATFORM_WINDOWS && !defined(_CRT_NONSTDC_NO_WARNINGS)
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <cstring>
#include <fcntl.h>
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

#include <wrappers/sys_legacy.hpp>

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

  const auto stdout = spp_sys_legacy::Stdout();
  const auto stdin = spp_sys_legacy::Stdin();
  const auto stderr = spp_sys_legacy::Stderr();
  constexpr auto O_RDONLY = spp_sys_legacy::kORdonly;
  constexpr auto O_RDWR = spp_sys_legacy::kORdwr;
  constexpr auto SEEK_SET = spp_sys_legacy::kSeekSet;

  thread_local auto &errno = spp_sys_legacy::Errno();

  inline auto S_ISDIR(const mode_t mode) -> int {
    return spp_sys_legacy::IsDir(mode);
  }
}
