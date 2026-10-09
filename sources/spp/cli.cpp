module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <spp/version.hpp>
#include <spp/codegen/llvm_passes.hpp>

#define SPP_VALIDATE_STRUCTURE(is_exe) \
  if (not HandleValidate(is_exe)) { return; }

#define SPP_VALIDATE_STRUCTURE_OR(is_exe, ...) \
  if (not HandleValidate(is_exe)) { return __VA_ARGS__; }

#define SPP_CLI_NULL \
  bp::v1::std_out > bp::v1::null

module spp.cli;
import spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.asts.module_prototype_ast;
import spp.compiler.compiler;
import spp.compiler.compiler_boot;
import spp.compiler.module_tree;
import spp.compiler.out_layout;
import spp.lex.tokens;
import spp.lsp.diagnostic;
import spp.lsp.resolution_index;
import spp.parse.errors.parser_error;
import spp.utils.error_formatter;
import spp.utils.errors;
import spp.utils.features;
import spp.utils.files;
import cli11;
import genex;
import tomlpp;

inline constexpr spp::Str OUT_FOLDER = "out";
inline constexpr spp::Str SRC_FOLDER = "src";
inline constexpr spp::Str VCS_FOLDER = "vcs";
inline constexpr spp::Str FFI_FOLDER = "ffi";
inline constexpr spp::Str TST_FOLDER = "tst";

inline constexpr spp::Str MAIN_FILE = "main.spp";
inline constexpr spp::Str CONFIG_FILE = "spp.toml";
inline constexpr spp::Str STUB_FILE = "stub.spp";

inline const spp::Str MAIN_FILE_CONTENTS = R"(
fun main() -> Void {
  std::console::println("Hello world!")
})";

inline const spp::Str CONFIG_FILE_CONTENTS = R"(
[project]
name = "$"
version = "0.1.0"
build = "exe"

[vcs]
std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" })";

namespace spp::cli {
  namespace {
    /// Iterate a directory that may not be there. The optional
    /// folders are only created inside this project, so a
    /// dependency legitimately has none of them and walking one
    /// must not throw.
    auto SafeDirectoryIterator(std::filesystem::path const &dir) -> std::filesystem::directory_iterator {
      return std::filesystem::exists(dir)
        ? std::filesystem::directory_iterator(dir)
        : std::filesystem::directory_iterator();
    }

    /// Run a compilation and report a mistake in the source being
    /// compiled.
    auto CompileReportingErrors(
      compiler::Compiler &c, Str const &message_format = "human") -> bool {
      // Json is asked for by something reading the output rather
      // than someone looking at it, so it catches in every build:
      // a debugger is not what is on the other end of the pipe.
      // Used by the IDEA plugin.
      if (message_format == "json") {
        return lsp::CompileReportingJson(c);
      }

#if SPP_DEBUG
      // A debug build deliberately does not: "Compiler::Compile"
      // leaves its own catch out under the same condition, so the
      // stack is still standing where the throw happened and a
      // debugger can be pointed at it. That build is the compiler's
      // own; this is the one a program is compiled with.
      return c.Compile();
#else
      try {
        return c.Compile();
      }
      catch (utils::errors::AbstractError const &e) {
        std::cerr << e.what() << "\n";
        return false;
      }
#endif
    }

    /// Force a memory limit on the process. This was originally
    /// required for when the early STD was extremely bugged,
    /// causing allocations that blew up WSL by overwriting
    /// critical process memory space (not sure how). Not required
    /// anymore I don't think. Good failsafe?
    auto WithMemoryLimit(Str const &command) -> Str {
#if SPP_PLATFORM_WINDOWS
      return command;
#else
      constexpr auto default_kb = 16ull * 1024 * 1024;
      const auto env = std::getenv("SPP_MEMORY_LIMIT_KB");
      const auto limit_kb = env != nullptr ? std::strtoull(env, nullptr, 10) : default_kb;
      return limit_kb == 0 ? command : "ulimit -v " + std::to_string(limit_kb) + " && " + command;
#endif
    }

    /// Run a git command invocation, reporting a non-zero exit
    /// rather than discarding it. Wraps all git command calls.
    auto RunGit(Str const &args) -> bool {
      const auto command = "git " + args;
      if (const auto status = std::system(command.c_str()); status != 0) {
        std::cerr << "Error: git failed (" << status << "): " << command << "\n";
        return false;
      }
      return true;
    }

