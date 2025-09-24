#pragma once
#include <cstdint>
#include <CLI/CLI.hpp>

#define SPP_VERSION "0.1.0"


/// @cond
namespace spp::cli {
    auto run_cli(std::int32_t argc, char **argv) -> std::int32_t;

    /**
     * Initialize the current directory as a new S++ project. This creates a default set of files and directories
     * requires to build and run a new S++ project.
     */
    auto handle_init() -> void;
    auto handle_vcs() -> void;
    auto handle_build(std::string const &mode, bool skip_vcs = false) -> void;
    auto handle_run(std::string const &mode) -> void;
    auto handle_clean(std::string const &mode) -> void;
    auto handle_test() -> void;
    auto handle_validate() -> bool;
    auto handle_version() -> void;

    auto create_default_config_for(std::string const &project_name) -> std::string;
    auto get_system_shared_library_extension() -> std::string;
}

/// @endcond
