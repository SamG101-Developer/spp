module;
#include <llvm/Config/llvm-config.h>
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>
#include <spp/version.hpp>

#define SPP_VALIDATE_STRUCTURE(is_exe) \
  if (not HandleValidate(is_exe)) { std::exit(1); }

#define SPP_VALIDATE_STRUCTURE_OR(is_exe, ...) \
  if (not HandleValidate(is_exe)) { return __VA_ARGS__; }

#define SPP_VALIDATE_PROJECT(is_exe) \
  if (not ValidateProject(is_exe, true)) { std::exit(1); }

#define SPP_VALIDATE_PROJECT_OR(is_exe, ...) \
  if (not ValidateProject(is_exe, true)) { return __VA_ARGS__; }

#define SPP_CLI_NULL \
  bp::v1::std_out > bp::v1::null

#define CLI_ERR \
  CLI::RuntimeError(1)

module spp.cli;
import spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.asts.module_prototype_ast;
import spp.codegen.llvm_artefact;
import spp.codegen.llvm_support;
import spp.codegen.llvm_target;
import spp.compiler.compiler;
import spp.compiler.compiler_boot;
import spp.compiler.dependencies;
import spp.compiler.ffi;
import spp.compiler.module_tree;
import spp.compiler.out_layout;
import spp.lex.tokens;
import spp.lsp.diagnostic;
import spp.lsp.resolution_index;
import spp.parse.errors.parser_error;
import spp.utils.env;
import spp.utils.error_formatter;
import spp.utils.errors;
import spp.utils.features;
import spp.utils.files;
import spp.utils.ice;
import spp.utils.shell;
import spp.utils.stats;
import cli11_;
import genex;
import magic_enum;
import sys;
import tomlpp;

inline constexpr spp::Str OUT_FOLDER = "out";
inline constexpr spp::Str SRC_FOLDER = "src";
inline constexpr spp::Str VCS_FOLDER = "vcs";
inline constexpr spp::Str FFI_FOLDER = "ffi";
inline constexpr spp::Str TST_FOLDER = "tst";

inline constexpr spp::Str MAIN_FILE = "main.spp";
inline constexpr spp::Str CONFIG_FILE = "spp.toml";

inline constexpr auto CMD_HELP_TEXT_BUILD_TARGET =
  "Target triple to build for; the host by default. Pass an unknown one to "
  "list what this build supports. A target other than the host is compiled and "
  "an object emitted, but not linked - see the note the build prints.";

