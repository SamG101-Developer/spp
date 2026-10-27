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
#include <stdlib.h>  // setenv, unsetenv; _dupenv_s, _putenv_s

module spp.utils.files;
import genex;
import std;

auto spp::utils::files::DisplayString(
  Path const &path)
  -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.display_string();
#else
  return path.string();
#endif
}

auto spp::utils::files::NativeString(
  Path const &path)
  -> Str {
#if _GLIBCXX_RELEASE >= 17
  return path.native_encoded_string();
#else
  return path.string();
#endif
}

auto spp::utils::files::ReadFile(
  Path const &path)
  -> Str {
  // Create an input file stream.
  auto in = std::ifstream(path, std::ios::in);
  auto buf = std::ostringstream();
  buf << in.rdbuf();
  return buf.str();
}

auto spp::utils::files::WriteFile(
  Path const &path,
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
  Path const &path,
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

  _Handle = reinterpret_cast<std::intptr_t>(handle);
  return true;
#else
  auto const fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) { return false; }

  if (::flock(fd, exclusive ? LOCK_EX : LOCK_SH) != 0) {
    ::close(fd);
    return false;
  }

  _Handle = fd;
  return true;
#endif
}

auto spp::utils::files::FileLock::LockShared(
  Path const &path)
  -> bool {
  return Acquire(path, false);
}

auto spp::utils::files::FileLock::LockExclusive(
  Path const &path)
  -> bool {
  return Acquire(path, true);
}

auto spp::utils::files::FileLock::Unlock()
  -> void {
  if (_Handle == -1) { return; }

#if SPP_PLATFORM_WINDOWS
  auto const handle = reinterpret_cast<HANDLE>(_Handle);
  auto overlapped = OVERLAPPED{};
  ::UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped);
  ::CloseHandle(handle);
#else
  auto const fd = static_cast<int>(_Handle);
  ::flock(fd, LOCK_UN);
  ::close(fd);
#endif

  _Handle = -1;
}
SPP_MOD_END

auto spp::utils::files::GlobSpp(
  Path const &path)
  -> Vec<Path> {
  // Use the filesystem iterator to recursively walk the path,
  // finding all ".spp" files.
  auto paths = Vec<Path>();
  for (auto const &entry : std::filesystem::recursive_directory_iterator(path)) {
    if (not entry.is_regular_file()) { continue; }
    if (entry.path().extension() != ".spp") { continue; }
    paths.emplace_back(entry.path());
  }

  // Normalise file order across machines for consistent erroring
  // although file and declarations dhould be order agnostic.
  paths |= genex::actions::sort;
  return paths;
}

auto spp::utils::files::SharedLibraryName(
  const StrView package, const StrView extension)
  -> Str {
  const auto prefix = extension == "dll" ? "" : "lib";
  return prefix + Str(package) + "." + Str(extension);
}

namespace {
  /// The directories on PATH, in order.
  auto PathDirectories() -> spp::Vec<spp::Path> {
    auto dirs = spp::Vec<spp::Path>();
    const auto raw = spp::utils::files::GetEnv("PATH");
    if (not raw.has_value()) { return dirs; }
    auto rest = spp::StrView(*raw);
    while (not rest.empty()) {
      const auto end = rest.find(spp::utils::files::PathListSeparator);
      const auto dir = rest.substr(0, end);
      if (not dir.empty()) { dirs.emplace_back(dir); }
      rest = end == spp::StrView::npos ? spp::StrView() : rest.substr(end + 1);
    }
    return dirs;
  }

  /// Whether "path" is a file this process may run.
  auto IsExecutable(spp::Path const &path) -> bool {
    auto ec = std::error_code();
    if (not std::filesystem::is_regular_file(path, ec)) { return false; }
#if SPP_PLATFORM_WINDOWS
    return true;
#else
    return access(path.c_str(), X_OK) == 0;
#endif
  }
}

auto spp::utils::files::FindProgram(
  const StrView name)
  -> Opt<Path> {
  for (auto const &dir : PathDirectories()) {
    if (const auto candidate = dir / name; IsExecutable(candidate)) { return candidate; }
  }
  return Nullopt;
}

auto spp::utils::files::FindNewestVersion(
  const StrView name)
  -> Opt<Path> {
  const auto prefix = Str(name) + "-";
  auto best = Opt<Path>();
  auto best_version = 0UL;
  for (auto const &dir : PathDirectories()) {
    auto ec = std::error_code();
    for (auto it = std::filesystem::directory_iterator(dir, ec); not ec and it != std::filesystem::directory_iterator();
         it.increment(ec)) {
      const auto file = NativeString(it->path().filename());
      if (not file.starts_with(prefix) or file.size() == prefix.size()) { continue; }
      const auto digits = StrView(file).substr(prefix.size());
      if (not genex::all_of(digits, [](const char c) { return c >= '0' and c <= '9'; })) { continue; }
      const auto version = std::stoul(Str(digits));
      if ((best.has_value() and version <= best_version) or not IsExecutable(it->path())) { continue; }
      best = it->path();
      best_version = version;
    }
  }
  return best;
}

auto spp::utils::files::GetEnv(
  const StrView name)
  -> Opt<Str> {
#if SPP_PLATFORM_WINDOWS
  // MSVC deprecates getenv for its shared static buffer.
  char *raw = nullptr;
  auto size = std::size_t{0};
  if (_dupenv_s(&raw, &size, Str(name).c_str()) != 0 or raw == nullptr) { return Nullopt; }
  auto value = Str(raw);
  std::free(raw);
  return value;
#else
  auto const *const raw = std::getenv(Str(name).c_str());
  if (raw == nullptr) { return Nullopt; }
  return Str(raw);
#endif
}

auto spp::utils::files::SetEnv(
  const StrView name,
  const StrView value)
  -> void {
#if SPP_PLATFORM_WINDOWS
  _putenv_s(Str(name).c_str(), Str(value).c_str());
#else
  setenv(Str(name).c_str(), Str(value).c_str(), 1);
#endif
}

auto spp::utils::files::UnsetEnv(
  const StrView name)
  -> void {
#if SPP_PLATFORM_WINDOWS
  _putenv_s(Str(name).c_str(), "");
#else
  unsetenv(Str(name).c_str());
#endif
}
