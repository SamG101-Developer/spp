#pragma once

#include <spp/macros-platforms.hpp>

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if SPP_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace spp_sys_legacy {
  inline auto Stderr() -> FILE* { return stderr; }
  inline auto Stdin() -> FILE* { return stdin; }
  inline auto Stdout() -> FILE* { return stdout; }
  inline auto Errno() -> int& { return errno; }
  inline auto IsDir(const unsigned int mode) -> int { return S_ISDIR(mode); }

  inline constexpr auto kOCreat = O_CREAT;
  inline constexpr auto kORdonly = O_RDONLY;
  inline constexpr auto kORdwr = O_RDWR;
  inline constexpr auto kSeekSet = SEEK_SET;

#if SPP_PLATFORM_WINDOWS
  inline auto InvalidHandleValue() -> HANDLE { return INVALID_HANDLE_VALUE; }

  inline constexpr auto kFileAttributeNormal = DWORD{FILE_ATTRIBUTE_NORMAL};
  inline constexpr auto kFileShareRead = DWORD{FILE_SHARE_READ};
  inline constexpr auto kFileShareWrite = DWORD{FILE_SHARE_WRITE};
  inline constexpr auto kGenericRead = DWORD{GENERIC_READ};
  inline constexpr auto kGenericWrite = DWORD{GENERIC_WRITE};
  inline constexpr auto kLockfileExclusiveLock = DWORD{LOCKFILE_EXCLUSIVE_LOCK};
  inline constexpr auto kMaxDword = DWORD{MAXDWORD};
  inline constexpr auto kOpenAlways = DWORD{OPEN_ALWAYS};
#else
  inline constexpr auto kLockEx = LOCK_EX;
  inline constexpr auto kLockSh = LOCK_SH;
  inline constexpr auto kLockUn = LOCK_UN;
  inline constexpr auto kRtldLazy = RTLD_LAZY;
  inline constexpr auto kRtldNoload = RTLD_NOLOAD;
  inline constexpr auto kXOk = X_OK;
#endif
}