inline constexpr auto CMD_HELP_TEXT_TEST_TARGET =
  "Target triple to build the tests for; the host by default. Another target's "
  "tests run only under a '--runner' that can execute them, e.g. "
  "\"qemu-aarch64 -L /usr/aarch64-linux-gnu\"";

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
    /// Check the current directory has the structure a project needs, without
    /// looking at its dependencies. This is all that can be asked before a
    /// fetch: a dependency's checkout may be missing, or broken in a way the
    /// fetch is about to repair. "create_missing" is for the project itself, as
    /// filling a dependency's checkout with empty folders makes it dirty.
    auto ValidateProject(
      const bool is_exe, const bool create_missing) -> bool {
      using namespace std::string_literals;

      // Check for the key folders/files. The "src" folder holds all s++ code,
      // the main.spp is the required entry point for executables, and the
      // spp.toml is required for the project config.
      const auto cwd = std::filesystem::current_path();
      if (not std::filesystem::exists(cwd / SRC_FOLDER)) {
        std::cerr << "Error: Missing 'src' folder.\n";
        return false;
      }
      const auto main_file = cwd / SRC_FOLDER / MAIN_FILE;
      if (not std::filesystem::exists(main_file) and is_exe) {
        std::cerr << "Error: Missing 'src/main.spp' file.\n";
        return false;
      }
      if (not std::filesystem::exists(cwd / CONFIG_FILE)) {
        std::cerr << "Error: Missing 'spp.toml' file.\n";
        return false;
      }

      // Create the other folders if they don't exist, but only in this project
      // - a dependency's checkout is a fetched artifact, and filling it with
      // empty folders makes its working tree dirty.
      if (create_missing) {
        std::filesystem::create_directory(cwd / OUT_FOLDER);
        std::filesystem::create_directory(cwd / VCS_FOLDER);
        std::filesystem::create_directory(cwd / FFI_FOLDER);
        std::filesystem::create_directory(cwd / TST_FOLDER);
      }

      // Check the config file against the schema: every section and key it
      // holds has to be one the compiler knows, and every required one has to
      // be there. An unrecognised key is reported rather than ignored, because
      // a mistyped one that silently does nothing is worse than no key - a
      // protection turned off by a typo turns off nothing.
      auto config_errors = Vec<Str>();
      if (not utils::features::Validate(cwd / CONFIG_FILE, config_errors)) {
        for (auto const &e : config_errors) {
          std::cerr << "Error in spp.toml: " << e << "\n";
        }
        std::cerr << "\nRun 'spp config' for everything spp.toml accepts.\n";
        return false;
      }

      // Parse the TOML file and extract the project section, guaranteed to be
      // present. This contains the key project metadata.
      const auto toml = toml::parse_file(CONFIG_FILE);
      const auto project = toml["project"].as_table();

      // Check the version follows the "major.minor.patch" format.
      const auto version = project->at("version").value<Str>().value_or("");
      const auto version_part_count = genex::count(version, '.') + 1;
      if (version_part_count != 3) {
        std::cerr
          << "Error: Invalid version format in spp.toml. "
          << "Version must follow 'major.minor.patch' format.\n";
        return false;
      }

      // Check the build type is a known one.
      const auto build_type = project->at("build").value<Str>().value_or("");
      if (not compiler::TypeFromName(build_type).has_value()) {
        auto type_names_list = compiler::TypeNames();
        const auto type_names = type_names_list
          | genex::views::join_with(',')
          | genex::to<Str>();
        std::cerr
          << "Error: Invalid build type in spp.toml. "
          << "Build type must be one of {" << type_names << "}.\n";
        return false;
      }

      // Check the dependency records, with the rest of this config file's
      // checks. All checks passed otherwise.
      return compiler::dependencies::Declared(cwd).has_value();
    }

    /// The build type the project's "spp.toml" names. Only call this after
    /// "ValidateProject", which checks the key is there and a known type.
    auto ProjectBuildType() -> compiler::BuildType {
      const auto toml = toml::parse_file(CONFIG_FILE);
      const auto name = toml["project"].as_table()->at("build").value<Str>();
      return *compiler::TypeFromName(*name);
    }

    /// Iterate a directory that may not be there. The optional
    /// folders are only created inside this project, so a
    /// dependency legitimately has none of them and walking one
    /// must not throw.
    auto SafeDirectoryIterator(
      Path const &dir) -> std::filesystem::directory_iterator {
      return std::filesystem::exists(dir)
        ? std::filesystem::directory_iterator(dir)
        : std::filesystem::directory_iterator();
    }

    /// [CHECKED]
    /// Run a compilation and report a mistake in the source being compiled. The
    /// "json" format using the lsp logic, and c++ debug builds don't catch any
    /// errors, because otherwise the debug traceback would be wrong.
    auto CompileReportingErrors(
      compiler::Compiler &c, Str const &message_format = "human") -> bool {
      // Json is asked for by something reading the output rather than someone
      // looking at it, such as the IDEA plugin. Any other plugins can parse the
      // json.
      if (message_format == "json") {
        return lsp::CompileReportingJson(c);
      }

      // Debug builds omit the catch, so that the traceback is correct for
      // proper debugging.
      #if SPP_DEBUG
      return c.Compile();
      #else

      // For normal builds, run the compiler, and catch a syntax-/semantic-based
      // error, print the formatted version and return false, so the correct
      // exit code can be used.
      try {
        return c.Compile();
      }
      catch (utils::errors::AbstractError const &e) {
        std::cerr << e.what() << "\n";
        return false;
      }
      catch (InternalCompilerError const &e) {
        std::cerr << e.what() << "\n";
        return false;
      }
      #endif
    }

    /// [CHECKED]
    /// There are three key checks that need to pass in this load function; the
    /// features need to be loadable from the config file, the versions "floor"
    /// needs to be met, and the target needs to be supported. If all three
    /// pass, then the project features are determined as loadable.
    auto LoadProjectFeatures() -> bool {
      // Create the config errors list to mutate inside the loading functions,
      // and define the config file path for loading.
      auto config_errors = Vec<Str>();
      const auto config_file = std::filesystem::current_path() / CONFIG_FILE;
      const auto target = utils::features::TargetFacts{
        .SafeStack = codegen::DoesTargetSupportSafeStack(),
        .SafeStackCanary = codegen::DoesTargetSupportSafeStackCanary(),
        .IsLinux = codegen::TargetIsLinux(),
        .Folder = codegen::TargetFolderName()
      };

      // The CPU is chosen once the config is known, before anything
      // builds the target machine; "SelectCpu" reports its own error.
      if (
        utils::features::Load(config_file, config_errors)
        and utils::features::CheckFloor(target, config_errors)
        and utils::features::CheckTargetSupport(target, config_errors)) {
        return codegen::SelectCpu(utils::features::Text(utils::features::ConfigKey::TargetFloorCpu).c_str());
      }

      // One (or more) of the loading functions have failed, so print the errors
      // to the console and return false.
      for (auto const &e : config_errors) {
        std::cerr << "Error in spp.toml: " << e << "\n";
      }
      return false;
    }

    /// [CHECKED]
    /// Cap the memory of a command this runs (the program "spp run" starts),
    /// from "SPP_MEMORY_LIMIT_KB" (0 for none): a runaway allocation in the
    /// compiled program then stops it, rather than exhausting the machine. Only
    /// needed until the s++ std allocation API is ironed out and stable.
    auto WithMemoryLimit(Str const &command) -> Str {
      // Nothing can be done for windows. Todo: There is possible scope to use
      // windows.h "CreateJobObjectW" and related APIs. On macOS, "ulimit -v"
      // fails with EINVAL, and the "&&" below would then skip the command.
      #if SPP_PLATFORM_WINDOWS or SPP_PLATFORM_MACOS
      return command;
      #else

      // For UNIX systems, get the limit from the environment variable if
      // provided, otherwise 16GB. As long as the limit > 0, then run the
      // program inside "ulimit".
      const auto limit_kb = utils::env::GetCount(utils::env::Var::MemoryLimitKb)
        .value_or(16ull * 1024 * 1024);
      return limit_kb == 0
        ? command
        : "ulimit -v " + std::to_string(limit_kb) + " && " + command;
      #endif
    }

    /// [CHECKED]
    /// Run a git command, reporting a non-zero exit instead of discarding it.
    /// Wraps all git command calls made from the CLI.
    auto RunGit(Str const &args) -> bool {
      // Form the full command by prepending the "git" part, and then run the
      // command though the local system. Record the returned status.
      const auto command = "git " + args;
      const auto status = utils::shell::Run(command);

      // If the status is not 0, it errored, so print that to terminal, and
      // return "false" from this function.
      if (status != 0) {
        std::cerr << "Error: git failed (" << status << "): " << command << "\n";
        return false;
      }
      return true;
    }

    /// [CHECKED]
    /// Given a url of a repository, extract the host from it. Simple parsing by
    /// character detection at either end.
    auto GetUrlHost(Str const &url) -> Str {
      // Strip a scheme prefix off of the url, searching for "://" and removing
      // everything before it, and those 3 characters too.
      auto rest = StrView(url);
      if (const auto scheme = rest.find("://"); scheme != StrView::npos) {
        rest.remove_prefix(scheme + 3);
      }

      // Only a "user@" before the first "/" is credentials; an "@" further in
      // belongs to the path.
      const auto slash = rest.find('/');
      if (const auto at = rest.find('@');
        at != StrView::npos and (slash == StrView::npos or at < slash)) {
        rest.remove_prefix(at + 1);
      }
      return Str(rest.substr(0, rest.find_first_of(":/?")));
    }

    auto IsRemoteReachable(Str const &url) -> bool {
      // Bypass the check when there is no url being checked (failsafe), or the
      // env var has been provided to bypass network checks.
      if (url.empty()
        or utils::env::IsEnabled(utils::env::Var::NoNetworkCheck)) {
        return true;
      }

      // Otherwise, add arguments to the git command that query the repository
      // host.
      const auto args =
        " -c credential.helper= -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=10 ls-remote --exit-code --heads "
        + utils::shell::Quote(url);

#if SPP_PLATFORM_WINDOWS
      const auto command = "set GIT_TERMINAL_PROMPT=0&& git" + args + " >NUL 2>&1";
#else
      const auto command = "GIT_TERMINAL_PROMPT=0 git" + args + " >/dev/null 2>&1";
#endif
      return utils::shell::Run(command) == 0;
    }
  }
}

