module;
#include <spp/macros.hpp>

export module spp.utils.files;
import spp.utils.types;
import std;

namespace spp::utils::files {
  /// Convert a path to a string for human presentation, with a C++16/17 split
  /// for correct behaviour.
  SPP_EXP_FUN auto DisplayString(Path const &path) -> Str;

  /// Convert a path to its native-encoded string, with a C++16/17 split for
  /// correct behaviour.
  SPP_EXP_FUN auto NativeString(Path const &path) -> Str;

  /// Read the contents of a file into a string. The only file-read function,
  /// for uniform handling in all areas.
  SPP_EXP_FUN auto ReadFile(Path const &path) -> Str;

  /// Write a string to a file. The only file-write function, for uniform
  /// handling in all areas.
  SPP_EXP_FUN auto WriteFile(Path const &path, Str const &content) -> void;

  /// Recursively walk a directory, collecting every ".spp" file under it. This
  /// is used to load the module trees.
  SPP_EXP_FUN auto GlobSpp(Path const &path) -> Vec<Path>;

  /// The library name for a package and extension. For example, "sppc" and "so"
  /// will get "libsppc.so", where-as "sppc" and "dll" is just "sppc.dll".
  SPP_EXP_FUN auto SharedLibraryName(StrView package, StrView extension) -> Str;

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

  /// The newest "name-N" on PATH, like "loongarch64-linux-gnu-gcc-14"; gcc
  /// names a versioned driver and sometimes multiple versions might exist.
  SPP_EXP_FUN auto FindNewestVersion(StrView name) -> Opt<Path>;

  /// The environment variable "name", or none when it is not set. Safe wrapper
  /// for c++ "getenv".
  SPP_EXP_FUN auto GetEnv(StrView name) -> Opt<Str>;

  /// Set the environment variable "name" to "value", replacing any value it
  /// had. On Windows an empty value unsets it. Safe wrapper for c++ "setenv".
  SPP_EXP_FUN auto SetEnv(StrView name, StrView value) -> void;

  /// Remove the environment variable "name", if it is set. Safe wrapper for c++
  /// "unsetenv".
  SPP_EXP_FUN auto UnsetEnv(StrView name) -> void;

  /// An advisory lock over a whole file: "flock(2)" on POSIX, and the
  /// equivalent whole-range "LockFileEx" on Windows. Both platforms hold the
  /// lock on the open file rather than on the process, so it is released when
  /// the file closes.
  SPP_EXP_CLS class FileLock;
}

SPP_EXP_CLS class spp::utils::files::FileLock {
public:
  FileLock() = default;
  FileLock(FileLock const &) = delete;
  FileLock(FileLock &&other) noexcept;
  auto operator=(FileLock const &) -> FileLock& = delete;
  auto operator=(FileLock &&other) noexcept -> FileLock&;
  ~FileLock();

  /// Take a shared lock (n readers) on the file. Returns true if the lock was
  /// successful.
  auto LockShared(Path const &path) -> bool;

  /// Take an exclusive lock (1 writer) on the file. Returns true if the lock
  /// was successful.
  auto LockExclusive(Path const &path) -> bool;

  /// Release the lock and close the file.
  auto Unlock() -> void;

private:
  /// The handle for the file that the lock is active over (an int fd on POSIX,
  /// or a HANDLE on Windows). -1 by default (no lock).
  std::intptr_t _Handle = -1;

  /// Open "path", creating it if it is not there, and lock it, waiting for as
  /// long as another holder has it. An exclusive lock is a writer's, otherwise
  /// a reader's. Returns true if the lock was taken.
  auto _Acquire(Path const &path, bool exclusive) -> bool;
};