    auto HostOf(Str const &url) -> Str {
      auto rest = StrView(url);
      if (const auto scheme = rest.find("://"); scheme != StrView::npos) { rest.remove_prefix(scheme + 3); }

      // Only a "user@" before the first "/" is credentials;
      // an "@" further in belongs to the path.
      const auto slash = rest.find('/');
      if (const auto at = rest.find('@');
        at != StrView::npos and (slash == StrView::npos or at < slash)) {
        rest.remove_prefix(at + 1);
      }
      return Str(rest.substr(0, rest.find_first_of(":/?")));
    }

    auto IsRemoteReachable(Str const &url) -> bool {
      if (url.empty() or std::getenv("SPP_NO_NETWORK_CHECK") != nullptr) { return true; }
      const auto args =
        " -c credential.helper= -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=10 ls-remote --exit-code --heads "
        + url;

#if SPP_PLATFORM_WINDOWS
      const auto command = "set GIT_TERMINAL_PROMPT=0&& git" + args + " >NUL 2>&1";
#else
      const auto command = "GIT_TERMINAL_PROMPT=0 git" + args + " >/dev/null 2>&1";
#endif
      return std::system(command.c_str()) == 0;
    }
  }
}

auto spp::cli::RunCli(
  const std::int32_t argc,
  char **argv)
  -> std::int32_t {
  // Create the CLI object and require that a subcommand
  // is provided.
  auto app = CLI::App("SPP build tool", "spp");
  app.require_subcommand(1);

  // Declared so that it parses and shows up in the help, but read in "main" rather than here: the working directory
  // has to be settled before any subcommand callback runs, and every one of them resolves the project from it.
  auto project_dir = Str();
  app.add_option(
    "--dir", project_dir,
    "Project directory to work in; the sample project beside this binary by default.");

  // One variable per subcommand, each holding its own
  // default. A single shared one takes whichever default
  // was declared last, so "spp build" with no "-m" ran
  // with an empty mode and was rejected as invalid.
  auto build_mode = Str("dev");
  auto run_mode = Str("dev");
  auto clean_mode = Str("all");

  // One target per subcommand. Empty is the host, which
  // is what every build was before the option existed.
  auto build_target = Str();
  auto run_target = Str();
  auto clean_target = Str();

  // How a failure is reported: as the blocks a person reads, or
  // as one diagnostic object per error for whatever is reading
  // the output - the editor integration, today.
  auto build_message_format = Str("human");

  // Both are for whatever is driving the compiler rather than
  // for a person: skip the dependency fetch a build normally
  // starts with, and stop once the analysis is done.
  auto build_skip_vcs = false;
  auto build_analyse_only = false;
  auto build_index_files = Vec<Str>();
  auto build_index_project = false;

  // No example triple: which ones resolve depends on the backends
  // this binary was linked with, so an example here would be wrong
  // for a default build. Passing an unrecognised one lists what
  // this build can actually emit for.
  constexpr auto target_help =
    "Target triple to build for; the host by default. Pass an unknown one to list what this build supports. A "
    "target other than the host is compiled and an object emitted, but not linked - see the note the build prints.";

  constexpr auto message_format_help =
    "How errors are reported: 'human' for the annotated source blocks, or 'json' for one diagnostic object per error.";

  app.add_subcommand("init", "Initialize the new project")
     ->fallthrough()
     ->callback([] { if (not HandleInit()) { throw CLI::RuntimeError(1); } });

  app.add_subcommand("vcs", "Initialize version control for the project")
     ->fallthrough()
     ->callback([] { if (not HandleVcs()) { throw CLI::RuntimeError(1); } });

  const auto build_cmd = app.add_subcommand("build", "Build the project")->fallthrough();
  build_cmd->add_option("-m,--mode", build_mode, "Build mode (dev or rel)")
           ->check(CLI::IsMember({"dev", "rel"}))
           ->default_val("dev");
  build_cmd->add_option("-t,--target", build_target, target_help);
  build_cmd->add_option("--message-format", build_message_format, message_format_help)
           ->check(CLI::IsMember({"human", "json"}))
           ->default_val("human");
  build_cmd->add_flag(
    "--skip-vcs", build_skip_vcs,
    "Do not fetch the [vcs] dependencies before building");
  build_cmd->add_flag(
    "--analyse-only", build_analyse_only,
    "Stop once the analysis is done: report errors, but generate and write nothing");
  build_cmd->add_option(
    "--index-file", build_index_files,
    "Also report what every name written in this file resolved to; may be given more than once. Needs "
    "'--message-format=json'");
  build_cmd->add_flag(
    "--index-project", build_index_project,
    "Report what every name in the project's own files resolved to - the same compile answers for all of them");
  build_cmd->callback(
    [&build_mode, &build_target, &build_message_format, &build_skip_vcs, &build_analyse_only, &build_index_files,
      &build_index_project] {
      if (not HandleBuild(
        build_mode, build_target, build_skip_vcs, build_message_format, build_analyse_only, build_index_files,
        build_index_project)) {
        throw CLI::RuntimeError(1);
      }
    });

  const auto run_cmd = app.add_subcommand("run", "Run the project")->fallthrough();
  run_cmd->add_option("-m,--mode", run_mode, "Run mode (dev or rel)")
         ->check(CLI::IsMember({"dev", "rel"}))
         ->default_val("dev");
  run_cmd->add_option("-t,--target", run_target, target_help);
  run_cmd->callback([&run_mode, &run_target] {
    if (not HandleRun(run_mode, run_target)) { throw CLI::RuntimeError(1); }
  });

  const auto clean_cmd = app.add_subcommand("clean", "Clean the project")->fallthrough();
  clean_cmd->add_option("-m,--mode", clean_mode, "Clean mode (dev, rel or all)")
           ->check(CLI::IsMember({"dev", "rel", "all"}))
           ->default_val("all");
  clean_cmd->add_option(
    "-t,--target", clean_target,
    "Only clean this target's tree; every target's by default");
  clean_cmd->callback([&clean_mode, &clean_target] { HandleClean(clean_mode, clean_target); });

  auto test_name_filter = Str();
  auto test_group_filter = Str();
  auto test_libs = std::vector<Str>();
  auto test_all_libs = false;
  const auto test_cmd = app.add_subcommand("test", "Test the project")->fallthrough();
  test_cmd->add_option(
    "-f,--filter", test_name_filter,
    "Only run tests whose fully qualified name contains this");
  test_cmd->add_option(
    "-g,--group", test_group_filter,
    "Only run tests in this group");
  test_cmd->add_option(
    "-l,--lib", test_libs,
    "Also run the unit tests of this [vcs] library (repeatable)");
  test_cmd->add_flag(
    "--all-libs", test_all_libs,
    "Also run the unit tests of every [vcs] library");

  test_cmd->callback([&test_name_filter, &test_group_filter, &test_libs, &test_all_libs] {
    HandleTest(
      test_name_filter, test_group_filter,
      Vec(test_libs.begin(), test_libs.end()), test_all_libs);
  });

  app.add_subcommand("validate", "Validate the project")
     ->fallthrough()
     ->callback([] { if (not HandleValidate(false)) { throw CLI::RuntimeError(1); } });

  app.add_subcommand("config", "List every section and key 'spp.toml' accepts")
     ->fallthrough()
     ->callback([] { std::cout << utils::features::HelpText(); });

  app.add_subcommand("version", "Show version information")
     ->fallthrough()
     ->callback(HandleVersion);

  // Parse the command line arguments.
  try {
    app.parse(argc, argv);
  }
  catch (CLI::CallForHelp const &e) { return app.exit(e); }
  catch (CLI::CallForAllHelp const &e) { return app.exit(e); }
  catch (CLI::RuntimeError const &e) { return e.get_exit_code(); }
  catch (CLI::ParseError const &e) {
    std::cerr << e.what() << "\n\n" << app.help();
    return e.get_exit_code();
  }
  return 0;
}

