#define _GLIBCXX_DEBUG

#include <spp/cli.hpp>


int main(const std::int32_t, char **argv) {
    std::filesystem::current_path(std::filesystem::absolute(argv[0]).parent_path().parent_path() / "project");
    spp::cli::handle_build("dev");
    return 0;
}
