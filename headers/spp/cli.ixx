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

  SPP_EXP_FUN auto handle_vcs()
    -> void;

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

  SPP_EXP_FUN auto handle_test()
    -> void;

  SPP_EXP_FUN auto handle_validate(
    bool is_exe)
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
  SPP_EXP_FUN auto unit_test(
    Str const &mode,
    Str &&main_code)
    -> Map<Str, Str>;

  auto format_default_file_contents(
    StrView contents)
    -> Str;
}
