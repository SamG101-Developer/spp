module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>

module spp.utils.error_formatter;
import spp.asts.ast;
import spp.lex.tokens;
import colex;
import opex.cast;
import genex;
import std;


SPP_MOD_BEGIN
spp::utils::errors::ErrorFormatter::ErrorFormatter(Vec<lex::RawToken> tokens, Str file_path) :
    _Tokens(std::move(tokens)),
    _FilePath(std::move(file_path)) {
}


auto spp::utils::errors::ErrorFormatter::InternalParseErrorRawPos(
    const std::size_t ast_start_pos,
    std::size_t ast_size, Str &&tag_message)
    -> std::tuple<Str, Str, Str, Str, Str> {
    using lex::RawTokenType;
    using namespace std::literals;

    // Check for bugs in the ast size (max number? => 1)
    if (ast_size > 1000 or ast_size < 1) {
        ast_size = 1;
    }

    const auto error_line_start_pos = genex::position_last(
        _Tokens | genex::views::take(ast_start_pos) | genex::to<Vec>(),
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; }) + 1;

    const auto error_line_end_pos = genex::position(
        _Tokens | genex::views::drop(ast_start_pos) | genex::to<Vec>(),
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; }, {}, _Tokens.Len() - ast_start_pos) + ast_start_pos;

    auto error_line_tokens = Vec(
        _Tokens.begin() + error_line_start_pos,
        _Tokens.begin() + (error_line_end_pos as SSize));

    auto error_line_as_string = genex::fold_left(
        error_line_tokens, Str(),
        [](Str const &acc, const lex::RawToken &token) { return acc + token.data; });
    while (!error_line_as_string.empty() and error_line_as_string.back() == ' ') {
        error_line_as_string.pop_back();
    }

    // Get the line number of the error.
    auto error_line_number = std::to_string(genex::operations::size(_Tokens
        | genex::views::take(ast_start_pos)
        | genex::views::filter([](const lex::RawToken &token) { return token.type == RawTokenType::TK_LINE_FEED; })
        | genex::to<Vec>()));

    // The number of "^" is the length of the token data where the error is.
    const auto num_tokens_before_ast_pos = ast_start_pos - (error_line_start_pos as USize) + 1;
    auto carets = Str(ast_size, '^');
    carets.insert(0, Str(num_tokens_before_ast_pos, ' '));

    // Print the preceding spaces before the error line.
    const auto l1 = error_line_as_string.length();
    auto pos = 0uz;
    while ((pos = error_line_as_string.find("  "s, pos)) != Str::npos) {
        error_line_as_string.replace(pos, 2, "");
    }
    const auto temp = std::min(carets.length(), l1 - error_line_as_string.length());
    carets = carets.substr(temp) + (colex::fg_bright_white & colex::st_bold) + " <- " + tag_message;
    auto left_padding = Str(error_line_number.length(), ' ');

    // Remove "\n" from the start and end of the error line string.
    if (!error_line_as_string.empty() and error_line_as_string.front() == '\n') {
        error_line_as_string.erase(0, 1);
    }

    // Return the file path, line number, line as string, and carets.
    return std::make_tuple(_FilePath, error_line_number, error_line_as_string, left_padding, carets);
}


auto spp::utils::errors::ErrorFormatter::ErrorRawPos(
    const std::size_t ast_start_pos,
    const std::size_t ast_size,
    Str &&message,
    Str &&tag_message)
    -> Str {
    using namespace std::string_literals;

    auto [file_path, line_number, error_line, left_padding, carets] = InternalParseErrorRawPos(
        ast_start_pos, ast_size, std::move(tag_message));
    const auto line1 = (colex::fg_bright_white & colex::st_bold) + "Error in file '"s + file_path + "', on line "s + line_number + ":\n";
    const auto line2 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |\n"s;
    const auto line3 = (colex::fg_bright_red & colex::st_bold) + line_number + " | "s + error_line + "\n"s;
    const auto line4 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |"s;
    const auto line5 = (colex::reset & colex::fg_bright_red) + carets + "\n"s;
    const auto line6 = (colex::reset & colex::fg_bright_red) + message + "\n"s;
    return line1 + line2 + line3 + line4 + line5 + line6;
}


auto spp::utils::errors::ErrorFormatter::ErrorRawPosMinimal(
    const std::size_t ast_start_pos,
    const std::size_t ast_size,
    Str &&tag_message)
    -> Str {
    using namespace std::string_literals;

    auto [file_path, line_number, error_line, left_padding, carets] = InternalParseErrorRawPos(
        ast_start_pos, ast_size, std::move(tag_message));
    const auto line1 = (colex::fg_bright_white & colex::st_bold) + "Context from file '"s + file_path + "', on line "s + line_number + ":\n";
    const auto line2 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |\n"s;
    const auto line3 = (colex::fg_bright_green & colex::st_bold) + line_number + " | "s + error_line + "\n"s;
    const auto line4 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |"s;
    const auto line5 = (colex::reset & colex::fg_bright_green) + carets + "\n"s;
    return line1 + line2 + line3 + line4 + line5;
}


auto spp::utils::errors::ErrorFormatter::ErrorAst(
    asts::Ast const *ast,
    Str &&message,
    Str &&tag_message)
    -> Str {
    return ErrorRawPos(
        ast->PosStart(), ast->PosEnd() - ast->PosStart(), std::move(message), std::move(tag_message));
}


auto spp::utils::errors::ErrorFormatter::ErrorAstMinimal(
    asts::Ast const *ast,
    Str &&tag_message)
    -> Str {
    return ErrorRawPosMinimal(
        ast->PosStart(), ast->PosEnd() - ast->PosStart(), std::move(tag_message));
}

SPP_MOD_END
