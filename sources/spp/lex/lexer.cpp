module spp.lex.lexer;
import spp.utils.strings;
import genex;
import magic_enum;


spp::lex::Lexer::Lexer(std::string code)
    : m_code("\n" + std::move(code)) {
}


auto spp::lex::Lexer::lex() const
    -> std::vector<RawToken> {
    // Define tracker variables.
    auto tokens = std::vector<RawToken>();
    auto in_char = false;
    auto in_string = false;
    auto in_single_line_comment = false;
    auto in_multi_line_comment = false;

    // Save keywords.
    auto keywords = std::map<RawTokenType, std::string>();
    for (auto [kw, kw_string] : magic_enum::enum_entries<RawTokenType>()) {
        if (kw_string.starts_with("KW_")) {
            keywords[kw] = kw_string.substr(3)
                | genex::views::transform([](auto c) { return static_cast<char>(std::tolower(c)); })
                | genex::to<std::string>();
        }
    }

    // Iterate the source code.
    auto i = 0uz;
    while (i < m_code.length()) {
        const auto c = m_code[i];

        // Skip any characters in a single-line comment (except terminating newline character).
        if (in_single_line_comment and c != '\n') {
            ++i;
            continue;
        }

        // Append any characters in a character literal as a character token (except terminating apostrophe).
        if (in_char and c != '\'') {
            tokens.emplace_back(RawTokenType::LX_CHARACTER, std::string(1, c));
            ++i;
            continue;
        }

        // Append any characters in a string literal as a character token (except terminating quotation mark).
        if (in_string and c != '"') {
            tokens.emplace_back(RawTokenType::LX_CHARACTER, std::string(1, c));
            ++i;
            continue;
        }

        // Enter a multi-line comment on '##' (not currently in a multi-line comment).
        if (not in_multi_line_comment and c == '#' and m_code[i + 1] == '#') {
            in_multi_line_comment = true;
            i += 2;
            continue;
        }

        // Exit a multi-line comment on '##' (currently in a multi-line comment).
        if (in_multi_line_comment and c == '#' and m_code[i + 1] == '#') {
            in_multi_line_comment = false;
            i += 2;
            continue;
        }

        // Skip any characters in a multi-line comment.
        if (in_multi_line_comment) {
            ++i;
            continue;
        }

        // Otherwise, match the character to known tokens and append them to the token list.
        switch (c) {
        case '#': {
            in_single_line_comment = true;
            ++i;
            continue;
        }
        case '=': {
            tokens.emplace_back(RawTokenType::TK_EQUALS_TO, "=");
            ++i;
            continue;
        }
        case '<': {
            tokens.emplace_back(RawTokenType::TK_LESS_THAN, "<");
            ++i;
            continue;
        }
        case '>': {
            tokens.emplace_back(RawTokenType::TK_GREATER_THAN, ">");
            ++i;
            continue;
        }
        case '+': {
            tokens.emplace_back(RawTokenType::TK_PLUS_SIGN, "+");
            ++i;
            continue;
        }
        case '-': {
            tokens.emplace_back(RawTokenType::TK_HYPHEN, "-");
            ++i;
            continue;
        }
        case '*': {
            tokens.emplace_back(RawTokenType::TK_ASTERISK, "*");
            ++i;
            continue;
        }
        case '/': {
            tokens.emplace_back(RawTokenType::TK_SLASH, "/");
            ++i;
            continue;
        }
        case '%': {
            tokens.emplace_back(RawTokenType::TK_PERCENT_SIGN, "%");
            ++i;
            continue;
        }
        case '(': {
            tokens.emplace_back(RawTokenType::TK_LEFT_PARENTHESIS, "(");
            ++i;
            continue;
        }
        case ')': {
            tokens.emplace_back(RawTokenType::TK_RIGHT_PARENTHESIS, ")");
            ++i;
            continue;
        }
        case '[': {
            tokens.emplace_back(RawTokenType::TK_LEFT_SQUARE_BRACKET, "[");
            ++i;
            continue;
        }
        case ']': {
            tokens.emplace_back(RawTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
            ++i;
            continue;
        }
        case '{': {
            tokens.emplace_back(RawTokenType::TK_LEFT_CURLY_BRACE, "{");
            ++i;
            continue;
        }
        case '}': {
            tokens.emplace_back(RawTokenType::TK_RIGHT_CURLY_BRACE, "}");
            ++i;
            continue;
        }
        case '?': {
            tokens.emplace_back(RawTokenType::TK_QUESTION_MARK, "?");
            ++i;
            continue;
        }
        case ':': {
            tokens.emplace_back(RawTokenType::TK_COLON, ":");
            ++i;
            continue;
        }
        case '&': {
            tokens.emplace_back(RawTokenType::TK_AMPERSAND, "&");
            ++i;
            continue;
        }
        case '|': {
            tokens.emplace_back(RawTokenType::TK_VERTICAL_BAR, "|");
            ++i;
            continue;
        }
        case '^': {
            tokens.emplace_back(RawTokenType::TK_CARET, "^");
            ++i;
            continue;
        }
        case '.': {
            tokens.emplace_back(RawTokenType::TK_PERIOD, ".");
            ++i;
            continue;
        }
        case ',': {
            tokens.emplace_back(RawTokenType::TK_COMMA, ",");
            ++i;
            continue;
        }
        case '@': {
            tokens.emplace_back(RawTokenType::TK_AT_SIGN, "@");
            ++i;
            continue;
        }
        case '_': {
            tokens.emplace_back(RawTokenType::TK_UNDERSCORE, "_");
            ++i;
            continue;
        }
        case '\'': {
            tokens.emplace_back(RawTokenType::TK_APOSTROPHE, "'");
            in_char = !in_char;
            ++i;
            continue;
        }
        case '"': {
            tokens.emplace_back(RawTokenType::TK_QUOTATION_MARK, "\"");
            in_string = !in_string;
            ++i;
            continue;
        }
        case '!': {
            tokens.emplace_back(RawTokenType::TK_EXCLAMATION_MARK, "!");
            ++i;
            continue;
        }
        case ';': {
            tokens.emplace_back(RawTokenType::TK_SEMICOLON, ";");
            ++i;
            continue;
        }
        case '$': {
            tokens.emplace_back(RawTokenType::TK_DOLLAR_SIGN, "$");
            ++i;
            continue;
        }
        case ' ': {
            tokens.emplace_back(RawTokenType::TK_SPACE, " ");
            ++i;
            continue;
        }
        case '\n': {
            tokens.emplace_back(RawTokenType::TK_LINE_FEED, "\n");
            in_single_line_comment = false;
            ++i;
            continue;
        }
        case '\r': {
            ++i;
            continue;
        }
        default:
            ;
        }

        // No symbolic tokens match, so try to match a keyword.
        auto found_kw = false;
        for (auto [kw_enum, kw_string] : keywords) {
            const auto is_prev_alnum = i > 0 and utils::strings::is_alphanumeric(m_code[i - 1]);
            const auto is_next_alnum = i + kw_string.length() < m_code.length() and utils::strings::is_alphanumeric(m_code[i + kw_string.length()]);

            if (m_code.substr(i).starts_with(kw_string) and not is_prev_alnum and not is_next_alnum) {
                tokens.emplace_back(kw_enum, kw_string);
                i += kw_string.length();
                found_kw = true;
                break;
            }
        }
        if (found_kw) {
            continue;
        }

        // Try to match individual characters or numbers, otherwise the token is unknown.
        if (('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z')) {
            tokens.emplace_back(RawTokenType::LX_CHARACTER, std::string(1, c));
            ++i;
            continue;
        }
        else if ('0' <= c and c <= '9') {
            tokens.emplace_back(RawTokenType::LX_DIGIT, std::string(1, c));
            ++i;
            continue;
        }
        else {
            tokens.emplace_back(RawTokenType::SP_UNKNOWN, std::string(1, c));
            ++i;
            continue;
        }
    }

    // Add a final EOF token.
    tokens.emplace_back(RawTokenType::SP_EOF, "");
    return tokens;
}