/// [CHECKED]
auto spp::cli::RunCli(
  const std::int32_t argc, char **argv) -> std::int32_t {
  // Refuse an unknown or malformed "SPP_" variable before anything reads it: a
  // typo would otherwise silently do nothing.
  auto env_errors = Vec<Str>();
  if (not utils::env::Check(env_errors)) {
    for (auto const &e : env_errors) {
      std::cerr << "Error in the environment: " << e << "\n";
    }
    return 1;
  }

  // Create the CLI object and require that a subcommand is provided.
  auto app = CLI::App("SPP build tool", "spp");
  app.require_subcommand(1);

  // Storage for the commands that can be ran; the build, run and clean can each
  // have a mode and target to operate on.
  auto clean_mode = Str("all");
  auto clean_target = Str();

  // The "spp init" command, ran in an empty directory. It will generate the
  // files and folders needed for a basic project to run. It fails for non-empty
  // directories, unless --force is provided.
  const auto init_cmd = app.add_subcommand(
    "init", "Initialize the new project");
  init_cmd->fallthrough();

  auto init_force = false;
  init_cmd->add_flag(
    "-f,--force", init_force,
    "Initialize even if the directory isn't empty (existing files are kept)");

  init_cmd->callback([&] {
    if (not HandleInit(init_force)) { throw CLI_ERR; }
  });

  // The "spp vcs" command, which can be invoked at any time, to update the
  // dependencies referenced in the [vcs] section of the spp.toml file.
  const auto vcs_cmd = app.add_subcommand(
    "vcs", "Initialize version control for the project");
  vcs_cmd->fallthrough();
  vcs_cmd->callback([] { if (not HandleVcs()) { throw CLI_ERR; } });

  // The "spp build" command, which builds the project. It can accept a few
  // options and flags for the mode/target/error-format etc, and flags for
  // indexing.
  const auto build_cmd = app.add_subcommand(
    "build", "Build the project");
  build_cmd->fallthrough();

  auto build_mode = Str("dev");
  const auto build_cmd_mode_option = build_cmd->add_option(
    "-m,--mode", build_mode, "Build mode");
  build_cmd_mode_option->check(CLI::IsMember(compiler::ModeNames()));
  build_cmd_mode_option->default_val("dev");

  auto build_target = Str();
  const auto build_cmd_target_option = build_cmd->add_option(
    "-t,--target", build_target,
    CMD_HELP_TEXT_BUILD_TARGET);

  auto build_message_format = Str("human");
  const auto build_cmd_msgfmt_option = build_cmd->add_option(
    "--message-format", build_message_format,
    "How errors are reported: 'human' for the annotated source blocks, or "
    "'json' for one diagnostic object per error.");
  build_cmd_msgfmt_option->check(CLI::IsMember({"human", "json"}));
  build_cmd_msgfmt_option->default_val("human");

  auto build_skip_vcs = false;
  build_cmd->add_flag(
    "--skip-vcs", build_skip_vcs,
    "Do not fetch the [vcs] dependencies before building");

  auto build_analyse_only = false;
  build_cmd->add_flag(
    "--analyse-only", build_analyse_only,
    "Stop once the analysis is done: report errors, but no codegen");

  auto build_index_files = Vec<Str>();
  build_cmd->add_option(
    "--index-file", build_index_files,
    "Report what every name in this file resolved to (can give multiple files)."
    "Needs '--message-format=json'");

  auto build_index_project = false;
  build_cmd->add_flag(
    "--index-project", build_index_project,
    "Report what every name in the project's own files resolved to. Needs"
    "'--message-format=json'");

  auto build_stats_file = Str();
  build_cmd->add_option(
    "--emit-stats", build_stats_file,
    "Write a json report of the work each stage did, its time and peak memory, "
    "to this file");

  build_cmd->callback([&] {
    if (not HandleBuild(
      *compiler::ModeFromName(build_mode), build_target, build_skip_vcs,
      build_message_format, build_analyse_only, build_index_files,
      build_index_project, build_stats_file)) {
      throw CLI_ERR;
    }
  });

  // The "spp run" command, which runs the project. It can accept a few options
  // and flags for the mode/target etc.
  auto run_mode = Str("dev");
  const auto run_cmd = app.add_subcommand(
    "run", "Run the project");
  run_cmd->fallthrough();

  const auto run_cmd_mode_option = run_cmd->add_option(
    "-m,--mode", run_mode,
    "Run mode");
  run_cmd_mode_option->check(CLI::IsMember(compiler::ModeNames()));
  run_cmd_mode_option->default_val("dev");

  auto run_target = Str();
  const auto run_cmd_target_option = run_cmd->add_option(
    "-t,--target", run_target,
    CMD_HELP_TEXT_BUILD_TARGET);

  run_cmd->callback([&] {
    if (not HandleRun(*compiler::ModeFromName(run_mode), run_target)) { throw CLI_ERR; }
  });

  // The "spp clean" command, which deletes artefacts created by the build (such
  // as the executable). This will become more important with incremental
  // compilation.
  const auto clean_cmd = app.add_subcommand("clean", "Clean the project");
  clean_cmd->fallthrough();

  const auto clean_cmd_mode_option = clean_cmd->add_option(
    "-m,--mode", clean_mode,
    "Clean mode");
  auto clean_modes = compiler::ModeNames();
  clean_modes.emplace_back("all");
  clean_cmd_mode_option->check(CLI::IsMember(clean_modes));
  clean_cmd_mode_option->default_val("all");

  const auto clean_cmd_target_option = clean_cmd->add_option(
    "-t,--target", clean_target,
    "Only clean this target's tree; every target's by default");

  clean_cmd->callback([&] {
    // "all" is the one accepted value that isn't a mode, so it names none.
    HandleClean(compiler::ModeFromName(clean_mode), clean_target);
  });

  // The "spp test" command, which runs the unit tests from the "tests" folder.
  // This can be configured and tuned, for the current project, its dependencies
  // too, targets and runners.
  const auto test_cmd = app.add_subcommand(
    "test", "Test the project");
  test_cmd->fallthrough();

  auto test_name_filter = Str();
  test_cmd->add_option(
    "-f,--filter", test_name_filter,
    "Only run tests whose fully qualified name contains this");

  auto test_group_filter = Str();
  test_cmd->add_option(
    "-g,--group", test_group_filter,
    "Only run tests in this group");

  auto test_libs = Vec<Str>();
  test_cmd->add_option(
    "-l,--lib", test_libs,
    "Also run the unit tests of this [vcs] library (repeatable)");

  auto test_all_libs = false;
  test_cmd->add_flag(
    "-a,--all-libs", test_all_libs,
    "Also run the unit tests of every [vcs] library");

  auto test_runner = Str();
  test_cmd->add_option(
    "-r,--runner", test_runner,
    "Run the test executable under this command, e.g. \"valgrind "
    "--error-exitcode=9\" (lifts the memory cap)");

  auto test_mode = Str("rel");
  const auto test_cmd_mode_option = test_cmd->add_option(
    "-m,--mode", test_mode,
    "Test mode");
  test_cmd_mode_option->check(CLI::IsMember(compiler::ModeNames()));
  test_cmd_mode_option->default_val("rel");

  auto test_target = Str();
  test_cmd->add_option(
    "-t,--target", test_target,
    CMD_HELP_TEXT_TEST_TARGET);

  test_cmd->callback([&] {
    HandleTest(
      *compiler::ModeFromName(test_mode), test_name_filter, test_group_filter,
      Vec(test_libs), test_all_libs, test_runner, test_target);
  });

  // The "spp validate" command, validates the project structure, and the vcs
  // dependencies too (this is a cheap operation). This also handles the ffi
  // library folders' structures.
  const auto validate_cmd = app.add_subcommand(
    "validate", "Validate the project");
  validate_cmd->fallthrough();
  validate_cmd->callback([] {
    if (not HandleValidate(false) or not compiler::ffi::Validate(
      std::filesystem::current_path())) {
      throw CLI_ERR;
    }
  });

  // The "spp config" command lists all the possible keys available for the
  // "spp.toml" configuration file, and the environment variables. The compiler
  // will error on invalid keys and variables too.
  const auto config_cmd = app.add_subcommand(
    "config",
    "List every section and key 'spp.toml' accepts,"
    "and the environment variables");
  config_cmd->fallthrough();
  config_cmd->callback([] {
    std::cout << utils::features::HelpText() << utils::env::HelpText();
  });

  // The "spp audit" command displays the memory/linking configuration keys that
  // a program was built with. The path to the executable needs to be passed in.
  // This uses a path so that any s++ executable can be checked.
  const auto audit_cmd = app.add_subcommand(
    "audit", "Show what a program spp built was built with");
  audit_cmd->fallthrough();

  auto audit_path = Str();
  const auto audit_cmd_path_option = audit_cmd->add_option(
    "executable", audit_path, "The program to read");
  audit_cmd_path_option->required();
  audit_cmd->callback([&] {
    if (not HandleAudit(audit_path)) { throw CLI::RuntimeError(1); }
  });

  // The "spp version" command displays the current compiler version. This reads
  // the version number out of the generated version file.
  const auto version_cmd = app.add_subcommand(
    "version", "Show version information");
  version_cmd->fallthrough();
  version_cmd->callback(HandleVersion);

  // Parse the command line arguments, and handle different types of CLI11
  // errors that can raise.
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

/// [CHECKED]
auto spp::cli::HandleInit(const bool force) -> bool {
  // If the current directory is empty, then throw an error, so existing folders
  // don't get bloated by accident. The explicit "--force" to bypasses it.
  const auto cwd = std::filesystem::current_path();
  if (not force and not std::filesystem::is_empty(cwd)) {
    std::cerr
      << "Error: The current directory is not empty."
      << "Please run this command in an empty directory, or pass --force.\n";
    return false;
  }

  // Create the directory structure (folders). This is the core project layout.
  // Some folders might not be used, but validation will recreate them.
  std::filesystem::create_directory(cwd / OUT_FOLDER);
  std::filesystem::create_directory(cwd / SRC_FOLDER);
  std::filesystem::create_directory(cwd / VCS_FOLDER);
  std::filesystem::create_directory(cwd / FFI_FOLDER);
  std::filesystem::create_directory(cwd / TST_FOLDER);

  // Within the source folder, add the root folder matching the project name.
  // This creates the root namespace of the package.  "std/src/std/..."
  std::filesystem::create_directory(cwd / SRC_FOLDER / cwd.filename());

  // Fill in "main.spp" and "spp.toml" with template content. A forced init
  // keeps any that already exist.
  if (not std::filesystem::exists(cwd / SRC_FOLDER / MAIN_FILE)) {
    utils::files::WriteFile(
      cwd / SRC_FOLDER / MAIN_FILE,
      MAIN_FILE_CONTENTS);
  }

  if (not std::filesystem::exists(cwd / CONFIG_FILE)) {
    utils::files::WriteFile(
      cwd / CONFIG_FILE,
      CreateDefaultConfigFor(utils::files::DisplayString(cwd.filename())));
  }
  return true;
}

/// [CHECKED]
auto spp::cli::HandleVcs() -> bool {
  // Validate the project structure first. This is a failsafe to prevent
  // downstream errors.
  using namespace std::string_literals;
  SPP_VALIDATE_PROJECT_OR(false, false);

  // Clone or update each dependency, and each of theirs. A project with none
  // has nothing to fetch, which is a successful outcome, not a failure.
  auto reachable = Map<Str, bool>();
  const auto fetch_one = [&](compiler::dependencies::Dependency const &dependency) -> bool {
    auto const &repo_name = dependency.Name;
    auto const &repo_url = dependency.Git;
    auto const &repo_branch = dependency.Branch;
    auto const &repo_folder = dependency.Folder;
    const auto quoted_target = utils::shell::Quote(utils::files::NativeString(repo_folder));
    const auto quoted_branch = utils::shell::Quote(repo_branch);

    // Ping the host before handing it to git, and cache per host, so that later
    // repositories on the same host reuse the answer.
    const auto host = GetUrlHost(repo_url);
    auto host_is_up = [&] {
      auto [cached, first_seen] = reachable.try_emplace(host, false);
      if (first_seen) { cached->second = IsRemoteReachable(repo_url); }
      return cached->second;
    };

    // A pinned dependency is checked out at its commit, detached, and never
    // pulled. One already at it needs no git and no network, so a build with an
    // unchanged pin works offline.
    if (dependency.Rev.has_value()) {
      auto const &rev = *dependency.Rev;
      if (compiler::dependencies::FindCheckedOutRev(repo_folder) == rev) { return true; }

      if (not std::filesystem::exists(repo_folder)) {
        if (not RunGit("clone --no-checkout " + utils::shell::Quote(repo_url) + " " + quoted_target)) {
          if (not host_is_up()) {
            std::cerr
              << "Error: '"s << host << "' is unreachable, and "
              << repo_name << " has not been cloned yet.\n";
          }
          return false;
        }
      }

      // The pinned commit may be newer than the checkout, so fetch first. A
      // host that is down only matters if the commit is not here already,
      // which the checkout then reports.
      else if (not RunGit("-C " + quoted_target + " fetch origin")) {
        if (host_is_up()) { return false; }
        std::cerr
          << "Warning: '"s << host << "' is unreachable; looking for "
          << rev << " in the existing checkout of " << repo_name << ".\n";
      }

      if (not RunGit("-C " + quoted_target + " checkout --quiet --detach " + utils::shell::Quote(rev))) {
        return false;
      }
      std::cout
        << "Checked out "s << repo_name << " from " << repo_url
        << " at " << rev << "\n";
      return true;
    }

    // If the repo doesn't exist locally, then clone it on the correct branch.
    // If the clone fails, check if the host is up or not, and report the error.
    if (not std::filesystem::exists(repo_folder)) {
      if (not RunGit(
        "clone --branch " + quoted_branch + " " + utils::shell::Quote(repo_url) + " " + quoted_target)) {
        if (not host_is_up()) {
          std::cerr
            << "Error: '"s << host << "' is unreachable, and "
            << repo_name << " has not been cloned yet.\n";
        }
        return false;
      }
      std::cout
        << "Cloned "s << repo_name << " from "
        << repo_url << "\n";
    }

    // Otherwise, update the branch (for mutating tags like "master"). Same
    // error system as above; attempt, then check if host is up etc.
    else if (
      not RunGit("-C " + quoted_target + " checkout " + quoted_branch) or
      not RunGit("-C " + quoted_target + " pull origin " + quoted_branch)) {
      if (host_is_up()) { return false; }
      std::cerr
        << "Warning: '"s << host
        << "' is unreachable; using the existing checkout of "
        << repo_name << ".\n";
    }
    else {
      std::cout
        << "Updated "s << repo_name << " from " << repo_url
        << " (" << repo_branch << ")" << "\n";
    }
    return true;
  };
  return compiler::dependencies::Fetch(
    std::filesystem::current_path(), fetch_one);
}

/// [CHECKED]
auto spp::cli::HandleBuild(
  const compiler::BuildMode build_mode, Str const &target, const bool skip_vcs,
  Str const &message_format, const bool analyse_only,
  Vec<Str> const &index_files, const bool index_project,
  Str const &stats_file) -> bool {
  // Validate the project structure first. This is a failsafe to prevent
  // downstream errors. Select the LLVM target, and perform a failsafe assertion
  // on intrinsic names.
  SPP_VALIDATE_PROJECT_OR(false, false);
  if (not codegen::SelectTarget(target.c_str())) { return false; }
  codegen::AssertIntrinsicNamingIsSound();

  // Create the inner directory (rel or dev). Then build the layout that can
  // produce all the paths we need.
  const auto cwd = std::filesystem::current_path();
  const auto out = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = build_mode
  };
  std::filesystem::create_directories(out.OutRoot());

  // If the build were to fail, a stale exe might be picked up for "run".
  // Prevent that by deleting it before, unless the new build is for "analysis
  // only", which must not disturb the executable.
  if (not analyse_only) { std::filesystem::remove(out.ExecutablePath()); }

  // Handle VCS if not skipped. Building against a half-fetched "vcs" folder
  // reports every imported symbol as undefined rather than the fetch failure
  // that caused it, so stop here instead.
  if (not skip_vcs and not HandleVcs()) {
    std::cerr <<
      "Error: Aborting the build; the [vcs] dependencies could not be"
      "fetched.\n";
    return false;
  }

  // Revalidate (after including the VCS folders), and check the ffi packages
  // the fetch brought. Try to load the project features too.
  SPP_VALIDATE_STRUCTURE_OR(false, false);
  if (not compiler::ffi::Validate(std::filesystem::current_path())
    or not LoadProjectFeatures()) {
    return false;
  }

  // Compile the code, as the "lib" or "exe" the toml config says. Call the
  // resolution index hooks depending on what is being indexed (project,
  // specific files, etc).
  auto c = compiler::Compiler(build_mode, ProjectBuildType());

  // If this is an analysis-only build, mark the compiler as that, to restrict
  // the stages, otherwise enable indexing for the marked files.
  c.SetAnalyseOnly(analyse_only);
  if (index_project) { lsp::resolution_index::EnableProject(); }
  else { lsp::resolution_index::EnableFiles(index_files); }

  // The report is written whether or not the build succeeded: how long a
  // project full of errors takes to reject is worth measuring too.
  if (stats_file.empty()) { return CompileReportingErrors(c, message_format); }
  utils::stats::Enable(std::filesystem::absolute(stats_file));
  const auto built = CompileReportingErrors(c, message_format);
  const auto written = utils::stats::Finish(
    compiler::ModeName(build_mode), codegen::TargetFolderName(), built);
  return built and written;
}

