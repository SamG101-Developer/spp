module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <spp/version.hpp>

module spp.cli;
import spp.utils.env;
import spp.utils.files;
import spp.utils.shell;
import spp.utils.types;
import llvm;
import std;

/// The "spp self" commands, which manage this spp rather than a project.
/// "install" copies it to ~/.spp/bin and puts that on PATH. "update" replaces
/// it with a release: GitHub's (or SPP_UPDATE_BASE_URL's, laid out the same
/// way), the latest from the "releases/latest" redirect or the one asked for,
/// whose zip ("spp-<label>.zip", holding "spp"/"spp.exe") is checked against
/// its SHA256SUMS, unpacked, and moved over this binary. "uninstall" removes
/// it, and what "install" set up. Everything that reaches the network or
/// unpacks goes through the shell (curl, unzip or tar), as "spp vcs" does with
/// git; spp has no client of its own.

namespace spp::cli {
  /// Where releases are published: the repository whose releases spp's
  /// release pipeline creates.
  static constexpr auto kReleaseRepository =
    StrView("https://github.com/SamG101-Developer/spp");

  /// The first release published as zips, which is what an update downloads;
  /// one before it has no "spp-<label>.zip" to fetch.
  static constexpr auto kFirstZippedRelease =
    StrView("0.7.0");

  /// The line "spp self install" adds to a shell's start-up file, and
  /// "spp self uninstall" takes back out: it reads ~/.spp/env, which puts
  /// the install directory on PATH.
  static constexpr auto kShellLine =
    StrView(". \"$HOME/.spp/env\" # added by \"spp self install\"");

  namespace {
    /// A scratch directory, removed with everything in it when the update is
    /// done, whichever way it ends.
    struct ScratchDir {
      Path Dir;
      ScratchDir() {
        // Random temp location for the update.
        auto random = std::random_device();
        Dir = std::filesystem::temp_directory_path() / ("spp-update-" + std::to_string(random()));
        std::filesystem::create_directories(Dir);
      }
      ~ScratchDir() {
        // Clean the temp folder up.
        auto ec = std::error_code();
        std::filesystem::remove_all(Dir, ec);
      }
      ScratchDir(ScratchDir const &) = delete;
      auto operator=(ScratchDir const &) -> ScratchDir& = delete;
    };
  }

  /// The releases' base URL, without a trailing "/", and whether it is the
  /// default (which is only ever reached over https). This is required so that
  /// the tests can make mock github servers to check the "self" suite works.
  static auto ReleaseBase() -> Pair<Str, bool>;

  /// A curl command for "args": failing on an HTTP error, following
  /// redirects, retrying a dropped connection, and over https alone unless
  /// SPP_UPDATE_BASE_URL points elsewhere (a test's local server).
  static auto Curl(
    Str const &args, bool https_only) -> Str;

  /// Run "command" with its errors written to "log"; on failure, report
  /// "what" failing and show them.
  static auto RunLogged(
    Str const &command, Path const &log, StrView what) -> bool;

  /// A version's numbers ("0.7.1" -> {0, 7, 1}), or none for anything that
  /// is not a dotted run of whole numbers. Simple 3 number parser.
  static auto VersionNumbers(
    StrView version) -> Opt<Vec<std::uint64_t>>;

  /// The latest release's version: where "<base>/releases/latest" redirects
  /// to ("<base>/releases/tag/v0.7.1"), so no API is asked, and no rate limit
  /// met.
  static auto LatestVersion(
    ScratchDir const &scratch, Str const &base, bool https_only) -> Opt<Str>;

  /// Whether "<base>/releases/tag/v<version>" exists: a release asked for by
  /// "--version" is told apart from a download that failed.
  static auto ReleaseExists(
    ScratchDir const &scratch, Str const &base, Str const &version,
    bool https_only) -> bool;

  /// "data"'s SHA-256, as lower-case hex.
  static auto Sha256Hex(StrView data) -> Str;

  /// Whether "zip" is the one SHA256SUMS lists under "name". Both come from
  /// the same release, so this catches a corrupt or cut-off download, not a
  /// release that was tampered with (which its attestation is for).
  static auto MatchesChecksum(
    Path const &zip, Path const &sums, StrView name) -> bool;

  /// Unpack "entry" from "zip" into "into": with unzip on Linux, and with the
  /// system tar, which is bsdtar and reads a zip, on macOS and Windows.
  static auto Unpack(
    Path const &zip, StrView entry, Path const &into, Path const &log) -> bool;

  /// The running spp, its symbolic links followed, so the update replaces
  /// the file itself rather than a link to it. Prefer the absolute path to the
  /// relative one.
  static auto RunningExecutable() -> Opt<Path>;

