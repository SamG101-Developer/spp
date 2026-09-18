module;
#include <spp/macros.hpp>

export module spp.cli;
import spp.utils.types;
import std;

namespace spp::cli {
  SPP_EXP_FUN auto run_cli(std::int32_t argc, char **argv) -> std::int32_t;

  /// Initialise the current directory as a new S++ project,
  /// creating the default files and directories needed to
  /// build and run it.
  SPP_EXP_FUN auto handle_init()
    -> void;

  /// Clone or update every repository in the project's [vcs]
  /// section into the "vcs" folder. Answers false when any git
  /// invocation failed or a repository is missing afterwards.
  /// A caller compiling against these sources must not go on
  /// after false: an empty "vcs" folder still passes
  /// "handle_validate", and only shows up as every imported
  /// symbol being undefined.
  SPP_EXP_FUN auto handle_vcs()
    -> bool;

  /// Compile the project. "mode" is "dev" or "rel", selecting
  /// the optimisation pipeline and naming the folder built
  /// into. "target" is a target triple, or empty for the host;
  /// a foreign target is compiled and an object emitted, but
  /// not linked - the linker driver is the host's, and the ffi
  /// runtimes a project ships are host objects, so there is
  /// nothing to link a foreign object against. "skip_vcs"
  /// skips fetching the [vcs] dependencies first.
  ///
  /// Answers whether the build ran to completion. Every
  /// failure has already said what it was, so a caller reports
  /// nothing further - but it has to ask, because "an
  /// executable is there" is a different question: a build
  /// that stopped before clearing the last one away leaves one
  /// behind that has nothing to do with this build.
  SPP_EXP_FUN auto handle_build(
    Str const &mode,
    Str const &target = "",
    bool skip_vcs = false)
    -> bool;

  /// Compile the project and run what it produced. Only a host
  /// build can be run. "mode" is "dev" or "rel", and "target"
  /// a target triple, or empty for the host. Only ever answers
  /// false, and only when there was nothing to run: once the
  /// program runs, its own status is what this exits with, so
  /// returning at all means the run did not happen.
  SPP_EXP_FUN auto handle_run(
    Str const &mode,
    Str const &target = "")
    -> bool;

  /// Remove built artifacts. "mode" is "dev", "rel" or "all".
  /// "target" limits the clean to that target's tree; empty
  /// cleans every target that has been built here.
  SPP_EXP_FUN auto handle_clean(
    Str const &mode,
    Str const &target = "")
    -> void;

  /// Build the project together with its "tst" folder and run
  /// the unit tests it declares. "name_filter" keeps tests
  /// whose fully qualified name contains it, and
  /// "group_filter" keeps one group; empty runs all. "libs"
  /// also runs the tests of those [vcs] libraries, named by
  /// their folder under "vcs", and "all_libs" runs every [vcs]
  /// library's tests.
  SPP_EXP_FUN auto handle_test(
    Str const &name_filter = "",
    Str const &group_filter = "",
    Vec<Str> const &libs = {},
    bool all_libs = false)
    -> void;

  /// Check the current directory has the structure a project
  /// needs, reporting anything missing, and answer whether it
  /// is sound. "is_exe" is whether the project is entered
  /// through a "src/main.spp" (a library is not).
  /// "create_missing" creates the optional folders ("out",
  /// "vcs", "ffi", "tst") that are absent; it is false when
  /// checking someone else's project, because a dependency's
  /// checkout is not this build's to write into.
  SPP_EXP_FUN auto handle_validate(
    bool is_exe,
    bool create_missing = true)
    -> bool;

  SPP_EXP_FUN auto handle_version()
    -> void;

  SPP_EXP_FUN auto create_default_config_for(
    Str const &project_name)
    -> Str;

  /// Compile a single module of code as a throwaway project,
  /// for the test suite, in build mode "dev" or "rel". Answers
  /// the values the module's compile-time constants resolved
  /// to, by name (see "Compiler::CompTimeConstants"); empty
  /// when the module declares none.
  SPP_EXP_FUN auto run_cpp_google_test(
    Str const &mode,
    Str &&main_code)
    -> Map<Str, Str>;

  auto format_default_file_contents(
    StrView contents)
    -> Str;
}