/// [CHECKED]
auto spp::cli::HandleRun(
  const compiler::BuildMode mode, Str const &target) -> bool {
  // A library has no entry point, so there is nothing to run. Say so before
  // the fetch and compile, rather than after them as a missing executable.
  SPP_VALIDATE_PROJECT_OR(false, false);
  if (ProjectBuildType() == compiler::BuildType::LIB) {
    std::cerr
      << "Error: This project is a library ('build = \"lib\"' in spp.toml), "
      << "so there is nothing to run. Use 'spp build' to build it, or "
      << "'spp test' to run its tests.\n";
    return false;
  }

  // Build the project first (skip VCS). Don't run the old binary when the
  // current code causes an error.
  if (not HandleBuild(mode, target, false)) { return false; }

  // Nothing this machine can execute comes out of a cross build, so say that
  // rather than reporting the missing executable as though the build had gone
  // wrong.
  if (not codegen::TargetIsHost()) {
    std::cerr
      << "Error: Cannot run a build for '" << codegen::TargetFolderName()
      << "'; only a build for the host can be executed here.\n";
    return false;
  }

  // A build that did not get as far as linking has said why already, so there
  // is nothing to add here beyond not trying to run something that was never
  // produced.
  const auto cwd = std::filesystem::current_path();
  const auto exe_file = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = mode
  }.ExecutablePath();
  if (not std::filesystem::exists(exe_file)) {
    std::cerr
      << "Error: No executable was built at '"
      << utils::files::DisplayString(exe_file) << "'.\n";
    return false;
  }

  // Pull the returned status code from the run process (ie the compiled s++
  // code), and exit with that code, after displaying the output message.
  std::cout
    << "Running: " << utils::files::DisplayString(exe_file)
    << std::endl;
  std::cout.flush();

  // Reset the console, so the program's own output is all that is on the
  // console, rather than the leftover of the build steps that produced it.
  std::cout << "\033[H\033[2J\033[3J";
  std::cout.flush();

  // Use the memory bound until the stl is stable; todo: also wire to a flag?
  // There have been instances where allocation issues mangled the WSL instance.
  const auto status = utils::shell::Run(
    WithMemoryLimit(utils::shell::Quote(utils::files::NativeString(exe_file))));
  const auto exited_normally = sys::WIFEXITED(status);
  const auto exit_code = exited_normally
    ? sys::WEXITSTATUS(status)
    : status;

  // Print the information about the program termination, and exit with the
  // correct code. For an exit code of "0", there will be nothing printed, and
  // a normal clean successful exit.
  if (not exited_normally) {
    std::cerr << "Program terminated abnormally (status " << status << ").\n";
  }
  else if (exit_code != 0) {
    std::cerr << "Program exited with code " << exit_code << ".\n";
  }
  std::exit(exit_code);
  std::unreachable();
}