auto spp::cli::HandleInit()
  -> bool {
  // Check if the current directory is empty or not.
  const auto cwd = std::filesystem::current_path();
  if (not std::filesystem::is_empty(cwd)) {
    std::cerr << "Error: The current directory is not empty. Please run this command in an empty directory.\n";
    return false;
  }

  // Create the directory structure (folders).
  std::filesystem::create_directory(cwd / OUT_FOLDER);
  std::filesystem::create_directory(cwd / SRC_FOLDER);
  std::filesystem::create_directory(cwd / VCS_FOLDER);
  std::filesystem::create_directory(cwd / FFI_FOLDER);
  std::filesystem::create_directory(cwd / TST_FOLDER);

  // Add the key files into the directory structure.
  std::filesystem::create_directory(cwd / SRC_FOLDER / cwd.filename());

  // Fill in "main.spp" and "spp.toml" with template content.
  utils::files::WriteFile(cwd / SRC_FOLDER / MAIN_FILE, FormatDefaultFileContents(MAIN_FILE_CONTENTS));
  utils::files::WriteFile(cwd / CONFIG_FILE, CreateDefaultConfigFor(utils::files::DisplayString(cwd.filename())));
  return true;
}

auto spp::cli::HandleVcs()
  -> bool {
  // Validate the project structure first.
  using namespace std::string_literals;
  SPP_VALIDATE_STRUCTURE_OR(false, false);

  // Parse the spp.toml config file and get the optional "vcs"
  // section. A project with no dependencies has nothing to
  // fetch, which is a successful outcome rather than a failure.
  const auto toml = toml::parse_file(CONFIG_FILE);
  if (not toml["vcs"]) { return true; }

  // Move into the VCS folder.
  const auto cwd = std::filesystem::current_path();
  std::filesystem::current_path(cwd / VCS_FOLDER);

  // Iterate over the vcs section and clone/update the repositories.
  auto ok = true;
  auto reachable = Map<Str, bool>();
  auto vcs = toml["vcs"].as_table();
  for (auto [key, info] : *vcs) {
    auto repo_name = Str(key);
    auto repo_url = (*info.as_table())["git"].value<Str>().value();
    auto repo_branch = (*info.as_table())["branch"].value<Str>().value_or("master");
    auto repo_folder = cwd / VCS_FOLDER / repo_name;
    auto repo_target = utils::files::NativeString(repo_folder);

    // Ping the host before handing it to git, once per host:
    // later repositories on the same host reuse the answer.
    const auto host = HostOf(repo_url);
    auto host_is_up = [&] {
      auto [cached, first_seen] = reachable.try_emplace(host, false);
      if (first_seen) { cached->second = IsRemoteReachable(repo_url); }
      return cached->second;
    };

    // Repo doesn't exist locally => clone it.
    if (not std::filesystem::exists(repo_folder)) {
      if (not RunGit("clone --branch " + repo_branch + " " + repo_url + " " + repo_target)) {
        if (not host_is_up()) {
          std::cerr << "Error: '"s + host + "' is unreachable, and " + repo_name + " has not been cloned yet.\n";
        }
        ok = false;
        continue;
      }
      std::cout << "Cloned "s + repo_name + " from " + repo_url + "\n";
    }

    else if (not RunGit("-C " + repo_target + " checkout " + repo_branch) or
      not RunGit("-C " + repo_target + " pull origin " + repo_branch)) {
      if (host_is_up()) {
        ok = false;
        continue;
      }
      std::cerr << "Warning: '"s + host + "' is unreachable; using the existing checkout of " + repo_name + ".\n";
    }
    else {
      std::cout << "Updated "s + repo_name + " from " + repo_url + " (" + repo_branch + ")" + "\n";
    }

    // Copy all DLLs from the VCS's FFI folder into this
    // project's FFI folder.
    auto ffi_repo_folder = repo_folder / FFI_FOLDER;
    if (std::filesystem::exists(ffi_repo_folder)) {
      for (auto const &entry : std::filesystem::directory_iterator(ffi_repo_folder)) {
        std::filesystem::copy(
          entry.path(), cwd / FFI_FOLDER / entry.path().filename(),
          std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
      }
    }
  }

  // Move back into the original working directory.
  std::filesystem::current_path(cwd);
  return ok;
}

auto spp::cli::HandleBuild(
  Str const &mode,
  Str const &target,
  const bool skip_vcs,
  Str const &message_format,
  const bool analyse_only,
  Vec<Str> const &index_files,
  const bool index_project)
  -> bool {
  // Validate the project structure first.
  SPP_VALIDATE_STRUCTURE_OR(false, false);

  // Choose the target before anything is compiled: every module
  // carries the triple and the data layout it was built for, and
  // those are resolved once, on first use.
  if (not codegen::SelectTarget(target.c_str())) { return false; }

  // Before anything is generated, not after: if llvm cannot spell an intrinsic name, every module holding one is
  // rejected by the verifier with a complaint about the name, and the cause is nowhere in that message.
  codegen::AssertIntrinsicNamingIsSound();

  // Create the inner directory (rel or dev).
  const auto cwd = std::filesystem::current_path();
  const auto out = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = mode
  };
  std::filesystem::create_directories(out.OutRoot());

  // Remove the executable first, so a build that fails leaves
  // nothing behind for "run" to pick up and execute as if it
  // were the build that just happened. An analysis-only run
  // produces no executable and must not disturb the one a real
  // build left there - an editor analysing on every save would
  // otherwise keep deleting it. Stages 1-9 inclusive.
  if (not analyse_only) { std::filesystem::remove(out.ExecutablePath()); }

  // Handle VCS if not skipped. Building against a half-fetched
  // "vcs" folder reports every imported symbol as undefined
  // rather than the fetch failure that caused it, so stop here
  // instead.
  if (not skip_vcs and not HandleVcs()) {
    std::cerr << "Error: Aborting the build; the [vcs] dependencies could not be fetched.\n";
    return false;
  }

  // Revalidate (after including the VCS folders).
  SPP_VALIDATE_STRUCTURE_OR(false, false);
  const auto build_type =
    toml::parse_file(CONFIG_FILE)["project"].as_table()->at("build").value<Str>();

  // Validate the mode is "dev" or "rel".
  if (mode != "dev" and mode != "rel") {
    std::cerr << "Error: Invalid mode. Mode must be 'dev' or 'rel'.\n";
    return false;
  }

  // Adopt this project's feature settings. Only this one: how
  // a dependency configured itself is a property of that
  // dependency's own build, and what comes out of here is
  // one program, generated once.
  auto config_errors = Vec<Str>();
  if (not utils::features::Load(std::filesystem::current_path() / CONFIG_FILE, config_errors)) {
    for (auto const &e : config_errors) { std::cerr << "Error in spp.toml: " << e << "\n"; }
    return false;
  }

  // Compile the code. Call the resolution index hooks depending
  // on what is being indexed (project, specific files, etc).
  auto c = compiler::Compiler(
    mode == "dev" ? compiler::Compiler::Mode::DEV : compiler::Compiler::Mode::REL,
    build_type == "exe" ? compiler::Compiler::BuildType::EXE : compiler::Compiler::BuildType::LIB);

  c.SetAnalyseOnly(analyse_only);
  if (index_project) { lsp::resolution_index::EnableProject(); }
  else { lsp::resolution_index::EnableFiles(index_files); }
  return CompileReportingErrors(c, message_format);
}

