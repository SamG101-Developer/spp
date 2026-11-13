module;
#include <spp/macros-platforms.hpp>

#if SPP_PLATFORM_WINDOWS
#if !defined(_CRT_NONSTDC_NO_WARNINGS)
#define _CRT_NONSTDC_NO_WARNINGS
#endif
#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#endif

#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#if SPP_PLATFORM_WINDOWS
// First: psapi.h uses BOOL, DWORD and WINAPI without including it.
#include <windows.h>

#include <direct.h>
#include <io.h>
#include <psapi.h>
#include <stddef.h>

#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <dlfcn.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <wrappers/sys_legacy.hpp>

#undef stderr
#undef stdin
#undef stdout
#undef errno
#undef environ
#undef S_ISDIR
#undef O_CREAT
#undef O_RDONLY
#undef O_RDWR
#undef SEEK_SET

#undef FILE_ATTRIBUTE_NORMAL
#undef FILE_SHARE_READ
#undef FILE_SHARE_WRITE
#undef GENERIC_READ
#undef GENERIC_WRITE
#undef INVALID_HANDLE_VALUE
#undef LOCKFILE_EXCLUSIVE_LOCK
#undef MAXDWORD
#undef OPEN_ALWAYS

#undef LOCK_EX
#undef LOCK_SH
#undef LOCK_UN
#undef RTLD_LAZY
#undef RTLD_NOLOAD
#undef X_OK
#undef WEXITSTATUS
#undef WIFEXITED

export module sys;

export namespace sys {
#if SPP_PLATFORM_WINDOWS
  using mode_t = unsigned short;
  using ssize_t = ::ptrdiff_t;

  inline auto strcasecmp(
    char const *lhs, char const *rhs) -> int {
    return ::_stricmp(lhs, rhs);
  }

  inline auto setenv(
    char const *name, char const *value, const int overwrite) -> int {
    if (overwrite == 0 and ::getenv(name) != nullptr) { return 0; }
    return ::_putenv_s(name, value);
  }

  inline auto unsetenv(const char *const name) -> int {
    return ::_putenv_s(name, "");
  }

  // "system" returns the exit code itself, not a wait status.
  inline auto WIFEXITED(const int) -> bool {
    return true;
  }

  inline auto WEXITSTATUS(const int status) -> int {
    return status;
  }

  // No execute bit, and _waccess rejects any mode but 0/2/4/6.
  constexpr auto X_OK = 0;

  inline auto access(
    wchar_t const *path, const int mode) -> int {
    return ::_waccess(path, mode);
  }

  using ::DWORD;
  using ::HANDLE;
  using ::OVERLAPPED;

  using ::CloseHandle;
  using ::CreateFileW;
  using ::LockFileEx;
  using ::UnlockFileEx;

  const auto INVALID_HANDLE_VALUE = spp_sys_legacy::InvalidHandleValue();
  constexpr auto FILE_ATTRIBUTE_NORMAL = spp_sys_legacy::kFileAttributeNormal;
  constexpr auto FILE_SHARE_READ = spp_sys_legacy::kFileShareRead;
  constexpr auto FILE_SHARE_WRITE = spp_sys_legacy::kFileShareWrite;
  constexpr auto GENERIC_READ = spp_sys_legacy::kGenericRead;
  constexpr auto GENERIC_WRITE = spp_sys_legacy::kGenericWrite;
  constexpr auto LOCKFILE_EXCLUSIVE_LOCK = spp_sys_legacy::kLockfileExclusiveLock;
  constexpr auto MAXDWORD = spp_sys_legacy::kMaxDword;
  constexpr auto OPEN_ALWAYS = spp_sys_legacy::kOpenAlways;

#else
  using ::ssize_t;
  using ::strcasecmp;
  using ::setenv;
  using ::unsetenv;

  // POSIX: file locking and the dynamic loader.
  using ::dlclose;
  using ::dlopen;
  using ::flock;

  inline auto WIFEXITED(
    const int status) -> bool {
    return spp_sys_legacy::IfExited(status);
  }

  inline auto WEXITSTATUS(
    const int status) -> int {
    return spp_sys_legacy::ExitStatus(status);
  }

  constexpr auto LOCK_EX = spp_sys_legacy::kLockEx;
  constexpr auto LOCK_SH = spp_sys_legacy::kLockSh;
  constexpr auto LOCK_UN = spp_sys_legacy::kLockUn;
  constexpr auto RTLD_LAZY = spp_sys_legacy::kRtldLazy;
  constexpr auto RTLD_NOLOAD = spp_sys_legacy::kRtldNoload;
  constexpr auto X_OK = spp_sys_legacy::kXOk;

#if SPP_PLATFORM_UNIX
  // glibc only: where a loaded object was loaded from.
  using ::dlinfo;
  using ::RTLD_DI_ORIGIN;
#endif
#endif

  using ::access;
  using ::close;
  using ::chdir;
  using ::fdopen;
  using ::fileno;
  using ::getenv;
  using ::isatty;
  using ::open;
  using ::read;
  using ::rmdir;
  using ::stat;
  using ::write;

  const auto stdout = spp_sys_legacy::Stdout();
  const auto stdin = spp_sys_legacy::Stdin();
  const auto stderr = spp_sys_legacy::Stderr();
  constexpr auto O_CREAT = spp_sys_legacy::kOCreat;
  constexpr auto O_RDONLY = spp_sys_legacy::kORdonly;
  constexpr auto O_RDWR = spp_sys_legacy::kORdwr;
  constexpr auto SEEK_SET = spp_sys_legacy::kSeekSet;

  thread_local auto &errno = spp_sys_legacy::Errno();

  inline auto environ() -> char** {
    return spp_sys_legacy::Environ();
  }

  inline auto S_ISDIR(const mode_t mode) -> int {
    return spp_sys_legacy::IsDir(mode);
  }

  // Have the console read this process's output as UTF-8 and act on ANSI
  // escapes, as a POSIX terminal already does; a no-op elsewhere.
  inline auto use_utf8_console() -> void {
#if SPP_PLATFORM_WINDOWS
    ::SetConsoleOutputCP(CP_UTF8);

    // Once per stream, through a lambda rather than a loop over a braced list:
    // this module imports no std, so std::initializer_list is not declared on
    // Windows (Linux's C headers happen to bring it in).
    const auto enable_vt = [](const ::DWORD which) {
      const auto handle = ::GetStdHandle(which);
      auto mode = ::DWORD{};
      if (handle != spp_sys_legacy::InvalidHandleValue() and ::GetConsoleMode(handle, &mode)) {
        ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
      }
    };
    enable_vt(STD_OUTPUT_HANDLE);
    enable_vt(STD_ERROR_HANDLE);
#endif
  }

  // The most memory this process has held, in bytes (0 if unknown).
  inline auto peak_rss_bytes() -> unsigned long long {
#if SPP_PLATFORM_WINDOWS
    auto counters = ::PROCESS_MEMORY_COUNTERS{};
    if (::K32GetProcessMemoryInfo(
      ::GetCurrentProcess(), &counters, sizeof(counters)) == 0) { return 0; }
    return counters.PeakWorkingSetSize;
#else
    auto usage = ::rusage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) { return 0; }
#if SPP_PLATFORM_MACOS or SPP_PLATFORM_IOS
    return static_cast<unsigned long long>(usage.ru_maxrss);
#else
    return static_cast<unsigned long long>(usage.ru_maxrss) * 1024ull;
#endif
#endif
  }
}