/// [CHECKED]
auto spp::cli::HandleClean(
  const Opt<compiler::BuildMode> mode, Str const &target) -> void {
  // Validate the project structure first.
  SPP_VALIDATE_PROJECT(false);

  // If a target was explicitly provided, select it via the codegen module, and
  // record the folder name.
  const auto cwd = std::filesystem::current_path();
  auto targets = Vec<Str>();
  if (not target.empty()) {
    if (not codegen::SelectTarget(target.c_str())) { return; }
    targets.emplace_back(codegen::TargetFolderName());
  }

  // Otherwise, this is a "clean all targets", so iterate the "out" artefact
  // folder, and emplace every target folder into the list.
  else {
    for (auto const &entry : SafeDirectoryIterator(cwd / OUT_FOLDER)) {
      if (!entry.is_directory()) { continue; }
      targets.emplace_back(
        utils::files::NativeString(entry.path().filename()));
    }
  }

  // Remove the appropriate folders. Every build's artifacts live under its own
  // target and mode folder, so removing that is the whole of the clean - there
  // is nothing left outside it.
  for (auto const &t : targets) {
    for (const auto known : magic_enum::enum_values<compiler::BuildMode>()) {
      if (mode.has_value() and *mode != known) { continue; }
      std::filesystem::remove_all(
        compiler::OutLayout{.Root = cwd, .Target = t, .Mode = known}.OutRoot());
    }

    // A target folder with no modes left in it is noise, so it goes too - but
    // only when it is genuinely empty, because a "clean -m dev" leaves the
    // "rel" build standing.
    const auto target_root = cwd / OUT_FOLDER / t;
    if (std::filesystem::exists(target_root)
      and std::filesystem::is_empty(target_root)) {
      std::filesystem::remove(target_root);
    }
  }
}