auto spp::cli::HandleRun(
  Str const &mode,
  Str const &target)
  -> bool {
  // Build the project first (skip VCS). Don't run the old
  // binary when the current code causes an error.
  if (not HandleBuild(mode, target, false)) { return false; }

  // Nothing this machine can execute comes out of a cross build,
  // so say that rather than reporting the missing executable as
  // though the build had gone wrong.
  if (not codegen::TargetIsHost()) {
    std::cerr
      << "Error: Cannot run a build for '" << codegen::TargetFolderName()
      << "'; only a build for the host can be executed here.\n";
    return false;
  }

  // A build that did not get as far as linking has said why
  // already, so there is nothing to add here beyond not
  // trying to run something that was never produced.
  const auto cwd = std::filesystem::current_path();
  const auto exe_file = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = mode
  }.ExecutablePath();
  if (not std::filesystem::exists(exe_file)) {
    std::cerr << "Error: No executable was built at '" << utils::files::DisplayString(exe_file) << "'.\n";
    return false;
  }

  // Pull the returned status code from the run process (ie
  // the compiled s++ code), and exit with that code, after
  // displaying the output message.
  std::cout << "Running: " << utils::files::DisplayString(exe_file) << std::endl;
  std::cout.flush();

  // Home the cursor, wipe the screen and then the scrollback,
  // so the program's own output is all that is on the console,
  // rather than the tail of the build that produced it. This
  // is the byte sequence "clear" sends: without the ED 3, the
  // erased lines stay in the scrollback and the console still
  // reads as uncleared.
  std::cout << "\033[H\033[2J\033[3J";
  std::cout.flush();

  const auto status = std::system(WithMemoryLimit(utils::files::NativeString(exe_file)).c_str());
  const auto exited_normally = (status & 0x7F) == 0;
  const auto exit_code = exited_normally ? (status >> 8) & 0xFF : status;

  if (not exited_normally) { std::cerr << "Program terminated abnormally (status " << status << ").\n"; }
  else if (exit_code != 0) { std::cerr << "Program exited with code " << exit_code << ".\n"; }
  std::exit(exit_code);
}

