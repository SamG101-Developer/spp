module spp.utils.strings;

auto spp::utils::strings::IsAlNum(
    const char c)
    -> bool {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or (c == '_');
}

auto spp::utils::strings::SnakeToPascal(
    Str const &str)
    -> Str {
    auto out = Str();
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

auto spp::utils::strings::ClosestMatch(
    const StrView query,
    Vec<Str> const &choices)
    -> std::optional<Str> {
    auto match_found = false;
    auto best_score = 0.0;
    auto best_match = Str();

    for (auto const &choice : choices) {
        const auto score = SimilarityRatio(query, choice);
        if (score > best_score) {
            best_score = score;
            best_match = choice;
            match_found = true;
        }
    }

    return match_found ? std::make_optional(best_match) : std::nullopt;
}

auto spp::utils::strings::Levenshtein(
    const StrView s1,
    const StrView s2)
    -> std::size_t {
    const auto m = s1.length();
    const auto n = s2.length();

    auto prev = Vec<std::size_t>(n + 1);
    auto curr = Vec<std::size_t>(n + 1);
    for (auto j = 0uz; j <= n; ++j) {
        prev[j] = j;
    }

    for (auto i = 0uz; i < m; ++i) {
        curr[0] = i + 1;
        for (auto j = 0uz; j < n; ++j) {
            const auto cost = (s1[i] == s2[j]) ? 0uz : 1uz;
            curr[j + 1] = std::min({curr[j] + 1, prev[j + 1] + 1, prev[j] + cost});
        }
        std::swap(prev, curr);
    }

    return prev[n];
}

auto spp::utils::strings::SimilarityRatio(
    const StrView s1,
    const StrView s2)
    -> double {
    const auto max_len = std::max(s1.length(), s2.length());
    if (max_len == 0) { return 1.0; }

    const auto distance = Levenshtein(s1, s2);
    return 1.0 - static_cast<double>(distance) / static_cast<double>(max_len);
}

auto spp::utils::strings::NormaliseIntegerString(
    const StrView s1)
    -> boost::BigInt {
    // Normalise the input string by removing underscores.
    const auto out = NormaliseAnyString(s1);
    return boost::BigInt(std::move(out));
}

auto spp::utils::strings::NormalizeFloatString(
    const StrView s1,
    const StrView s2,
    const StrView exp)
    -> boost::BigDec {
    // Normalise the input strings by removing underscores.
    const auto int_part = NormaliseAnyString(s1);
    const auto dec_part = NormaliseAnyString(s2);
    const auto exp_part = NormaliseAnyString(exp);

    // Recombine into single string.
    const auto final = int_part
        + (not dec_part.empty() ? "." + dec_part : "")
        + (not exp_part.empty() ? "e" + exp_part : "");
    return boost::BigDec(std::move(final));
}

auto spp::utils::strings::NormaliseAnyString(
    const StrView s1)
    -> Str {
    // Strip all underscore characters from the input string.
    auto out = Str();
    out.reserve(s1.length());
    std::ranges::copy_if(s1, std::back_inserter(out), [](const char c) { return c != '_'; });
    return out;
}