  /// If the S++ compiler was installed via another package manager, don't allow
  /// the raw tool to update/uninstall itself - defer to the package manager.
  /// This if future proofing.
  static auto ManagedInstall(Path const &exe) -> Opt<Str>;

  /// Put "fresh" in place of "exe". It is copied beside "exe" first, so the
  /// final move stays on one file system and is a rename. On Linux and macOS
  /// the rename replaces the file at once, and this process runs on from the
  /// old one; Windows will not replace a running executable, but will rename
  /// it, so the old one is moved aside ("spp.exe.old", removed at the next
  /// start) and the new one moved in.
  static auto Replace(Path const &exe, Path const &fresh) -> bool;

  /// spp's own directory for what "spp self install" writes: ~/.spp, or
  /// %LOCALAPPDATA%\spp on Windows. None when that variable is not set.
  static auto SppHome() -> Opt<Path>;

  /// Whether "a" and "b" are the same file (both existing).
  static auto SameFile(Path const &a, Path const &b) -> bool;

  /// Whether two files hold the same bytes.
  static auto SameContent(Path const &a, Path const &b) -> bool;

#if SPP_PLATFORM_WINDOWS
  /// Run a PowerShell script on the user's Path, from the registry so that
  /// entries written as %VARIABLE% stay unexpanded (as [Environment]'s
  /// getter would not keep them), and written back as an expandable string.
  /// Setting a throwaway user variable afterwards broadcasts the change, so
  /// programs started from now on see it. "$d" is "entry", an entry's text.
  static auto RunOnUserPath(StrView entry, StrView change) -> bool;

  /// The user Path's entries, each as stored (a %VARIABLE% unexpanded) beside
  /// it expanded, which is the directory it names.
  static auto UserPathEntries() -> Vec<Pair<Str, Str>>;
#endif

  /// Put "dir" on the user's PATH, for shells started from now on: on
  /// Windows the user's Path, first; elsewhere ~/.spp/env (which puts it
  /// first, once), read from each shell start-up file that exists, or from
  /// ~/.profile (and ~/.zshrc on macOS, whose shell is zsh) when none does;
  /// and a fish snippet where fish is set up. What it did, one line each.
  static auto AddToPath(Path const &dir, Path const &home) -> Vec<Str>;

  /// Take back what "AddToPath" did for "dir", where it did it. The setup is
  /// found by the directory it names, not by its spelling: install records
  /// the directory as the environment spells it, which can differ from the
  /// running spp's own path, resolved (a symbolic link in HOME, as macOS's
  /// /var is; an 8.3 short name in LOCALAPPDATA, as GitHub's Windows runners'
  /// is). What it undid, one line each.
  static auto RemoveFromPath(Path const &dir, Path const &home) -> Vec<Str>;
}

/// [CHECKED]
auto spp::cli::ReleaseBase() -> Pair<Str, bool> {
  // Get the base url. This is either the main GitHub repo, or a mock server for
  // unit tests. No base means default to the release repository.
  auto base = utils::env::Get(utils::env::Var::UpdateBaseUrl);
  if (not base.has_value() or base->empty()) { return {Str(kReleaseRepository), true}; }

  // Strip off the trailing "/" characters, and return the custom server base
  // with "false" (not default).
  while (base->ends_with('/')) { base->pop_back(); }
  return {*base, false};
}

/// [CHECKED]
auto spp::cli::Curl(Str const &args, const bool https_only) -> Str {
  // Secure "curl" runner for https to the release server, otherwise allow http
  // to a local mock server.
  const auto protocols = https_only
    ? Str("--proto =https --proto-redir =https --tlsv1.2")
    : Str("--proto =https,http --proto-redir =https,http");
  return "curl --fail --silent --show-error --location --retry 3 " + protocols + " " + args;
}

/// [CHECKED]
auto spp::cli::RunLogged(
  Str const &command, Path const &log, const StrView what) -> bool {
  // Run the command into the log file. Enforce a status of 0 to ensure it was
  // successful.
  const auto status = utils::shell::Run(
    command + " 2> " + utils::shell::Quote(utils::files::NativeString(log)));
  if (status == 0) { return true; }

  // Collect any errors from the log file.
  auto errors = Str();
  if (auto in = std::ifstream(log); in) { errors.assign(std::istreambuf_iterator(in), {}); }
  while (not errors.empty() and std::isspace(static_cast<unsigned char>(errors.back()))) { errors.pop_back(); }
  std::cerr
    << "Error: " << what << " failed (" << status << ")"
    << (errors.empty() ? "" : ": ") << errors << "\n";
  return false;
}