auto spp::cli::HandleClean(
  Str const &mode,
  Str const &target)
  -> void {
  // Validate the project structure first.
  SPP_VALIDATE_STRUCTURE(false);

  // Which targets to sweep: the one named, or every one that has
  // been built here. Reading them off disk rather than off a list
  // of known triples is what makes a target built once and then
  // never asked for again still cleanable.
  const auto cwd = std::filesystem::current_path();
  auto targets = Vec<Str>();
  if (not target.empty()) {
    if (not codegen::SelectTarget(target.c_str())) { return; }
    targets.EmplaceBack(codegen::TargetFolderName());
  }
  else {
    for (auto const &entry : SafeDirectoryIterator(cwd / OUT_FOLDER)) {
      if (entry.is_directory()) { targets.EmplaceBack(utils::files::NativeString(entry.path().filename())); }
    }
  }

  // Remove the appropriate folders. Every build's artifacts live
  // under its own target and mode folder, so removing that is the
  // whole of the clean - there is nothing left outside it.
  for (auto const &t : targets) {
    for (auto const &known : compiler::OutLayout::AllModes()) {
      if (mode != "all" and mode != known) { continue; }
      std::filesystem::remove_all(compiler::OutLayout{.Root = cwd, .Target = t, .Mode = known}.OutRoot());
    }

    // A target folder with no modes left in it is noise, so it
    // goes too - but only when it is genuinely empty, because a
    // "clean -m dev" leaves the "rel" build standing.
    const auto target_root = cwd / OUT_FOLDER / t;
    if (std::filesystem::exists(target_root) and std::filesystem::is_empty(target_root)) {
      std::filesystem::remove(target_root);
    }
  }
}