/// [CHECKED] [TIDY]
auto spp::cli::HandleTest(
  const compiler::BuildMode mode, Str const &name_filter,
  Str const &group_filter, Vec<Str> const &libs, const bool all_libs,
  Str const &runner, Str const &target) -> void {
  // Validate the project structure first.
  SPP_VALIDATE_PROJECT(false);

  // Choose the target before anything is compiled, as a build does. Nothing
  // this machine can execute comes out of a cross build, so one is only tested
  // under a runner that can (QEMU etc).
  if (not codegen::SelectTarget(target.c_str())) { std::exit(1); }
  if (not codegen::TargetIsHost() and runner.empty()) {
    std::cerr
      << "Error: Cannot run tests built for '"
      << codegen::TargetFolderName()
      << "' here; give a '--runner' that can, "
      << "e.g. \"qemu-<arch> -L <sysroot>\".\n";
    std::exit(1);
  }

  // Fetch the dependencies, as an ordinary build does: a test build compiles
  // the same sources plus the "tst" folder.
  if (not HandleVcs()) {
    std::cerr
      << "Error: Aborting the test build; "
      << "the [vcs] dependencies could not be fetched.\n";
    return;
  }
  SPP_VALIDATE_STRUCTURE(false);
  if (not compiler::ffi::Validate(std::filesystem::current_path())) {
    std::exit(1);
  }
  if (not LoadProjectFeatures()) { std::exit(1); }

  // A test build compiles in "mode", so it must look for its executable there
  // too - see the Compiler constructed below.
  const auto cwd = std::filesystem::current_path();
  const auto out = compiler::OutLayout{
    .Root = cwd, .Target = codegen::TargetFolderName(), .Mode = mode
  };
  std::filesystem::create_directories(out.OutRoot());

  // Remove the executable before building, so a build that fails to link cannot
  // leave the previous one behind to be run and reported as a pass.
  const auto exe_file = out.ExecutablePath();
  std::filesystem::remove(exe_file);

  /// Manually validate the selected libraries to test. The test command fails
  /// completely if an invalid library is included.
  const auto dependencies = compiler::dependencies::Resolve(cwd);
  for (auto const &lib : libs) {
    if (genex::any_of(dependencies, [&](auto const &d) { return d.Name == lib; })) { continue; }
    std::cerr
      << "Error: No library named '" << lib
      << "' in the project's [" << VCS_FOLDER << "].\n";
    std::exit(1);
  }

  /// Create a test scope with the provided library information from the flags,
  /// and invoke a compiler instance with the scope, in the chosen mode and as an
  /// executable.
  const auto scope = compiler::TestScope{
    .Project = true, .AllLibs = all_libs, .Libs = libs
  };
  auto c = compiler::Compiler(
    mode, compiler::BuildType::EXE, scope);
  c.SetTestFilters(name_filter, group_filter);
  if (not CompileReportingErrors(c)) { std::exit(1); }

  // Special case for 0 tests being found in the test selection. This could be
  // to do with no tests defined, or an over-constrained filter.
  if (c.TestCount() == 0) {
    std::cerr << "No unit tests matched. Mark a function in 'tst' with '!test'";
    if (not name_filter.empty() or not group_filter.empty()) {
      std::cerr << ", or relax the filter";
    }
    std::cerr << ".\n";
    std::exit(1);
  }

  // Failsafe for the test executable failing to appear. This is needed as it
  // will be ran after being built.
  if (not std::filesystem::exists(exe_file)) {
    std::cerr
      << "Error: No test executable was built at '"
      << utils::files::DisplayString(exe_file) << "'.\n";
    std::exit(1);
  }

  // Prep the test runner, printing some debug info before the built test exe is
  // ran. Generate the exe path from the file too.
  std::cout << "Running " << c.TestCount() << " test(s)." << std::endl;
  std::cout.flush();
  const auto exe = utils::shell::Quote(utils::files::NativeString(exe_file));

  // The selector is set on the command rather than in this process, so nothing
  // has to be unset afterwards and the whole-suite run is plain. A runner
  // ("valgrind ...") goes in front of the executable.
  const auto run = [&exe, &runner](Str const &only) {
    // The target is the raw executable file for no special runner, otherwise
    // it's the runner followed by the exe.
    const auto target = runner.empty() ? exe : runner + " " + exe;
    const auto name = Str(utils::env::NameOf(utils::env::Var::TestOnly));
#if SPP_PLATFORM_WINDOWS
    const auto command = only.empty() ? target : "set \"" + name + "=" + only + "\"&& " + target;
#else
    const auto command = only.empty() ? target : name + "=" + utils::shell::Quote(only) + " " + target;
#endif

    // Invoke the command (adding the memory limit for a raw executable). Do the
    // normal status checks and return the parsed exit code.
    const auto status = utils::shell::Run(runner.empty() ? WithMemoryLimit(command) : command);
    return sys::WIFEXITED(status) ? sys::WEXITSTATUS(status) : -1;
  };

  // Run the tests, and check for a exit status of 0 (this can include failed
  // tests, but it means all the tests ran without crashing / aborting).
  if (run("") == 0) {
    std::cout << "All " << c.TestCount() << " test(s) passed." << std::endl;
    return;
  }

  // Otherwise, something took the test run down. As an assertion or a
  // miscompilation would have aborted the process, everything ordered behind
  // it will have never ran. Re-run them a process each, so one casualty costs
  // one result rather than the rest of the suite.
  std::cout
    << "\nA test did not finish. Re-running each test in its own process.\n"
    << std::endl;
  std::cout.flush();

  // Collect the crashing tests by running one at a time and collecting the exit
  // code.
  auto failed = Vec<Str>();
  for (auto const &name : c.TestNames()) {
    if (run(name) != 0) { failed.emplace_back(name); }
  }

  // Dump all the failed test information.
  std::cout
    << "\n" << (c.TestCount() - failed.size()) << "/" << c.TestCount()
    << " test(s) passed." << std::endl;
  if (failed.empty()) {
    // Every test passes on its own, so what failed is something they share
    // rather than any one of them.
    std::cerr
      << "The suite failed as a whole but every test passes alone - "
      << "suspect shared state or the harness.\n";
    std::exit(1);
  }

  std::cerr << "Failed:\n";
  for (auto const &name : failed) { std::cerr << "  " << name << "\n"; }
  std::exit(1);
}