/// [CHECKED]
auto spp::cli::VersionNumbers(StrView version) -> Opt<Vec<std::uint64_t>> {
  // Start with an empty version vector and begin iterating until the end of the
  // string has been reached.
  auto numbers = Vec<std::uint64_t>();
  while (true) {
    // Find the first dot, and the numeric part upto that dot. Go from 0 because
    // we substring the "version" string at the end of the loop.
    const auto dot = version.find('.');
    const auto part = version.substr(0, dot);
    auto number = std::uint64_t{0};
    const auto [end, ec] = std::from_chars(
      part.data(), part.data() + part.size(), number);

    // If the "part" is empty / there is an error code, then the version is
    // invalid, so return Nullopt.
    if (part.empty() or ec != std::errc{} or end != part.data() + part.size()) {
      return Nullopt;
    }

    // Store the number, and if the "." is the end of the string, then return
    // the number vector. This allows for any amount of numbers. Strip the
    // parsed part from the version and continue iterating.
    numbers.push_back(number);
    if (dot == StrView::npos) { return numbers; }
    version.remove_prefix(dot + 1);
  }
}

/// [CHECKED]
auto spp::cli::LatestVersion(
  ScratchDir const &scratch, Str const &base,
  const bool https_only) -> Opt<Str> {
  // Get the latest version from the releases page.
  const auto where = scratch.Dir / "latest.txt";
  const auto command = Curl("--head --output "
    + utils::shell::Quote(utils::files::NativeString(scratch.Dir / "latest.head")) + " --write-out \"%{url_effective}\" "
    + utils::shell::Quote(base + "/releases/latest") + " > "
    + utils::shell::Quote(utils::files::NativeString(where)), https_only);
  if (not RunLogged(command, scratch.Dir / "latest.err", "finding the latest release")) { return Nullopt; }

  // The output was stored into the "where" file, so read the url back out of
  // it, parse it, check for parsing errors (failsafe), and return the version.
  const auto url = utils::files::ReadBytes(where).value_or("");
  constexpr auto kTag = StrView("/releases/tag/v");
  const auto at = url.rfind(kTag);
  auto version = at == Str::npos ? Str() : url.substr(at + kTag.size());
  if (not VersionNumbers(version).has_value()) {
    std::cerr
      << "Error: the latest release could not be read from '"
      << url << "'.\n";
    return Nullopt;
  }
  return version;
}

auto spp::cli::ReleaseExists(
  ScratchDir const &scratch, Str const &base, Str const &version, const bool https_only) -> bool {
  const auto command = Curl(
    "--head --output " + utils::shell::Quote(utils::files::NativeString(scratch.Dir / "tag.head")) + " "
    + utils::shell::Quote(base + "/releases/tag/v" + version), https_only);
  return utils::shell::Run(command + " 2> " + utils::shell::Quote(
    utils::files::NativeString(scratch.Dir / "tag.err"))) == 0;
}

/// [CHECKED]
auto spp::cli::Sha256Hex(const StrView data) -> Str {
  // Make a digest over the inputted data, and format it in hex for the output
  // string. SHA256 => 32 bytes.
  const auto digest = llvm::SHA256::hash(llvm::ArrayRef(
    reinterpret_cast<std::uint8_t const*>(data.data()), data.size()));
  auto hex = Str();
  for (const auto byte : digest) { hex += std::format("{:02x}", byte); }
  return hex;
}

/// [CHECKED]
auto spp::cli::MatchesChecksum(
  Path const &zip, Path const &sums, const StrView name) -> bool {
  // Read the hashsum file and the zip file.
  const auto listed = utils::files::ReadBytes(sums);
  const auto data = utils::files::ReadBytes(zip);
  if (not listed.has_value() or not data.has_value()) {
    std::cerr << "Error: the downloaded release could not be read.\n";
    return false;
  }

  // Compare the computed hashes against the pre-computed hashes (from GitHub),
  // and check that they all match.
  auto lines = std::istringstream(*listed);
  for (auto line = Str(); std::getline(lines, line);) {
    if (line.ends_with('\r')) { line.pop_back(); }
    if (line.size() > 66 and line.substr(66) == name) {
      const auto expected = line.substr(0, 64);
      if (Sha256Hex(*data) == expected) { return true; }
      std::cerr
        << "Error: " << name
        << " does not match the release's SHA256SUMS; nothing was replaced.\n";
      return false;
    }
  }
  std::cerr
    << "Error: the release's SHA256SUMS does not list " << name << ".\n";
  return false;
}

