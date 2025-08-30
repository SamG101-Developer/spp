#include <unicode/uchar.h>

#include <spp/utils/strings.hpp>

#include <rapidfuzz/fuzz.hpp>


auto spp::utils::strings::snake_to_pascal(
    std::string const &str)
    -> std::string {
    auto out = std::string();
    auto caps = false;
    for (auto i = 0uz; i < str.length(); ++i) {
        const auto c = str[i];
        if (c == '_') {
            caps = true;
            continue;
        }
        if (caps) {
            out.push_back(static_cast<char>(std::toupper(c)));
            caps = false;
        }
        else {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}


auto spp::utils::strings::closest_match(
    std::string const &query,
    std::vector<std::string> const &choices)
    -> std::optional<std::string> {
    auto match_found = false;
    auto best_score = 0.0;
    auto best_match = std::string();

    const auto scorer = rapidfuzz::fuzz::CachedRatio(query);
    for (const auto &choice : choices) {
        const auto score = scorer.similarity(choice, best_score);
        if (score > best_score) {
            match_found = true;
            best_score = score;
            best_match = choice;
        }
    }

    return match_found ? std::make_optional(best_match) : std::nullopt;
}
