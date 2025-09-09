#pragma once
#include <filesystem>


namespace spp::utils::files {
    auto read_file(std::filesystem::path const &path) -> std::string;
    auto write_file(std::filesystem::path const &path, std::string const &content) -> void;
}
