module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>

module spp.utils.shell;
import spp.utils.env;
import spp.utils.files;
import genex;
import std;
import sys;

namespace fs = std::filesystem;

namespace spp::utils::shell {
  /// The directories on PATH, in order. Split by the given separator (which
  /// is platform dependent).
  static auto PathDirectories() -> Vec<Path>;

  /// Whether "path" is a file this process may run. On windows, provided the
  /// file is regular, it's always executable (sys::X_OK is "exists").
  static auto IsExecutable(Path const &path) -> bool;

  /// "path" if it is a program this process may run, or on Windows
  /// "path.exe" when "path" has no extension, as a shell finds "lld-link" as
  /// "lld-link.exe".
  static auto RunnableAt(Path const &path) -> Opt<Path>;

  /// The program a command starts with, without the quotes around it if it
  /// is quoted ("..." or '...'): up to the first space or tab otherwise.
  static auto ProgramOf(StrView command) -> Str;
}


auto spp::utils::shell::Quote(const StrView arg) -> Str {
  auto out = Str();
#if SPP_PLATFORM_WINDOWS
  // Backslashes are only special where they lead up to a double quote, so those
  // are doubled, including the ones before the closing quote.
  out += '"';
  auto slashes = 0uz;
  for (const auto c : arg) {
    if (c == '\\') {
      ++slashes;
      continue;
    }
    out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
    out += c;
    slashes = 0;
  }
  out.append(slashes * 2, '\\');
  out += '"';
#else
  // Nothing is special inside single quotes, except a single quote, which has
  // to step outside them.
  out += '\'';
  for (const auto c : arg) {
    if (c == '\'') { out += "'\\''"; }
    else { out += c; }
  }
  out += '\'';
#endif
  return out;
}

auto spp::utils::shell::Run(Str const &command) -> int {
  // "cmd /c" takes a leading quote and the last quote off a line that starts
  // with one, unless the line is simple enough, so such a line is given an
  // outer pair for it to take.
#if SPP_PLATFORM_WINDOWS
  if (command.starts_with('"')) {
    return std::system(("\"" + command + "\"").c_str());
  }
#endif
  return std::system(command.c_str());
}

/// [CHECKED]
auto spp::utils::shell::PathDirectories() -> Vec<Path> {
  // Get the PATH from the environment. If it is empty, then no extra
  // processing needs to happen. Return early.
  auto dirs = spp::Vec<Path>();
  const auto raw = env::Get(env::Var::Path);
  if (not raw.has_value()) { return dirs; }

  // Move through the "rest" which is a view over the "raw" PATH value, and
  // get the next path (upto the separator), and then trim and continue.
  auto rest = StrView(*raw);
  while (not rest.empty()) {
    const auto end = rest.find(PathListSeparator);
    const auto dir = rest.substr(0, end);
    if (not dir.empty()) { dirs.emplace_back(dir); }
    rest = end == StrView::npos ? StrView() : rest.substr(end + 1);
  }
  return dirs;
}


/// [CHECKED]
auto spp::utils::shell::IsExecutable(Path const &path) -> bool {
  auto ec = std::error_code();
  // Run the two checks (regular file, and access control).
  return fs::is_regular_file(path, ec)
    and sys::access(path.c_str(), sys::X_OK) == 0;
}


auto spp::utils::shell::RunnableAt(Path const &path) -> Opt<Path> {
  if (IsExecutable(path)) { return path; }
#if SPP_PLATFORM_WINDOWS
  if (not path.has_extension()) {
    if (auto exe = Path(path) += ".exe"; IsExecutable(exe)) { return exe; }
  }
#endif
  return Nullopt;
}


auto spp::utils::shell::ProgramOf(StrView command) -> Str {
  const auto start = command.find_first_not_of(" \t");
  if (start == StrView::npos) { return Str(); }
  command.remove_prefix(start);
  if (command.front() == '"' or command.front() == '\'') {
    const auto close = command.find(command.front(), 1);
    return Str(command.substr(1, close == StrView::npos ? StrView::npos : close - 1));
  }
  return Str(command.substr(0, command.find_first_of(" \t")));
}


/// [CHECKED]
auto spp::utils::shell::FindProgram(const StrView name) -> Opt<Path> {
  // Loop through the PATH directories, looking for one that contains the given
  // filename. Return the qualified filename (+directory) if found.
  for (auto const &dir : PathDirectories()) {
    if (auto found = RunnableAt(dir / name); found.has_value()) { return found; }
  }
  return Nullopt;
}


auto spp::utils::shell::FindCommand(const StrView command) -> Opt<Str> {
  // A path to a program and nothing else, spaces and all: quoted whole, so the
  // shell does not split it. A bare name is a program on PATH, never a file in
  // the current directory, as the shell takes it.
  if (Path(command).has_parent_path()) {
    if (const auto whole = RunnableAt(Path(command)); whole.has_value()) {
      return Quote(files::NativeString(*whole));
    }
  }

  // Otherwise a program and its arguments, the program found as the shell
  // would: where it says when it names a directory, else on PATH.
  const auto program = ProgramOf(command);
  if (program.empty()) { return Nullopt; }
  const auto found = Path(program).has_parent_path() ? RunnableAt(Path(program)) : FindProgram(program);
  return found.has_value() ? Opt<Str>(command) : Nullopt;
}


/// [CHECKED]
auto spp::utils::shell::FindNewestVersion(const StrView name) -> Opt<Path> {
  // The prefix is the start of the filename, ie "loongarch64-linux-gnu-gcc"
  // will become "loongarch64-linux-gnu-gcc-" and the version will be found
  // afterwards.
  const auto prefix = Str(name) + "-";
  auto best = Opt<Path>();
  auto best_version = 0UL;
  const auto is_digit = [](const char c) { return c >= '0' and c <= '9'; };

  // Use the path directories again, as this is the shell-searchable set of
  // directories.
  for (auto const &dir : PathDirectories()) {
    auto ec = std::error_code();
    for (
      auto it = fs::directory_iterator(dir, ec);
      not ec and it != fs::directory_iterator();
      it.increment(ec)) {
      // Filter out files that don't start with the same prefix (irrelevant), or
      // don't have anything after the prefix (non-versioned name).
      const auto file = files::NativeString(it->path().filename());
      if (not file.starts_with(prefix)) { continue; }
      if (file.size() == prefix.size()) { continue; }

      // Get the suffix digits following the prefix part of the filename, and
      // check the version is numeric.
      const auto digits = StrView(file).substr(prefix.size());
      if (not genex::all_of(digits, is_digit)) {
        continue;
      }

      // Parse the version into a number, and compare it against the current
      // best version of the file. Lower version or non-executable is skipped.
      const auto version = std::stoul(Str(digits));
      if (best.has_value() and version <= best_version) { continue; }
      if (not IsExecutable(it->path())) { continue; }
      best = it->path();
      best_version = version;
    }
  }
  return best;
}
