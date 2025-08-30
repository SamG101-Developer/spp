#include <algorithm>
#include <ranges>

#include <colex/common.hpp>
#include <genex/algorithms/fold.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/operations/size.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/take.hpp>
#include <genex/views/to.hpp>

#include <spp/utils/error_formatter.hpp>

#include "spp/asts/ast.hpp"


spp::utils::errors::ErrorFormatter::ErrorFormatter(std::vector<lex::RawToken> tokens, const std::string_view file_path) :
    m_tokens(std::move(tokens)),
    m_file_path(file_path) {
}


auto spp::utils::errors::ErrorFormatter::internal_parse_error_raw_pos(
    const std::size_t ast_start_pos,
    const std::size_t ast_size, std::string &&tag_message)
    -> std::tuple<std::string, std::string, std::string, std::string, std::string> {
    using lex::RawTokenType;
    using namespace std::string_literals;

    auto slice_front = m_tokens
        | genex::views::take(ast_start_pos)
        | genex::views::to<std::vector>();

    auto slice_back = m_tokens
        | genex::views::drop(ast_start_pos)
        | genex::views::to<std::vector>();

    const auto error_line_start_pos = genex::algorithms::position_last(
        slice_front,
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; }) + 1;

    const auto error_line_end_pos = genex::algorithms::position(
        slice_back,
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; }, {}, m_tokens.size()) + ast_start_pos;

    auto error_line_tokens = std::vector(
        m_tokens.begin() + error_line_start_pos,
        m_tokens.begin() + error_line_end_pos);

    auto error_line_as_string = genex::algorithms::fold_left(
        error_line_tokens, std::string(),
        [](std::string const &acc, const lex::RawToken &token) { return acc + token.data; });

    // Get the line number of the error.
    auto error_line_number = std::to_string(genex::operations::size(m_tokens
        | genex::views::take(ast_start_pos)
        | genex::views::filter([](const lex::RawToken &token) { return token.type == RawTokenType::TK_LINE_FEED; })));

    // The number of "^" is the length of the token data where the error is.
    const auto num_tokens_before_ast_pos = ast_start_pos - error_line_start_pos + 1;
    auto carets = std::string(ast_size, '^');
    carets.insert(0, std::string(num_tokens_before_ast_pos, ' '));

    // Print the preceding spaces before the error line.
    const auto l1 = error_line_as_string.length();
    auto pos = 0uz;
    while ((pos = error_line_as_string.find("  "s, pos)) != std::string::npos) {
        error_line_as_string.replace(pos, 2, "");
    }
    carets = carets.substr(l1 - error_line_as_string.length()) + (colex::fg_white & colex::st_bold) + " <- " + tag_message;
    auto left_padding = std::string(error_line_number.length(), ' ');

    // Remove "\n" from the start and end of the error line string.
    if (!error_line_as_string.empty() and error_line_as_string.front() == '\n') {
        error_line_as_string.erase(0, 1);
    }

    // Return the file path, line number, line as string, and carets.
    return std::make_tuple(std::string(m_file_path), error_line_number, error_line_as_string, left_padding, carets);
}


auto spp::utils::errors::ErrorFormatter::error_raw_pos(
    const std::size_t ast_start_port,
    const std::size_t ast_size,
    std::string &&message,
    std::string &&tag_message)
    -> std::string {
    using namespace std::string_literals;

    auto [file_path, line_number, error_line, left_padding, carets] = internal_parse_error_raw_pos(ast_start_port, ast_size, std::move(tag_message));
    const auto line1 = (colex::fg_white & colex::st_bold) + "Error in file '"s + file_path + "', on line "s + line_number + ";\n";
    const auto line2 = (colex::fg_white & colex::st_bold) + left_padding + " |\n"s;
    const auto line3 = (colex::fg_red & colex::st_bold) + line_number + " | "s + error_line + "\n"s;
    const auto line4 = (colex::fg_white & colex::st_bold) + left_padding + " |"s;
    const auto line5 = (colex::reset & colex::fg_red) + carets + "\n"s;
    const auto line6 = (colex::reset & colex::fg_red) + message + "\n"s;
    return line1 + line2 + line3 + line4 + line5 + line6;
}


auto spp::utils::errors::ErrorFormatter::error(
    const asts::Ast *ast,
    std::string &&message,
    std::string &&tag_message)
    -> std::string {
    const auto ast_start_pos = ast->pos_start();
    const auto ast_size = ast->size();
    return error_raw_pos(ast_start_pos, ast_size, std::move(message), std::move(tag_message));
}
