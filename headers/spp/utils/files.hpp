#pragma once
#include <filesystem>


/// @cond
namespace spp::utils::files {
    /**
     * Simple function to read the contents of a file into a string.
     * @param path The path to the file to read.
     * @return The contents of the file as a string.
     */
    auto read_file(std::filesystem::path const &path) -> std::string;

    /**
     * Simple function to write a string to a file.
     * @param path The path to the file to write to.
     * @param content The content to write to the file.
     */
    auto write_file(std::filesystem::path const &path, std::string const &content) -> void;
}
/// @endcond
