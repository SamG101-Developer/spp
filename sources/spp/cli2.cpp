module;
#include <spp/macros.hpp>

module spp.cli2;
import spp.utils.types;
import cli11;
import genex;

inline constexpr spp::Str OUT_FOLDER_NAME = "out";
inline constexpr spp::Str SRC_FOLDER_NAME = "src";
inline constexpr spp::Str VCS_FOLDER_NAME = "vcs";
inline constexpr spp::Str FFI_FOLDER_NAME = "ffi";
inline constexpr spp::Str MAIN_FILE_NAME = "main.spp";
inline constexpr spp::Str CONFIG_FILE_NAME = "spp.toml";

inline const auto MAIN_FILE_CONTENTS = R"(
    fun main(args: Vec[Str]) -> Void {
        std::io::println("Hello world!")
    })"_formatted;

inline const auto CONFIG_FILE_CONTENTS = R"(
    [project]
    name = "$"
    version = "0.1.0"
    build = "exe"

    [vcs]
    std = { git = "https://github.com/SamG101-Developer/SPP-STL", branch = "master" })"_formatted;

auto operator""_formatted(
    const char *str,
    const std::size_t len)
    -> spp::Str {
    return FormatDefaultContents(spp::StrView(str, len));
}

auto FormatDefaultContents(
    const spp::StrView contents)
    -> spp::Str {
    // Remove the first newline, and replace "    " with "".
    auto out = spp::Str();
    for (auto const &line : contents | genex::views::split('\n')) {
        if (line.size() <= 0) { continue; }
        auto formatted = line | genex::to<spp::Str>();
        formatted.replace(formatted.find("    "), 4, "");
    }
    return out;
}

auto spp::cli2::Run(
    std::int32_t,
    char **)
    -> std::int32_t {
    // CLI object for parsing the subcommands.
    // auto app
    return 0;
}
