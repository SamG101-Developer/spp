module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <version>  // _GLIBCXX_RELEASE

#if SPP_PLATFORM_WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <sys/file.h>
  #include <unistd.h>
#endif

module spp.utils.files;
import std;

auto spp::utils::files::DisplayString(
  std::filesystem::path const &path)
  -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.display_string();
#else
  return path.string();
#endif
}

auto spp::utils::files::NativeString(
  std::filesystem::path const &path)
  -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.native_encoded_string();
#else
  return path.string();
#endif
}

auto spp::utils::files::ReadFile(
  std::filesystem::path const &path)
  -> Str {
  // Create an input file stream.
  auto in = std::ifstream(path, std::ios::in);
  auto buf = std::ostringstream();
  buf << in.rdbuf();
  return buf.str();
}

auto spp::utils::files::WriteFile(
  std::filesystem::path const &path,
  Str const &content)
  -> void {
  // Create an output file stream.
  auto out = std::ofstream(path);
  out << content;
}

SPP_MOD_BEGIN
spp::utils::files::FileLock::~FileLock() {
  Unlock();
}

auto spp::utils::files::FileLock::Acquire(
  std::filesystem::path const &path,
  const bool exclusive)
  -> bool {
  // A second lock from the same object would leak the first
  // one's file, and on Windows it would also deadlock against
  // itself.
  Unlock();

#if SPP_PLATFORM_WINDOWS
  // OPEN_ALWAYS is O_CREAT: open the file, creating an empty one
  // if it is not there. The share flags let the other holders
  // open it at all -- the lock, not the open, is what excludes
  // them.
  auto const handle = ::CreateFileW(
    path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) { return false; }

  // The range is the largest one expressible, which is how
  // LockFileEx spells "the whole file" for a file that may still
  // grow.
  auto overlapped = OVERLAPPED{};
  auto const flags = exclusive ? DWORD{LOCKFILE_EXCLUSIVE_LOCK} : DWORD{0};
  if (not ::LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    ::CloseHandle(handle);
    return false;
  }

  m_handle = reinterpret_cast<std::intptr_t>(handle);
  return true;
#else
  auto const fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) { return false; }

  if (::flock(fd, exclusive ? LOCK_EX : LOCK_SH) != 0) {
    ::close(fd);
    return false;
  }

  m_handle = fd;
  return true;
#endif
}

auto spp::utils::files::FileLock::LockShared(
  std::filesystem::path const &path)
  -> bool {
  return Acquire(path, false);
}

auto spp::utils::files::FileLock::LockExclusive(
  std::filesystem::path const &path)
  -> bool {
  return Acquire(path, true);
}

auto spp::utils::files::FileLock::Unlock()
  -> void {
  if (m_handle == -1) { return; }

#if SPP_PLATFORM_WINDOWS
  auto const handle = reinterpret_cast<HANDLE>(m_handle);
  auto overlapped = OVERLAPPED{};
  ::UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped);
  ::CloseHandle(handle);
#else
  auto const fd = static_cast<int>(m_handle);
  ::flock(fd, LOCK_UN);
  ::close(fd);
#endif

  m_handle = -1;
}
SPP_MOD_END

auto spp::utils::files::GlobSpp(
  std::filesystem::path const &path)
  -> Vec<std::filesystem::path> {
  // Use the filesystem iterator to recursively walk the path, finding all ".spp" files.
  auto paths = Vec<std::filesystem::path>();
  for (auto const &entry : std::filesystem::recursive_directory_iterator(path)) {
    if (not entry.is_regular_file()) { continue; }
    if (entry.path().extension() != ".spp") { continue; }
    paths.EmplaceBack(entry.path());
  }
  return paths;
}

auto spp::utils::files::SharedLibraryExtension()
  -> Str {
#if SPP_PLATFORM_WINDOWS
  return "dll";
#elif SPP_PLATFORM_MACOS || SPP_PLATFORM_IOS
  return "dylib";
#else
  return "so";
#endif
}

auto spp::utils::files::SharedLibraryName(
  const StrView package)
  -> Str {
#if SPP_PLATFORM_WINDOWS
  return Str(package) + "." + SharedLibraryExtension();
#else
  return "lib" + Str(package) + "." + SharedLibraryExtension();
#endif
}