/// [CHECKED]
auto spp::cli::Unpack(
  Path const &zip, const StrView entry, Path const &into,
  Path const &log) -> bool {
  // Typically this unpacks the zip into the "/unpack" scratch directory, with
  // the entry being "spp" or "spp.exe" (platform dependent).
  const auto zip_arg = utils::shell::Quote(utils::files::NativeString(zip));
  const auto into_arg = utils::shell::Quote(utils::files::NativeString(into));
  const auto entry_arg = utils::shell::Quote(entry);

  // For Windows, find the "tar.exe" in system32; macOS can use the "tar"
  // command, and Linux the "unzip" (already checked it exists).
#if SPP_PLATFORM_WINDOWS
  const auto command = Str("\"%SystemRoot%\\System32\\tar.exe\" -xf ")
    + zip_arg + " -C " + into_arg + " " + entry_arg;
#elif SPP_PLATFORM_MACOS
  const auto command = "tar -xf " + zip_arg
    + " -C " + into_arg + " " + entry_arg;
#else
  const auto command = "unzip -o -q " + zip_arg + " " + entry_arg
    + " -d " + into_arg;
#endif
  if (not RunLogged(command, log, "unpacking the release")) { return false; }
  return std::filesystem::is_regular_file(into / entry);
}

/// [CHECKED]
auto spp::cli::RunningExecutable() -> Opt<Path> {
  // Run the LLVM call to get the executable based on the argv input. Shouldn't
  // fail, but consider it might with a failsafe.
  const auto path = llvm::sys::fs::getMainExecutable(nullptr, nullptr);
  if (path.empty()) { return Nullopt; }

  // Canonicalise the filepath (get the absolute based off of the current
  // working directory), and return the absolute path, or if that failed, the
  // relative one.
  auto ec = std::error_code();
  auto resolved = std::filesystem::canonical(path, ec);
  return ec ? Path(path) : resolved;
}

/// [CHECKED]
auto spp::cli::ManagedInstall(Path const &exe) -> Opt<Str> {
  // Check the absolute path of the executable for certain folders that indicate
  // that a package manager installed this exe. If so, return the manager. If
  // Nullopt is returned, then this is not a managed install.
  const auto text = exe.generic_string();
  if (text.find("/Cellar/") != Str::npos) { return Str("Homebrew (brew upgrade)"); }
  if (text.starts_with("/nix/store/")) { return Str("Nix"); }
  return Nullopt;
}

auto spp::cli::Replace(Path const &exe, Path const &fresh) -> bool {
  const auto staged = exe.parent_path() / (".spp-update-" + exe.filename().string());
  auto ec = std::error_code();
  std::filesystem::copy_file(fresh, staged, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    std::cerr
      << "Error: cannot write to '" << utils::files::NativeString(exe.parent_path())
      << "' (" << ec.message() << "). Run it as a user who can, or install spp "
      << "in a directory you own (spp self install).\n";
    return false;
  }

  // Windows doesn't allow for the self-replacement of a running executable, so
  // we have so load it on the side and rename it.
#if SPP_PLATFORM_WINDOWS
  if (not std::filesystem::exists(exe)) {
    std::filesystem::rename(staged, exe, ec);
  }
  else {
    const auto aside = Path(exe.native() + L".old");
    std::filesystem::remove(aside, ec);
    std::filesystem::rename(exe, aside, ec);
    if (not ec) {
      std::filesystem::rename(staged, exe, ec);
      if (ec) {
        auto undo = std::error_code();
        std::filesystem::rename(aside, exe, undo);
      }
    }
  }
#else
  std::filesystem::permissions(
    staged, static_cast<std::filesystem::perms>(0755), ec);
  if (not ec) { std::filesystem::rename(staged, exe, ec); }
#endif

  // Clean-up if the replacement errored; remove the staged file (capturing a
  // possible followup error code), and display the replacement error.
  if (ec) {
    auto cleanup = std::error_code();
    std::filesystem::remove(staged, cleanup);
    std::cerr
      << "Error: could not replace '" << utils::files::NativeString(exe)
      << "': " << ec.message() << "\n";
    return false;
  }
  return true;
}

/// [CHECKED]
auto spp::cli::SppHome() -> Opt<Path> {
  const auto root = utils::env::Get(utils::env::Var::UserDir);
  if (not root.has_value() or root->empty()) { return Nullopt; }
  return Path(*root) / (SPP_PLATFORM_WINDOWS ? "spp" : ".spp");
}

auto spp::cli::SameFile(Path const &a, Path const &b) -> bool {
  auto ec = std::error_code();
  return std::filesystem::equivalent(a, b, ec) and not ec;
}

auto spp::cli::SameContent(Path const &a, Path const &b) -> bool {
  const auto first = utils::files::ReadBytes(a);
  const auto second = utils::files::ReadBytes(b);
  return first.has_value() and second.has_value() and *first == *second;
}

#if SPP_PLATFORM_WINDOWS
auto spp::cli::RunOnUserPath(const StrView entry, const StrView change) -> bool {
  auto quoted = Str(entry);
  for (auto at = quoted.find('\''); at != Str::npos; at = quoted.find('\'', at + 2)) { quoted.insert(at, 1, '\''); }
  const auto script = "$d='" + quoted + "'; "
    "$k=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment',$true); "
    "$p=$k.GetValue('Path','',[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames); "
    "$parts=@($p -split ';' | Where-Object { $_ -ne '' }); " + Str(change) + " "
    "[Environment]::SetEnvironmentVariable('spp_path_refresh','1','User'); "
    "[Environment]::SetEnvironmentVariable('spp_path_refresh',$null,'User')";
  return utils::shell::Run("powershell -NoProfile -NonInteractive -Command \"" + script + "\"") == 0;
}