auto spp::cli::HandleTest(
  Str const &name_filter,
  Str const &group_filter,
  Vec<Str> const &libs,
  const bool all_libs)
  -> void {
  // Validate the project structure first.
  SPP_VALIDATE_STRUCTURE(false);

  // Fetch the dependencies, as an ordinary build does: a
  // test build compiles the same sources plus the "tst"
  // folder.
  if (not HandleVcs()) {
    std::cerr << "Error: Aborting the test build; the [vcs] dependencies could not be fetched.\n";
    return;
  }
  SPP_VALIDATE_STRUCTURE(false);

  // A test build compiles at "rel", so it must look for its
  // executable there too - see the Compiler constructed below.
  const auto cwd = std::filesystem::current_path();
  const auto out = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = "rel"
  };
  std::filesystem::create_directories(out.OutRoot());

  // Remove the executable before building, so a build that
  // fails to link cannot leave the previous one behind to be
  // run and reported as a pass.
  const auto exe_file = out.ExecutablePath();
  std::filesystem::remove(exe_file);

  for (auto const &lib : libs) {
    if (std::filesystem::is_directory(cwd / VCS_FOLDER / lib)) { continue; }
    std::cerr << "Error: No library named '" << lib << "' under '" << VCS_FOLDER << "'.\n";
    std::exit(1);
  }

  const auto scope = compiler::TestScope{.Project = true, .AllLibs = all_libs, .Libs = libs};
  auto c = compiler::Compiler(
    compiler::Compiler::Mode::REL, compiler::Compiler::BuildType::EXE, scope);
  c.SetTestFilters(name_filter, group_filter);
  if (not CompileReportingErrors(c)) { std::exit(1); }

  if (c.TestCount() == 0) {
    std::cerr << "No unit tests matched. Mark a function in 'tst' with '!test'";
    if (not name_filter.empty() or not group_filter.empty()) { std::cerr << ", or relax the filter"; }
    std::cerr << ".\n";
    std::exit(1);
  }

  if (not std::filesystem::exists(exe_file)) {
    std::cerr << "Error: No test executable was built at '" << utils::files::DisplayString(exe_file) << "'.\n";
    std::exit(1);
  }

  // One process for the whole suite first: that is the fast
  // path, and when everything passes it is all that runs.
  std::cout << "Running " << c.TestCount() << " test(s)." << std::endl;
  std::cout.flush();

  const auto exe = utils::files::NativeString(exe_file);

  // The selector is set on the command rather than in this
  // process, so nothing has to be unset afterwards and the
  // whole-suite run is plain.
  const auto run = [&exe](Str const &only) {
    const auto command = only.empty() ? exe : "SPP_TEST_ONLY=" + only + " " + exe;
    const auto status = std::system(WithMemoryLimit(command).c_str());
    const auto exited_normally = (status & 0x7F) == 0;
    return exited_normally ? (status >> 8) & 0xFF : -1;
  };

  if (run("") == 0) {
    std::cout << "All " << c.TestCount() << " test(s) passed." << std::endl;
    return;
  }

  // Something took the shared run down with it - an assertion
  // aborts the process, and so does a miscompiled test - so
  // everything ordered behind it never ran. Re-run them a
  // process each, so one casualty costs one result rather than
  // the rest of the suite.
  std::cout << "\nA test did not finish. Re-running each test in its own process.\n" << std::endl;
  std::cout.flush();

  auto failed = Vec<Str>();
  for (auto const &name : c.TestNames()) {
    if (run(name) != 0) { failed.EmplaceBack(name); }
  }

  std::cout << "\n" << (c.TestCount() - failed.Len()) << "/" << c.TestCount() << " test(s) passed." << std::endl;
  if (failed.IsEmpty()) {
    // Every test passes on its own, so what failed is something
    // they share rather than any one of them.
    std::cerr << "The suite failed as a whole but every test passes alone - suspect shared state or the harness.\n";
    std::exit(1);
  }

  std::cerr << "Failed:\n";
  for (auto const &name : failed) { std::cerr << "  " << name << "\n"; }
  std::exit(1);
}

