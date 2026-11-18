module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <version>  // _GLIBCXX_RELEASE

module spp.utils.files;
import genex;
import std;
import sys;

namespace fs = std::filesystem;
constexpr auto SPP_EXT = ".spp";

/// [CHECKED]
auto spp::utils::files::DisplayString(Path const &path) -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.display_string();
#else
  return path.string();
#endif
}

/// [CHECKED]
auto spp::utils::files::NativeString(Path const &path) -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.native_encoded_string();
#else
  return path.string();
#endif
}

/// [CHECKED]
auto spp::utils::files::ReadFile(Path const &path) -> Str {
  // Create an input file stream from the file, and then read it into a buffer,
  // extracting that into a string.
  auto in = std::ifstream(path, std::ios::in);
  auto buf = std::ostringstream();
  buf << in.rdbuf();
  return buf.str();
}

/// [CHECKED]
auto spp::utils::files::ReadBytes(Path const &path) -> Opt<Str> {
  // Binary, so nothing is translated; a stream that did not open is a file
  // that could not be read.
  auto in = std::ifstream(path, std::ios::binary);
  if (not in) { return Nullopt; }
  return Str(std::istreambuf_iterator(in), {});
}

/// [CHECKED]
auto spp::utils::files::WriteFile(
  Path const &path, Str const &content) -> void {
  // Create an output file stream from the path, and push the contents into it.
  auto out = std::ofstream(path);
  out << content;
}

/// [CHECKED]
auto spp::utils::files::GlobSpp(Path const &path) -> Vec<Path> {
  // Use the filesystem iterator to recursively walk the path, finding all
  // ".spp" files.
  auto paths = Vec<Path>();
  for (auto const &entry : fs::recursive_directory_iterator(path)) {
    if (not entry.is_regular_file()) { continue; }
    if (entry.path().extension() != SPP_EXT) { continue; }
    paths.emplace_back(entry.path());
  }

  // Normalise file order across machines for consistent erroring although file
  // and declarations should be order agnostic.
  paths |= genex::actions::sort;
  return paths;
}

SPP_MOD_BEGIN
/// [CHECKED]
spp::utils::files::FileLock::~FileLock() {
  // A lock destructing needs to free its internal file descriptor, otherwise
  // the locks would all leak.
  Unlock();
}

/// [CHECKED]
spp::utils::files::FileLock::FileLock(FileLock &&other) noexcept :
  _Handle(other._Handle) {
  other._Handle = -1;
}

/// [CHECKED]
auto spp::utils::files::FileLock::operator=(
  FileLock &&other) noexcept -> FileLock& {
  if (this != &other) {
    Unlock();
    _Handle = other._Handle;
    other._Handle = -1;
  }
  return *this;
}

auto spp::utils::files::FileLock::LockShared(Path const &path) -> bool {
  // Call the internal lock acquisition function, with the "exclusive" flag set
  // to false: shared.
  return _Acquire(path, false);
}

auto spp::utils::files::FileLock::LockExclusive(Path const &path) -> bool {
  // Call the internal lock acquisition function, with the "exclusive" flag set
  // to true: exclusive.
  return _Acquire(path, true);
}

auto spp::utils::files::FileLock::Unlock() -> void {
  if (_Handle == -1) { return; }

#if SPP_PLATFORM_WINDOWS
  const auto handle = reinterpret_cast<sys::HANDLE>(_Handle);
  auto overlapped = sys::OVERLAPPED{};
  sys::UnlockFileEx(handle, 0, sys::MAXDWORD, sys::MAXDWORD, &overlapped);
  sys::CloseHandle(handle);
#else
  auto const fd = static_cast<int>(_Handle);
  sys::flock(fd, sys::LOCK_UN);
  sys::close(fd);
#endif

  _Handle = -1;
}

/// [CHECKED]
auto spp::utils::files::FileLock::_Acquire(
  Path const &path, const bool exclusive) -> bool {
  // A second lock from the same object would leak the first one's file, and on
  // Windows it would also deadlock against itself.
  Unlock();

#if SPP_PLATFORM_WINDOWS
  // OPEN_ALWAYS is O_CREAT: open the file, creating an empty one if it is not
  // there.
  auto const handle = sys::CreateFileW(
    path.c_str(), sys::GENERIC_READ | sys::GENERIC_WRITE,
    sys::FILE_SHARE_READ | sys::FILE_SHARE_WRITE, nullptr,
    sys::OPEN_ALWAYS, sys::FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == sys::INVALID_HANDLE_VALUE) { return false; }

  // The range is the largest one expressible, which is how LockFileEx spells
  // "the whole file" for a file that may still grow.
  auto overlapped = sys::OVERLAPPED{};
  auto const flags = exclusive ? sys::LOCKFILE_EXCLUSIVE_LOCK : sys::DWORD{0};
  if (not sys::LockFileEx(
    handle, flags, 0, sys::MAXDWORD, sys::MAXDWORD, &overlapped)) {
    sys::CloseHandle(handle);
    return false;
  }

  _Handle = reinterpret_cast<std::intptr_t>(handle);
  return true;
#else
  // Open the file to get the file descriptor. If it is invalid (-1), then
  // return early.
  const auto fd = sys::open(path.c_str(), sys::O_RDWR | sys::O_CREAT, 0644);
  if (fd < 0) { return false; }

  // Lock the file using the correct lock type based on the boolean flag passed
  // into the function. Should the lock fail, close the file and return.
  if (sys::flock(fd, exclusive ? sys::LOCK_EX : sys::LOCK_SH) != 0) {
    sys::close(fd);
    return false;
  }

  // As the lock passed, save the handle to the lock class and return true. All
  // operations can now be done on this locked file.
  _Handle = fd;
  return true;
#endif
}

SPP_MOD_END