auto spp::cli::HandleValidate(
  const bool is_exe) -> bool {
  // The project, then that each dependency is well declared and fetched, then
  // each dependency's own structure. A dependency that fails stops the build,
  // as compiling against it reports errors far from their cause.
  if (not ValidateProject(is_exe, true)) { return false; }
  const auto cwd = std::filesystem::current_path();
  if (not compiler::dependencies::Validate(cwd)) { return false; }

  for (auto const &dependency : compiler::dependencies::Resolve(cwd)) {
    std::filesystem::current_path(dependency.Folder);
    const auto valid = ValidateProject(false, false);
    std::filesystem::current_path(cwd);
    if (valid) { continue; }
    std::cerr
      << "Error: the dependency '" << dependency.Name
      << "' is not valid.\n";
    return false;
  }
  return true;
}

/// [CHECKED]
auto spp::cli::HandleAudit(Str const &path) -> bool {
  // The record is written by every build, into the executable. A program
  // without one predates it or was not built by spp.
  const auto record = codegen::ReadHardeningRecord(path.c_str());

  // If the outermost "expected" doesn't have a value, then the path doesn't
  // point to an executable file at all.
  if (not record.has_value()) {
    std::cerr
      << "Error: '" << path << "' cannot be read as a program: "
      << record.error() << "\n";
    return false;
  }

  // If the inner "optional" doesn't have a value, then whilst the file is an
  // executable, a hardness record is not present (non-spp, old spp, etc.)
  if (not record->has_value()) {
    std::cerr
      << "Error: '" << path << "' has no hardening record.\n"
      << "  It was not built by spp, or by an pre-audited spp.\n";
    return false;
  }

  // Print the record (inside optional, inside expected), and return "true" - no
  // errors reached at this point.
  std::cout << **record;
  return true;
}