auto spp::cli::HandleValidate(
  const bool is_exe,
  const bool create_missing)
  -> bool {
  using namespace std::string_literals;

  // Check for the key folders/files.
  const auto cwd = std::filesystem::current_path();
  if (not std::filesystem::exists(cwd / SRC_FOLDER)) {
    std::cerr << "Error: Missing 'src' folder.\n";
    return false;
  }
  if (not std::filesystem::exists(cwd / SRC_FOLDER / MAIN_FILE) and is_exe) {
    std::cerr << "Error: Missing 'src/main.spp' file.\n";
    return false;
  }
  if (not std::filesystem::exists(cwd / CONFIG_FILE)) {
    std::cerr << "Error: Missing 'spp.toml' file.\n";
    return false;
  }

  // Create the other folders if they don't exist, but only in this
  // project - a dependency's checkout is a fetched artifact, and
  // filling it with empty folders makes its working tree dirty.
  if (create_missing) {
    if (not std::filesystem::exists(cwd / OUT_FOLDER)) {
      std::filesystem::create_directory(cwd / OUT_FOLDER);
    }
    if (not std::filesystem::exists(cwd / VCS_FOLDER)) {
      std::filesystem::create_directory(cwd / VCS_FOLDER);
    }
    if (not std::filesystem::exists(cwd / FFI_FOLDER)) {
      std::filesystem::create_directory(cwd / FFI_FOLDER);
    }
    if (not std::filesystem::exists(cwd / TST_FOLDER)) {
      std::filesystem::create_directory(cwd / TST_FOLDER);
    }
  }

  // Check the file against the schema: every section and key
  // it holds has to be one the compiler knows, and every
  // required one has to be there. An unrecognised key is
  // reported rather than ignored, because a mistyped one that
  // silently does nothing is worse than no key - a protection
  // turned off by a typo turns off nothing.
  auto config_errors = Vec<Str>();
  if (not utils::features::Validate(cwd / CONFIG_FILE, config_errors)) {
    for (auto const &e : config_errors) { std::cerr << "Error in spp.toml: " << e << "\n"; }
    std::cerr << "\nRun 'spp config' for everything spp.toml accepts.\n";
    return false;
  }

  const auto toml = toml::parse_file(CONFIG_FILE);
  const auto project = toml["project"].as_table();

  // Check the version follows "major.minor.patch" format.
  const auto version = project->at("version").value<Str>().value_or("");
  const auto version_parts = version | genex::views::split('.') | genex::to<Vec>();
  if (version_parts.Len() != 3) {
    std::cerr << "Error: Invalid version format in spp.toml. Version must follow 'major.minor.patch' format.\n";
    return false;
  }

  // Check the build type is either "exe" or "lib".
  const auto build_type = project->at("build").value<Str>().value_or("");
  if (build_type != "exe" and build_type != "lib") {
    std::cerr << "Error: Invalid build type in spp.toml. Build type must be 'exe' or 'lib'.\n";
    return false;
  }

  // For the VCS folders, validate each VCS entry (if it exists).
  for (auto const &vcs_dir : SafeDirectoryIterator(cwd / VCS_FOLDER)) {
    if (not vcs_dir.is_directory()) { continue; }
    std::filesystem::current_path(vcs_dir.path());
    HandleValidate(false, false);
    std::filesystem::current_path(cwd);
  }

  // Check the FFI subfolders are structured properly.
  for (auto const &ffi_dir : SafeDirectoryIterator(cwd / FFI_FOLDER)) {
    if (not std::filesystem::is_directory(ffi_dir)) {
      std::cerr << "Error: Non-directory found in 'ffi' folder: "s + utils::files::DisplayString(
        ffi_dir.path().filename()) + "\n";
      return false;
    }

    const auto lib_name = utils::files::NativeString(ffi_dir.path().filename());

    // Check for a stub file for each ffi package. This is what
    // s++ uses (s++ code) to hook into the shared library.
    if (not std::filesystem::exists(ffi_dir.path() / STUB_FILE)) {
      std::cerr << "Error: Missing '"s + STUB_FILE + "' file in 'ffi/" + lib_name + "' folder.\n";
      return false;
    }

    // Collect the shared library binaries within the folder, and
    // get the expected library binary name and extension that will
    // be checked against.
    const auto expected = utils::files::SharedLibraryName(lib_name);
    if (std::filesystem::exists(ffi_dir.path() / expected)) { continue; }

    auto found_names = Str();
    for (auto const &entry : SafeDirectoryIterator(ffi_dir.path())) {
      if (not entry.is_regular_file()) { continue; }
      const auto found = utils::files::NativeString(entry.path().filename());
      if (found == STUB_FILE) { continue; }
      found_names += (found_names.empty() ? "" : ", ") + found;
    }

    std::cerr
      << "Error: 'ffi/" + lib_name + "' has no '" + expected + "'; it holds "
      + (found_names.empty() ? Str("nothing but its stub") : found_names) + ".\n"
      << "  A package is linked through the library named for it, so one built for another platform does not "
      << "stand in for this host's.\n";
    return false;
  }

  // All checks passed.
  return true;
}

