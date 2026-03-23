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
    -> std::size_t {
    const auto m = s1.size();
    const auto n = s2.size();

    auto prev = std::vector<std::size_t>(n + 1);
    auto curr = std::vector<std::size_t>(n + 1);
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


auto spp::utils::strings::similarity_ratio(
    const std::string_view s1,
    const std::string_view s2)
    -> double {
    const auto max_len = std::max(s1.size(), s2.size());
    if (max_len == 0) { return 1.0; }

    const auto distance = levenshtein(s1, s2);
    return 1.0 - static_cast<double>(distance) / static_cast<double>(max_len);
}


auto spp::utils::strings::normalize_integer_string(
    const std::string_view s1)
    -> mppp::BigInt {
    auto out = std::string();
    auto base = 10;

    auto i = 0uz;
    if (s1.size() > 2 and s1[0] == '0') {
        if (s1[1] == 'b') {
            base = 2;
            i = 2;
        }
        else if (s1[1] == 'x') {
            base = 16;
            i = 2;
        }
        else if (s1[1] == 'o') {
            base = 8;
            i = 2;
        }
    }

    for (; i < s1.size(); ++i) {
        const auto c = s1[i];
        if (c == '_') { continue; }
        out.push_back(c);
    }

    return mppp::BigInt(out, base);
}


auto spp::utils::strings::normalize_float_string(
    const std::string_view s1,
    const std::string_view s2)
    -> mppp::BigDec {
    auto out1 = std::string();
    for (const auto c : s1) {
        if (c == '_') { continue; }
        out1.push_back(c);
    }

    auto out2 = std::string();
    for (const auto c : s2) {
        if (c == '_') { continue; }
        out2.push_back(c);
    }

    const auto numerator = out1 + out2;
    const auto denometer = std::string("1") + std::string(out2.size(), '0');
    return mppp::BigDec(numerator + "/" + denometer);
}


auto spp::utils::strings::expand_scientific_notation(
    const std::string_view s1)
    -> mppp::BigDec {
    // Handle empty case (should never throw).
    if (s1.empty()) { return mppp::BigDec("0"); }

    // Extract and strip the sign.
    auto s = std::string(s1);
    auto sign = std::string();
    if (s.front() == '-' or s.front() == '+') {
        sign = s.front();
        s.erase(0, 1);
    }

    // Find the exponent marker ('e').
    const auto e_pos = s.find_first_of('e');

    auto mantissa = std::string();
    auto exponent = 0uz;

    if (e_pos == std::string::npos) {
        // No exponent — treat as plain decimal.
        mantissa = s;
    }
    else {
        mantissa = s.substr(0, e_pos);
        exponent = std::stoull(s.substr(e_pos + 1));
    }

    // Split mantissa into integer and fractional parts.
    const auto dot_pos = mantissa.find('.');
    auto int_part = std::string();
    auto frac_part = std::string();
    if (dot_pos == std::string::npos) {
        int_part  = mantissa;
        frac_part = "";
    }
    else {
        int_part  = mantissa.substr(0, dot_pos);
        frac_part = mantissa.substr(dot_pos + 1);
    }

    auto digits = int_part + frac_part;
    auto dot_after = int_part.size();
    dot_after += exponent;
    auto result = std::string();

    if (dot_after <= 0) {
        // All digits are to the right of the decimal point; need leading zeros.
        result = "0.";
        result.append(-dot_after, '0');
        result += digits;
    }
    else if (dot_after >= digits.size()) {
        // Decimal point is beyond all digits; pad with trailing zeros.
        result = digits;
        result.append(dot_after - digits.size(), '0');
    }
    else {
        // Decimal point sits within the digit string.
        result  = digits.substr(0, dot_after);
        result += '.';
        result += digits.substr(dot_after);
    }

    // Strip redundant leading zeros (but keep at least one digit before the dot).
    {
        const auto first_nonzero = result.find_first_not_of('0');
        if (first_nonzero == std::string::npos) {
            result = "0";
        }
        else if (result[first_nonzero] == '.') {
            result = "0" + result.substr(first_nonzero);
        }
        else {
            result = result.substr(first_nonzero);
        }
    }

    // Strip redundant trailing zeros after a decimal point.
    if (result.find('.') != std::string::npos) {
        const auto last_nonzero = result.find_last_not_of('0');
        if (result[last_nonzero] == '.') {
            result = result.substr(0, last_nonzero);
        }
        else {
            result = result.substr(0, last_nonzero + 1);
        }
    }

    return mppp::BigDec(not result.empty() ? sign + result : "0");
}