auto spp::cli::UserPathEntries() -> Vec<Pair<Str, Str>> {
  // Written one entry a line, as stored and expanded with a tab between, in
  // UTF-8 so that a path outside the console's code page survives.
  auto random = std::random_device();
  const auto listing = std::filesystem::temp_directory_path() / ("spp-user-path-" + std::to_string(random()) + ".txt");
  const auto script = Str("[Console]::OutputEncoding=[Text.Encoding]::UTF8; "
    "$k=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment'); "
    "$p=$k.GetValue('Path','',[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames); "
    "foreach ($e in ($p -split ';')) { "
    "if ($e -ne '') { $e + [char]9 + [Environment]::ExpandEnvironmentVariables($e) } }");
  auto entries = Vec<Pair<Str, Str>>();
  if (utils::shell::Run("powershell -NoProfile -NonInteractive -Command \"" + script + "\" > "
    + utils::shell::Quote(utils::files::NativeString(listing))) == 0) {
    auto lines = std::istringstream(utils::files::ReadBytes(listing).value_or(""));
    for (auto line = Str(); std::getline(lines, line);) {
      if (line.ends_with('\r')) { line.pop_back(); }
      if (const auto tab = line.find('\t'); tab != Str::npos) {
        entries.emplace_back(line.substr(0, tab), line.substr(tab + 1));
      }
    }
  }
  auto ec = std::error_code();
  std::filesystem::remove(listing, ec);
  return entries;
}
#endif

auto spp::cli::AddToPath(Path const &dir, Path const &home) -> Vec<Str> {
  auto done = Vec<Str>();
#if SPP_PLATFORM_WINDOWS
  static_cast<void>(home);
  if (RunOnUserPath(utils::files::NativeString(dir),
    "if ($parts -notcontains $d) { $k.SetValue('Path', ((@($d) + $parts) -join ';'), "
    "[Microsoft.Win32.RegistryValueKind]::ExpandString) };")) {
    done.push_back("added " + utils::files::NativeString(dir) + " to your user Path");
  }
  else {
    done.push_back("could not change your user Path; add " + utils::files::NativeString(dir) + " to it");
  }
#else
  const auto shown = utils::files::NativeString(dir);
  utils::files::WriteFile(home / "env",
    "# Added by \"spp self install\" (removed by \"spp self uninstall\"): puts spp on PATH, once.\n"
    "case \":${PATH}:\" in\n"
    "  *:\"" + shown + "\":*) ;;\n"
    "  *) export PATH=\"" + shown + ":${PATH}\" ;;\n"
    "esac\n");
  const auto user = home.parent_path();
  auto files = Vec<Path>();
  for (const auto name : {".profile", ".bash_profile", ".bashrc", ".zshrc"}) {
    if (std::filesystem::exists(user / name)) { files.push_back(user / name); }
  }
  if (files.empty()) {
    files.push_back(user / ".profile");
#if SPP_PLATFORM_MACOS
    files.push_back(user / ".zshrc");
#endif
  }
  for (auto const &file : files) {
    auto text = std::filesystem::exists(file) ? utils::files::ReadFile(file) : Str();
    if (text.find(kShellLine) != Str::npos) { continue; }
    if (not text.empty() and not text.ends_with('\n')) { text += '\n'; }
    utils::files::WriteFile(file, text + Str(kShellLine) + "\n");
    done.push_back("added a line to " + utils::files::NativeString(file));
  }
  if (std::filesystem::is_directory(user / ".config" / "fish")) {
    const auto fish = user / ".config" / "fish" / "conf.d" / "spp.fish";
    std::filesystem::create_directories(fish.parent_path());
    utils::files::WriteFile(fish, "# Added by \"spp self install\" (removed by \"spp self uninstall\").\n"
      "fish_add_path --global --prepend \"" + shown + "\"\n");
    done.push_back("wrote " + utils::files::NativeString(fish));
  }
#endif
  return done;
}