auto spp::cli::HandleVersion()
  -> void {
  // Print the version information.
  std::cout << "SPP version " << SPP_VERSION << "\n";
}

auto spp::cli::CreateDefaultConfigFor(
  Str const &project_name)
  -> Str {
  // Inject the project name into the config template.
  auto contents = FormatDefaultFileContents(CONFIG_FILE_CONTENTS);
  const auto pos = contents.find('$');
  contents.replace(pos, 1, project_name);
  return contents;
}

auto spp::cli::RunCppGoogleTest(
  Str const &mode,
  Str &&main_code)
  -> Map<Str, Str> {
  // Validate the project structure first.
  SPP_VALIDATE_STRUCTURE_OR(false, {});

  // Create the inner directory (rel or dev).
  const auto cwd = std::filesystem::current_path();
  std::filesystem::create_directories(
    compiler::OutLayout{.Root = cwd, .Target = codegen::TargetFolderName(), .Mode = mode}.OutRoot());
  SPP_VALIDATE_STRUCTURE_OR(false, {});

  // Compile the code.
  const auto m = mode == "dev"
    ? compiler::Compiler::Mode::DEV
    : compiler::Compiler::Mode::REL;
  // Discarded on purpose: this path verifies rather than builds, and a
  // verification that fails throws out of here rather than answering.
  const auto c = compiler::Compiler::ForCppGoogleTest(m, std::move(main_code));
  static_cast<void>(c->Compile());
  return c->CompTimeConstants();
}

auto spp::cli::FormatDefaultFileContents(
  const StrView contents)
  -> Str {
  return Str(contents);
  // // Remove the first newline, and replace "    " with "".
  // auto out = Str();
  // for (const auto&line: contents | genex::views::split('\n')) {
  //     if (line.Len() <= 0) { continue; }
  //     auto formatted = line | genex::to<Str>();
  //     formatted.replace(formatted.find("    "), 4, "");
  // }
  // return out;
}
