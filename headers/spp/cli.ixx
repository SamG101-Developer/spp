module;
#include <spp/macros.hpp>

export module spp.cli;
import spp.utils.types;
import cli11;
import std;

namespace spp::cli {
  SPP_EXP_FUN auto run_cli(std::int32_t argc, char **argv) -> std::int32_t;

  /**
   * Initialize the current directory as a new S++ project. This creates a default set of files and directories
   * requires to build and run a new S++ project.
   */
  SPP_EXP_FUN auto handle_init()
    -> void;

  /**
   * Clone or update every repository in the project's [vcs] section into the "vcs" folder.
   * @return @c true when every repository was fetched; @c false when any git invocation failed or a repository is
   * missing afterwards. A caller that compiles against these sources must not proceed on @c false, because an empty
   * "vcs" folder still passes @c handle_validate and only shows up as every imported symbol being undefined.
   */
  SPP_EXP_FUN auto handle_vcs()
    -> bool;

  SPP_EXP_FUN auto handle_build(
    Str const &mode,
    bool skip_vcs = false)
    -> void;

  SPP_EXP_FUN auto handle_run(
    Str const &mode)
    -> void;

  SPP_EXP_FUN auto handle_clean(
    Str const &mode)
    -> void;

  /**
   * Build the project together with its "tst" folder and run the unit tests it declares.
   * @param[in] name_filter Only run tests whose fully qualified name contains this; empty runs all.
   * @param[in] group_filter Only run tests in this group; empty runs all.
   * @param[in] libs Also run the unit tests of these [vcs] libraries, named as their folder under "vcs".
   * @param[in] all_libs Also run the unit tests of every [vcs] library.
   */
  SPP_EXP_FUN auto handle_test(
    Str const &name_filter = "",
    Str const &group_filter = "",
    Vec<Str> const &libs = {},
    bool all_libs = false)
    -> void;

  /**
   * Check the current directory has the structure a project needs, reporting anything missing.
   * @param[in] is_exe Whether this project is entered through a "src/main.spp"; a library is not.
   * @param[in] create_missing Whether to create the optional folders ("out", "vcs", "ffi", "tst") that are absent.
   * False when checking somebody else's project - a dependency's checkout is not this build's to write into.
   * @return @c true when the structure is sound.
   */
  SPP_EXP_FUN auto handle_validate(
    bool is_exe,
    bool create_missing = true)
    -> bool;

  SPP_EXP_FUN auto handle_version()
    -> void;

  SPP_EXP_FUN auto create_default_config_for(
    Str const &project_name)
    -> Str;

  SPP_EXP_FUN auto get_system_shared_library_extension()
    -> Str;

  /**
   * Compile a single module of code as a throwaway project, for the test suite.
   * @param mode The build mode ("dev" or "rel").
   * @param main_code The source of the module to compile.
   * @return The values the module's compile-time constants resolved to, by name; see
   * @c Compiler::CompTimeConstants . Empty when the module declares none.
   */
  SPP_EXP_FUN auto run_cpp_google_test(
    Str const &mode,
    Str &&main_code)
    -> Map<Str, Str>;

  auto format_default_file_contents(
    StrView contents)
    -> Str;
}
