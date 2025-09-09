#include <fstream>
#include <spp/utils/files.hpp>

#include <opex/cast.hpp>


auto spp::utils::files::read_file(
    std::filesystem::path const &path)
    -> std::string {
    // Create an input file stream.
    auto file = std::ifstream(path);
    const auto size = std::filesystem::file_size(path);

    // Read the entire file into a string.
    auto content = std::string(size, '\0');
    file.read(content.data(), size as S64);
    return content;
}


auto spp::utils::files::write_file(
    std::filesystem::path const &path,
    std::string const &content)
    -> void {
    // Create an output file stream.
    auto file = std::ofstream(path);
    file << content;
}
