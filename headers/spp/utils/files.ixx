module;
#include <spp/macros.hpp>

export module spp.utils.files;
import spp.utils.types;
import std;

namespace spp::utils::files {
  /// Read the contents of a file into a string.
  SPP_EXP_FUN auto ReadFile(Path const &path) -> Str;

  /// Write a string to a file.
  SPP_EXP_FUN auto WriteFile(Path const &path, Str const &content) -> void;

  /// Convert a path to a string for human presentation, using
  /// the right member for the standard library version:
  /// "display_string()" on libstdc++ 17+ (where "string()" is
  /// deprecated), "string()" on older releases. The result may
  /// be lossily transcoded, so only use it for logs, error
  /// messages and other display output - never to reopen the
  /// path.
  SPP_EXP_FUN auto DisplayString(Path const &path) -> Str;

  /// Convert a path to its native-encoded string, using the
  /// right member for the standard library version:
  /// "native_encoded_string()" on libstdc++ 17+ (where
  /// "string()" is deprecated), "string()" on older releases.
  /// The result round-trips the path faithfully, so use it
  /// when the string goes back to the OS or shell, reopens the
  /// file, or is compared against native path components (e.g.
  /// "preferred_separator").
  SPP_EXP_FUN auto NativeString(Path const &path) -> Str;

  /// Recursively walk a directory, collecting every ".spp"
  /// file under it.
  SPP_EXP_FUN auto GlobSpp(Path const &path) -> Vec<Path>;

  /// The one file name a package's shared library may have on
  /// a target, which is the name the loader will ask for once
  /// something links against it. On an ELF or Mach-O target
  /// that is the soname, and a soname carries the "lib" prefix;
  /// on Windows there is no soname, and a dll is imported under
  /// the file name it was built as. "package" is the package's
  /// folder name under "ffi", which is also the library's own
  /// name, and "extension" is the target's, without the dot.
  SPP_EXP_FUN auto SharedLibraryName(StrView package, StrView extension) -> Str;

  /// What separates the directories on PATH: ";" on Windows,
  /// where a drive letter takes ":", and ":" elsewhere.
#if SPP_PLATFORM_WINDOWS
  SPP_EXP_CON inline constexpr auto PathListSeparator = ';';
#else
  SPP_EXP_CON inline constexpr auto PathListSeparator = ':';
#endif

  /// Find a program as a shell would: "name" in each directory on
  /// PATH in turn, the first that is an executable file.
  SPP_EXP_FUN auto FindProgram(StrView name) -> Opt<Path>;

  /// The newest "name-N" on PATH, as gcc names a versioned driver
  /// ("loongarch64-linux-gnu-gcc-14"): the highest N found, and the
  /// first on PATH of two with the same N.
  SPP_EXP_FUN auto FindNewestVersion(StrView name) -> Opt<Path>;

  /// The environment variable "name", or none when it is not set.
  SPP_EXP_FUN auto GetEnv(StrView name) -> Opt<Str>;

  /// Set the environment variable "name" to "value", replacing
  /// any value it had. On Windows an empty value unsets it.
  SPP_EXP_FUN auto SetEnv(StrView name, StrView value) -> void;

  /// Remove the environment variable "name", if it is set.
  SPP_EXP_FUN auto UnsetEnv(StrView name) -> void;

  /// An advisory lock over a whole file: "flock(2)" on POSIX,
  /// and the equivalent whole-range "LockFileEx" on Windows.
  /// Both platforms hold the lock on the open file rather than
  /// on the process, so it is released when the file closes.
  SPP_EXP_CLS class FileLock {
    /// The open file the lock is held on: a file descriptor on
    /// POSIX, a HANDLE on Windows, and -1 when no lock is held.
    std::intptr_t _Handle = -1;

    /// Open "path", creating it if it is not there, and lock
    /// it, waiting for as long as another holder has it. An
    /// exclusive lock is a writer's, otherwise a reader's.
    /// Answers whether the lock was taken.
    auto Acquire(Path const &path, bool exclusive) -> bool;

  public:
    FileLock() = default;

    FileLock(FileLock const &) = delete;

    auto operator=(FileLock const &) -> FileLock & = delete;

    FileLock(FileLock &&other) noexcept :
      _Handle(other._Handle) {
      other._Handle = -1;
    }

    auto operator=(FileLock &&other) noexcept -> FileLock & {
      if (this != &other) {
        Unlock();
        _Handle = other._Handle;
        other._Handle = -1;
      }
      return *this;
    }

    ~FileLock();

    /// Take a shared lock (n readers) on the file. Answers
    /// whether the lock was taken.
    auto LockShared(Path const &path) -> bool;

    /// Take an exclusive lock (1 writer) on the file. Answers
    /// whether the lock was taken.
    auto LockExclusive(Path const &path) -> bool;

    /// Release the lock and close the file.
    auto Unlock() -> void;
  };
}