auto spp::cli::RemoveFromPath(Path const &dir, Path const &home) -> Vec<Str> {
  auto done = Vec<Str>();
#if SPP_PLATFORM_WINDOWS
  static_cast<void>(home);
  for (auto const &[stored, expanded] : UserPathEntries()) {
    if (not SameFile(Path(std::u8string(expanded.begin(), expanded.end())), dir)) { continue; }
    if (RunOnUserPath(stored, "$kept=@($parts | Where-Object { $_ -ne $d }); if ($kept.Count -ne $parts.Count) { "
      "$k.SetValue('Path', ($kept -join ';'), [Microsoft.Win32.RegistryValueKind]::ExpandString) };")) {
      done.push_back("took " + stored + " out of your user Path");
    }
  }
#else
  // Only the setup that points at "dir": uninstalling some other copy of
  // spp leaves the installed one on PATH.
  // The directory ~/.spp/env records is in its export line, before ":${PATH}".
  auto ec = std::error_code();
  const auto env_text = std::filesystem::exists(home / "env") ? utils::files::ReadFile(home / "env") : Str();
  constexpr auto kExport = StrView("export PATH=\"");
  const auto from = env_text.find(kExport);
  const auto to = from == Str::npos ? Str::npos : env_text.find(":${PATH}\"", from);
  if (to == Str::npos) { return done; }
  const auto recorded = Path(env_text.substr(from + kExport.size(), to - from - kExport.size()));
  if (not SameFile(recorded, dir)) { return done; }
  if (std::filesystem::remove(home / "env", ec)) {
    done.push_back("removed " + utils::files::NativeString(home / "env"));
  }
  const auto user = home.parent_path();
  for (const auto name : {".profile", ".bash_profile", ".bashrc", ".zshrc"}) {
    const auto file = user / name;
    if (not std::filesystem::exists(file)) { continue; }
    const auto text = utils::files::ReadFile(file);
    if (text.find(kShellLine) == Str::npos) { continue; }
    auto kept = Str();
    auto lines = std::istringstream(text);
    for (auto line = Str(); std::getline(lines, line);) {
      if (line != kShellLine) { kept += line + "\n"; }
    }
    utils::files::WriteFile(file, kept);
    done.push_back("removed its line from " + utils::files::NativeString(file));
  }
  const auto fish = user / ".config" / "fish" / "conf.d" / "spp.fish";
  if (std::filesystem::remove(fish, ec)) { done.push_back("removed " + utils::files::NativeString(fish)); }
#endif
  return done;
}

auto spp::cli::HandleSelfInstall(Opt<Str> const &dir, const bool force, const bool modify_path) -> bool {
  const auto exe = RunningExecutable();
  if (not exe.has_value()) {
    std::cerr << "Error: could not find the running spp's executable.\n";
    return false;
  }
  const auto home = SppHome();
  if (not home.has_value() and (modify_path or not dir.has_value())) {
    std::cerr << "Error: " << utils::env::NameOf(utils::env::Var::UserDir) << " is not set, so there is no home "
      << "for spp; choose a directory with --dir, and --no-modify-path.\n";
    return false;
  }

  // Where it goes, under its plain name whatever the download was called.
  auto ec = std::error_code();
  const auto target_dir = dir.has_value() ? std::filesystem::absolute(Path(*dir), ec) : *home / "bin";
  const auto target = target_dir / (SPP_PLATFORM_WINDOWS ? "spp.exe" : "spp");
  const auto shown = utils::files::NativeString(target);

  // Each of these stops, unless "--force": this spp installed there already;
  // a different file there; another spp on PATH, which either the installed
  // one would hide (it is put first) or which would hide it.
  const auto running_there = SameFile(*exe, target);
  if (not force) {
    if (running_there or (std::filesystem::exists(target) and SameContent(*exe, target))) {
      std::cerr << "Error: spp is already installed at " << shown << " (--force installs it again).\n";
      return false;
    }
    if (std::filesystem::exists(target)) {
      std::cerr << "Error: " << shown << " is a different spp; --force replaces it.\n";
      return false;
    }
    if (const auto other = utils::files::FindProgram("spp"); other.has_value() and not SameFile(*other, target)
      and not SameFile(*other, *exe)) {
      std::cerr << "Error: another spp is on PATH, at " << utils::files::NativeString(*other) << ", which the one "
        << "installed at " << shown << " would replace as 'spp' in new terminals. Remove it, or --force.\n";
      return false;
    }
  }

  // The copy, unless this is it; then the PATH, unless asked not to.
  if (not running_there) {
    std::filesystem::create_directories(target_dir, ec);
    if (not Replace(target, *exe)) { return false; }
#if SPP_PLATFORM_MACOS
    // A browser marks a download, and so this copy, as quarantined; it was run
    // to get here, so the mark has served its purpose.
    ignore = utils::shell::Run("xattr -d com.apple.quarantine " + utils::shell::Quote(shown) + " 2> /dev/null");
#endif
  }
  std::cout << "Installed spp " << SPP_VERSION << " at " << shown << ".\n";
  if (not modify_path) {
    std::cout << "PATH was left alone (--no-modify-path): add " << utils::files::NativeString(target_dir)
      << " to it to run 'spp' from anywhere.\n";
    return true;
  }
  for (auto const &line : AddToPath(target_dir, *home)) { std::cout << "  " << line << "\n"; }
  std::cout << (SPP_PLATFORM_WINDOWS
    ? Str("Open a new terminal to use 'spp'.")
    : "Open a new terminal to use 'spp', or run: . \"" + utils::files::NativeString(*home / "env") + "\"")
    << "\nThe download you ran this from can be deleted; 'spp self update' keeps the installed one current.\n";
  return true;
}

