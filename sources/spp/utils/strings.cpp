#include <unicode/uchar.h>

#include <spp/utils/strings.hpp>


auto spp::utils::strings::snake_to_pascal(icu::UnicodeString const &str) -> icu::UnicodeString {
    auto out = icu::UnicodeString();
    auto caps = false;
    for (auto i = 0; i < str.length(); ++i) {
        const auto c = str.charAt(i);
        if (c == '_') {
            caps = true;
            continue;
        }
        if (caps) {
            out.append(u_toupper(c));
            caps = false;
        } else {
            out.append(u_tolower(c));
        }
    }
    return out;
}
