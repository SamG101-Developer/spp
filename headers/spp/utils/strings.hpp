#ifndef STRINGS_HPP
#define STRINGS_HPP

#include <unicode/unistr.h>


namespace spp::utils::strings {
    auto snake_to_pascal(icu::UnicodeString const &) -> icu::UnicodeString;
}


#endif //STRINGS_HPP