auto spp::cli::HandleSelfUninstall(const bool yes) -> bool {
  const auto exe = RunningExecutable();
  if (not exe.has_value()) {
    std::cerr << "Error: could not find the running spp's executable.\n";
    return false;
  }
  if (const auto manager = ManagedInstall(*exe); manager.has_value()) {
    std::cerr << "Error: this spp was installed by " << *manager << "; uninstall it there.\n";
    return false;
  }

  // Asked first, as nothing brings it back but a reinstall. No answer (no
  // terminal) is a no.
  const auto shown = utils::files::NativeString(*exe);
  if (not yes) {
    std::cout << "Remove " << shown << "? [y/N] " << std::flush;
    auto answer = Str();
    std::getline(std::cin, answer);
    for (auto &c : answer) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    if (answer != "y" and answer != "yes") {
      std::cout << "Nothing was removed.\n";
      return false;
    }
  }

  // Linux and macOS remove a running executable's name at once, and this
  // process runs on from the file until it exits. Windows will not delete a
  // running .exe, but will rename it, so it is moved aside at once (no longer
  // on PATH as "spp") and a detached cmd deletes it once this has exited.
  auto ec = std::error_code();
#if SPP_PLATFORM_WINDOWS
  const auto aside = Path(exe->native() + L".old");
  std::filesystem::remove(aside, ec);
  std::filesystem::rename(*exe, aside, ec);
  if (not ec) {
    const auto quoted = utils::shell::Quote(utils::files::NativeString(aside));
    ignore = utils::shell::Run(
      "start \"\" /b cmd /c \"ping -n 3 127.0.0.1 > nul & del /f /q " + quoted + "\"");
  }
#else
  std::filesystem::remove(*exe, ec);
#endif
  if (ec) {
    std::cerr << "Error: could not remove " << shown << " (" << ec.message() << "). Run it as a user who can "
      << "write to " << utils::files::NativeString(exe->parent_path()) << ".\n";
    return false;
  }

  // What "spp self install" set up goes with it: its PATH changes, and its
  // directories once empty. What is left is the projects': each one's
  // fetched dependencies and build output, which belong to them.
  std::cout << "Removed " << shown << ".\n";
  if (const auto home = SppHome(); home.has_value()) {
    for (auto const &line : RemoveFromPath(exe->parent_path(), *home)) { std::cout << "  " << line << "\n"; }
    if (SameFile(exe->parent_path(), *home / "bin") and std::filesystem::is_empty(*home / "bin", ec)) {
      std::filesystem::remove(*home / "bin", ec);
      if (std::filesystem::is_empty(*home, ec)) { std::filesystem::remove(*home, ec); }
    }
  }
  std::cout << "Left as they are: each project's 'vcs' and 'out' folders (delete them per project if you no longer "
    << "want them).\n";
  if (std::filesystem::exists(exe->parent_path(), ec) and std::filesystem::is_empty(exe->parent_path(), ec)) {
    std::cout << utils::files::NativeString(exe->parent_path()) << " is now empty; remove it too if it was made for "
      << "spp.\n";
  }
  return true;
}

auto spp::cli::RemoveUpdateLeftover() -> void {
#if SPP_PLATFORM_WINDOWS
  // What the last update moved aside, which can go now it no longer runs.
  if (const auto exe = RunningExecutable(); exe.has_value()) {
    auto ec = std::error_code();
    std::filesystem::remove(Path(exe->native() + L".old"), ec);
  }
#endif
}