/// [CHECKED]
auto spp::cli::HandleVersion() -> void {
  // Print the version information. This is from a generated file, parsed from
  // VERSION file (always consistent). The LLVM is the one built in, which a
  // benchmark record needs: the same pin can mean different minor versions.
  std::cout << "SPP version " << SPP_VERSION << "\n";
  std::cout << "LLVM version " << LLVM_VERSION_STRING << "\n";
}

/// [CHECKED]
auto spp::cli::CreateDefaultConfigFor(Str const &project_name) -> Str {
  // Inject the project name into the config template, and return it. This is
  // the default "spp.toml" for a project.
  auto contents = CONFIG_FILE_CONTENTS;
  const auto pos = contents.find('$');
  contents.replace(pos, 1, project_name);
  return contents;
}

/// [CHECKED]
auto spp::cli::RunCppGoogleTest(
  const compiler::BuildMode mode, Str &&main_code) -> Map<Str, Str> {
#if !defined(SPP_TEST_TEARDOWN)
  // A test process almost always compiles once and exits, and freeing a compile
  // is about 7% of its run. So the last compile is freed only when another one
  // is about to start, and otherwise left to the OS: never destroyed, as a
  // static would be at exit. Builds that exist to see destructors run define
  // SPP_TEST_TEARDOWN.
  static auto &previous = *new Unique<compiler::Compiler>();
  previous.reset();
#endif

  // Validate the project structure first.
  SPP_VALIDATE_STRUCTURE_OR(false, {});

  // Create the inner directory (rel or dev).
  const auto cwd = std::filesystem::current_path();
  const auto target = codegen::TargetFolderName();
  std::filesystem::create_directories(
    compiler::OutLayout{.Root = cwd, .Target = target, .Mode = mode}.OutRoot());
  SPP_VALIDATE_STRUCTURE_OR(false, {});

  // Compile the code. It is held before it runs, as a rejected program throws.
#if defined(SPP_TEST_TEARDOWN)
  const auto c = compiler::Compiler::ForCppGoogleTest(
    mode, std::move(main_code));
  ignore = c->Compile();
  return c->CompTimeConstants();
#else
  previous = compiler::Compiler::ForCppGoogleTest(
    mode, std::move(main_code));
  ignore = previous->Compile();
  return previous->CompTimeConstants();
#endif
}
