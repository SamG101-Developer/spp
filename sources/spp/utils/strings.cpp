#include <unicode/uchar.h>

#include <spp/utils/strings.hpp>


auto spp::utils::strings::snake_to_pascal(std::string const &str) -> std::string {
    auto out = std::string();
    auto caps = false;
    for (auto i = 0uz; i < str.length(); ++i) {
        const auto c = str[i];
        if (c == '_') {
            caps = true;
            continue;
        }
        if (caps) {
            out.push_back(std::toupper(c));
            caps = false;
        } else {
            out.push_back(std::tolower(c));
        }
    }
    return out;
}