auto spp::cli::HandleSelfUpdate(Opt<Str> const &version) -> bool {
  // Failsafe against the internally stored release label. This ensures that
  // everything is configured correctly.
  constexpr auto label = StrView(SPP_RELEASE_LABEL);
  if constexpr (label.empty()) {
    std::cerr
      << "Error: no release of spp is built for this platform; update it by "
      << "building it from source.\n";
    return false;
  }

  // Only the platforms a release is built for can be updated from one. Ensure
  // that the running executable is reachable, and is not managed by an external
  // package manager.
  const auto exe = RunningExecutable();
  if (not exe.has_value()) {
    std::cerr
      << "Error: could not find the running spp's executable.\n";
    return false;
  }
  if (const auto manager = ManagedInstall(*exe); manager.has_value()) {
    std::cerr
      << "Error: this spp was installed by " << *manager
      << "; update it there.\n";
    return false;
  }

  // Linux unpacks the release with "unzip", which is not always installed: say
  // so before downloading anything.
#if not SPP_PLATFORM_WINDOWS and not SPP_PLATFORM_MACOS
  if (not utils::files::FindProgram("unzip").has_value()) {
    std::cerr
      << "Error: 'spp self update' needs unzip to unpack the release; install "
      << "it (for example 'sudo apt install unzip') and run it again.\n";
    return false;
  }
#endif

  // Create a scratch directory (random temp folder). Get the server to request
  // the update from. This is either the release repo, or a mock test. Only the
  // release repo is "https-only".
  const auto scratch = ScratchDir();
  const auto [base, https_only] = ReleaseBase();

  constexpr auto current = StrView(SPP_VERSION);
  const auto current_numbers = VersionNumbers(current)
    .value_or(Vec<std::uint64_t>());
  auto target = Opt<Str>();

  // When requesting a specific version, allow the optional "v" prefix, followed
  // by the numbers.
  if (version.has_value()) {
    auto wanted = version->starts_with('v') ? version->substr(1) : *version;
    const auto numbers = VersionNumbers(wanted);
    if (not numbers.has_value()) {
      std::cerr // Target version validation.
        << "Error: '" << *version
        << "' is not a version (one like 0.7.1).\n";
      return false;
    }
    if (*numbers < *VersionNumbers(kFirstZippedRelease)) {
      std::cerr // Must be newer or the first zipped release.
        << "Error: spp " << wanted << " predates " << kFirstZippedRelease
        << ", the first release 'spp self update' can install; "
        << "download it from " << base << "/releases instead.\n";
      return false;
    }
    if (*numbers == current_numbers) {
      std::cout // Must be different to the current version.
        << "spp " << current
        << " is already the version asked for.\n";
      return true;
    }
    if (not ReleaseExists(scratch, base, wanted, https_only)) {
      std::cerr // An invalid release (but valis version format).
        << "Error: there is no release " << wanted
        << " (" << base << "/releases).\n";
      return false;
    }
    target = std::move(wanted);
  }

  // If no specific version was provided, then get the latest version. This is
  // queried from.
  else {
    target = LatestVersion(scratch, base, https_only);
    if (not target.has_value()) { return false; }
    const auto ordered = current_numbers <=> *VersionNumbers(*target);
    if (ordered == std::strong_ordering::equal) { // Already on the latest.
      std::cout
        << "spp " << current << " is the latest release.\n";
      return true;
    }
    if (ordered == std::strong_ordering::greater) { // Newer than the latest.
      std::cout
        << "spp " << current << " is newer than the latest release "
        << "(" << *target << "); nothing to do.\n";
      return true;
    }
  }

  // The platform's zip and the release's checksums, from that tag rather than
  // "latest", which could move between the two downloads. Print if this is an
  // upgrade or downgrade.
  const auto zip_name = "spp-" + Str(label) + ".zip";
  const auto tag_url = base + "/releases/download/v" + *target + "/";
  const auto zip = scratch.Dir / zip_name;
  const auto sums = scratch.Dir / "SHA256SUMS";
  const auto direction = current_numbers < *VersionNumbers(*target)
    ? "Updating"
    : "Downgrading";
  std::cout
    << direction << " spp " << current << " to "
    << *target << " (" << zip_name << ")\n";

  // Write the zip and the hashsum values into the filesystem; these will be
  // checked for a match afterwards.
  for (auto const &[file, name] : {Pair<Path, Str>{zip, zip_name}, Pair<Path, Str>{sums, "SHA256SUMS"}}) {
    const auto command = Curl(
      "--output "
      + utils::shell::Quote(utils::files::NativeString(file)) + " "
      + utils::shell::Quote(tag_url + name), https_only);
    if (not RunLogged(command, scratch.Dir / (name + ".err"), "downloading " + name)) { return false; }
  }

  // A checksum failure means that there is a corrupt package, so fail the
  // update.
  if (not MatchesChecksum(zip, sums, zip_name)) { return false; }

  // Unpack the one executable, and put it in place of this one.

  // Determine if the entry will be ".exe" or not (Windows vs POSIX), and create
  // the directory for the unpacking to happen.
  const auto entry = exe->filename().extension() == ".exe" ? Str("spp.exe") : Str("spp");
  const auto unpacked = scratch.Dir / "unpacked";
  std::filesystem::create_directories(unpacked);

  // Unpack the zip into the "unpacked" scratch subdirectory, for "entry". If
  // this unzipping works, continue to the replacement step.
  if (not Unpack(zip, entry, unpacked, scratch.Dir / "unpack.err")) { return false; }
  if (not Replace(*exe, unpacked / entry)) { return false; }
  std::cout
    << "Updated " << utils::files::NativeString(*exe)
    << " to spp " << *target << ".\n";
  return true;
}
