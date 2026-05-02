module;
#include <spp/macros.hpp>

export module spp.cli2;
import spp.utils.types;
import cli11;
import std;

auto operator""_formatted(
    const char *str,
    std::size_t len)
    -> spp::Str;

auto SystemSharedObjectExtension()
    -> spp::Str;

auto FormatDefaultContents(
    spp::StrView contents)
    -> spp::Str;

namespace spp::cli2 {
    SPP_EXP_CLS enum class Mode { REL, DEV };

    SPP_EXP_CLS enum class Type { EXE, LIB };

    struct BuildOptions {
        Mode mode;
        Type type;
        bool skip_vcs_update;
    };

    SPP_EXP_FUN auto Run(
        std::int32_t argc,
        char **argv)
        -> std::int32_t;

    SPP_EXP_FUN auto run_unit_test(
        BuildOptions &&options,
        Str &&code)
        -> void;

    auto init(
        Str &&root_dir)
        -> void;

    auto build(
        BuildOptions &&options)
        -> void;

    auto run(
        BuildOptions &&options)
        -> void;

    auto clean(
        BuildOptions &&options)
        -> void;

    auto version()
        -> void;

    auto validate()
        -> void;

    auto setup_toml_config(
        BuildOptions const &options)
        -> Str;
}
