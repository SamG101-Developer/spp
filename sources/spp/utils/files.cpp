module spp.utils.files;
import std;
import sys;

auto spp::utils::files::ReadFile(
    std::filesystem::path const &path)
    -> Str {
    // Create an input file stream.
    auto in = std::ifstream(path, std::ios::in);
    auto buf = std::ostringstream();
    buf << in.rdbuf();
    return buf.str();
}

auto spp::utils::files::WriteFile(
    std::filesystem::path const &path,
    Str const &content)
    -> void {
    // Create an output file stream.
    auto out = std::ofstream(path);
    out << content;
}

auto spp::utils::files::GlobSpp(
    std::string const &path)
    -> Vec<std::filesystem::path> {
    // Use the filesystem iterator to recursively walk the path, finding all ".spp" files.
    auto paths = Vec<std::filesystem::path>();
    for (auto const &entry : std::filesystem::recursive_directory_iterator(path)) {
        const auto full_name = entry.path().string();
        if (not entry.is_regular_file()) { continue; }
        if (entry.path().extension().string() != ".spp") { continue; }
        paths.EmplaceBack(full_name);
    }
    return paths;
}
