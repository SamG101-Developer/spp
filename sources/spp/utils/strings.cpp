module spp.utils.strings;


auto spp::utils::strings::is_alphanumeric(
    const char c)
    -> bool {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or (c == '_');
}


auto spp::utils::strings::snake_to_pascal(
    std::string const &str)
    -> std::string {
    auto out = std::string();
    auto caps = true;
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
    const std::string_view query,
    std::vector<std::string> const &choices)
    -> std::optional<std::string> {
    auto match_found = false;
    auto best_score = 0.0;
    auto best_match = std::string();

    for (const auto &choice : choices) {
        const auto score = similarity_ratio(query, choice);
        if (score > best_score) {
            best_score = score;
            best_match = choice;
            match_found = true;
        }
    }

    return match_found ? std::make_optional(best_match) : std::nullopt;
}


auto spp::utils::strings::levenshtein(
    const std::string_view s1,
    const std::string_view s2)
    -> size_t {
    const auto m = s1.size();
    const auto n = s2.size();

    auto prev = std::vector<size_t>(n + 1);
    auto curr = std::vector<size_t>(n + 1);

    for (auto j = 0uz; j <= n; ++j) {
        prev[j] = j;
    }

    for (auto i = 0uz; i < m; ++i) {
        curr[0] = i + 1;
        for (auto j = 0uz; j < n; ++j) {
            const auto cost = (s1[i] == s2[j]) ? 0 : 1;
            curr[j + 1] = std::min({curr[j] + 1, prev[j + 1] + 1, prev[j] + cost});
        }
        std::swap(prev, curr);
    }

    return prev[n];
}


auto spp::utils::strings::similarity_ratio(
    const std::string_view s1,
    const std::string_view s2)
    -> double {
    const auto max_len = std::max(s1.size(), s2.size());
    if (max_len == 0) { return 1.0; }

    const auto distance = levenshtein(s1, s2);
    return 1.0 - static_cast<double>(distance) / static_cast<double>(max_len);
}
