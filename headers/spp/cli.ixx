module;
#include <spp/macros.hpp>

export module spp.cli;
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
        std::string const &mode,
        bool skip_vcs = false)
        -> void;

    SPP_EXP_FUN auto handle_run(
        std::string const &mode)
        -> void;

    SPP_EXP_FUN auto handle_clean(
        std::string const &mode)
        -> void;

    SPP_EXP_FUN auto handle_test()
        -> void;

    SPP_EXP_FUN auto handle_validate()
        -> bool;

    SPP_EXP_FUN auto handle_version()
        -> void;

    SPP_EXP_FUN auto create_default_config_for(
        std::string const &project_name)
        -> std::string;

    SPP_EXP_FUN auto get_system_shared_library_extension()
        -> std::string;
}
