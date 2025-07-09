#include <algorithm>
#include <ranges>

#include <spp/utils/error_formatter.hpp>


spp::utils::errors::ErrorFormatter::ErrorFormatter(std::vector<lex::RawToken> tokens, const std::string_view file_path):
    m_tokens(std::move(tokens)),
    m_file_path(file_path) {
}


auto spp::utils::errors::ErrorFormatter::error_from_positions(
    std::size_t start_pos,
    std::string &&message,
    std::string &&tag,
    bool minimal) const -> std::string {

    using namespace std::string_literals;


    while (m_tokens[start_pos].type == lex::RawTokenType::TK_SPACE || m_tokens[start_pos].type == lex::RawTokenType::TK_LINE_FEED) {
        ++start_pos;
    }

    // Get the tokens at the start and end of the line containing the error. Skip the leading newline.
    const auto error_line_start_pos = std::ranges::distance(m_tokens.begin(), std::ranges::find_last_if(m_tokens | std::views::take(start_pos), [](const lex::RawToken &token) { return token.type == lex::RawTokenType::TK_LINE_FEED; }).begin());
    const auto error_line_end_pos = std::ranges::distance(m_tokens.begin(), std::ranges::find_if(m_tokens | std::views::drop(start_pos), [](const lex::RawToken &token) { return token.type == lex::RawTokenType::TK_LINE_FEED; })) + start_pos;
    const auto error_line_tokens = std::vector(m_tokens.begin() + error_line_start_pos, m_tokens.begin() + error_line_end_pos);
    auto error_line_as_string = std::ranges::fold_left(error_line_tokens, icu::UnicodeString(), [](icu::UnicodeString const &acc, const lex::RawToken &token) { return acc + token.data; });

    // Get the line number of the error
    const auto error_line_number = std::to_string(std::ranges::count(m_tokens | std::views::take(start_pos) | std::views::transform([](const lex::RawToken &token) { return token.type == lex::RawTokenType::TK_LINE_FEED; }), true));

    // The number of "^" is the length of the token data where the error is.
    auto caret_padding_count = std::ranges::fold_left(m_tokens | std::views::take(start_pos) | std::views::drop(error_line_start_pos), 0, [](const int acc, const lex::RawToken &token) { return acc + token.data.length(); });
    auto carets = std::string(" "s, caret_padding_count) + std::string("^"s, m_tokens[start_pos].data.length());

    // Print the preceding spaces before the error line
    auto l1 = error_line_as_string.length();
    error_line_as_string = error_line_as_string.findAndReplace("  ", "");
    carets = carets.substr(l1 - error_line_as_string.length()) + " <- "s + "\033[1;37m" + tag;

    auto error_line_as_string_utf8 = std::string();
    error_line_as_string.toUTF8String(error_line_as_string_utf8);

    const auto left_padding = std::string(" "s, error_line_number.length());
    const auto bar_character = "|";
    const auto final_error_message = "\n"s +
        "\033[1;37m" +
        (not minimal ? "Error in file '"s + m_file_path + "', on line " + error_line_number + ":\n"s : "Context from file '"s + m_file_path + "', on line "s + error_line_number + ":") +
        left_padding + " " + bar_character + "\n"s +
        (not minimal ? "\033[1;31m" : "\033[1;32m") + error_line_number + " " + bar_character + " " + error_line_as_string_utf8 + "\n"s +
        left_padding + " " + bar_character + " \033[0m" + (not minimal ? "\033[1;31m" : "\033[1;32m") + carets + "\n"s +
        (not minimal ? "\033[0m\033[1;31m" + message : "");

    return final_error_message;
}
