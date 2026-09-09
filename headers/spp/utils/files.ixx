module;
#include <spp/macros.hpp>

export module spp.utils.files;
import spp.utils.types;
import std;

namespace spp::utils::files {
  /**
   * Simple function to read the contents of a file into a string.
   * @param path The path to the file to read.
   * @return The contents of the file as a string.
   */
  SPP_EXP_FUN auto ReadFile(std::filesystem::path const &path) -> Str;

  /**
   * Simple function to write a string to a file.
   * @param path The path to the file to write to.
   * @param content The content to write to the file.
   */
  SPP_EXP_FUN auto WriteFile(std::filesystem::path const &path, Str const &content) -> void;

  /**
   * Convert a filesystem path to a string for human presentation, using the
   * correct member for the standard library version: display_string() on
   * libstdc++ 17+ (where string() is deprecated), string() on older releases.
   * The result may be lossily transcoded, so only use it for logs, error
   * messages and other display output -- never to reopen the path.
   * @param path The path to stringify.
   * @return The path as a display string.
   */
  SPP_EXP_FUN auto DisplayString(std::filesystem::path const &path) -> Str;

  /**
   * Convert a filesystem path to its native-encoded string, using the correct
   * member for the standard library version: native_encoded_string() on
   * libstdc++ 17+ (where string() is deprecated), string() on older releases.
   * The result faithfully round-trips the path, so use it when the string is
   * fed back to the OS/shell, used to reopen the file, or compared against
   * native path components (e.g. preferred_separator).
   * @param path The path to stringify.
   * @return The path as a native-encoded string.
   */
  SPP_EXP_FUN auto NativeString(std::filesystem::path const &path) -> Str;

  /**
   * Recursively walk a directory, collecting every ".spp" file under it.
   * @param path The directory to walk.
   * @return The paths of the ".spp" files found.
   */
  SPP_EXP_FUN auto GlobSpp(std::filesystem::path const &path) -> Vec<std::filesystem::path>;

  /**
   * The extension a shared library carries on this host, without the leading dot: "so", "dylib" or "dll".
   */
  SPP_EXP_FUN auto SharedLibraryExtension() -> Str;

  /**
   * The one file name a package's shared library may have on this host, which is the name the loader will ask for
   * once something links against it. On an ELF or Mach-O host that is the soname, and a soname carries the "lib"
   * prefix; on Windows there is no soname and a dll is imported under the file name it was built as.
   * @param[in] package The package's folder name under "ffi", which is also the library's own name.
   */
  SPP_EXP_FUN auto SharedLibraryName(StrView package) -> Str;

  /**
   * An advisory lock over a whole file: flock(2) on POSIX, and the equivalent
   * whole-range LockFileEx on Windows. Both platforms hold the lock on the open
   * file rather than on the process, so it is released when the file closes.
   */
  SPP_EXP_CLS class FileLock {
    /**
     * The open file the lock is held on: a file descriptor on POSIX, a HANDLE
     * on Windows, and -1 when no lock is held.
     */
    std::intptr_t m_handle = -1;

    /**
     * Open @p path, creating it if it is not there, and lock it, waiting for as
     * long as another holder has it.
     * @param[in] path The file to lock.
     * @param[in] exclusive Take a writer's lock rather than a reader's.
     * @return Whether the lock was taken.
     */
    auto Acquire(std::filesystem::path const &path, bool exclusive) -> bool;

  public:
    FileLock() = default;

    FileLock(FileLock const &) = delete;

    auto operator=(FileLock const &) -> FileLock & = delete;

    FileLock(FileLock &&other) noexcept :
      m_handle(other.m_handle) {
      other.m_handle = -1;
    }

    auto operator=(FileLock &&other) noexcept -> FileLock & {
      if (this != &other) {
        Unlock();
        m_handle = other.m_handle;
        other.m_handle = -1;
      }
      return *this;
    }

    ~FileLock();

    /**
     * Create a shared lock (n readers).
     * @param[in] path The file to lock.
     * @return Whether the lock was taken.
     */
    auto LockShared(std::filesystem::path const &path) -> bool;

    /**
     * Create an exclusive lock (1 writer).
     * @param[in] path The file to lock.
     * @return Whether the lock was taken.
     */
    auto LockExclusive(std::filesystem::path const &path) -> bool;

    /**
     * Release the lock and close the file.
     */
    auto Unlock() -> void;
  };
}
