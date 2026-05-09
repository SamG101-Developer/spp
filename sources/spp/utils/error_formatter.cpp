module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>

module spp.utils.error_formatter;
import spp.asts.ast;
import spp.lex.tokens;
import colex;
import genex;
import opex.cast;
import std;

SPP_MOD_BEGIN
spp::utils::errors::ErrorFormatter::ErrorFormatter(Vec<lex::RawToken> tokens, Str file_path) :
    _Tokens(std::move(tokens)),
    _FilePath(std::move(file_path)) {
}

auto spp::utils::errors::ErrorFormatter::InternalParseErrorRawPos(
    const std::size_t ast_start_pos,
    const std::size_t ast_size,
    Str &&tag_message)
    -> std::tuple<Str, Str, Str, Str, Str> {
    using lex::RawTokenType;
    using namespace std::literals;

    // Synthetic/generated ASTs have no real source position (pos == 0 is the lexer's
    // prepended newline sentinel). Show a placeholder rather than pointing at the wrong line.
    if (ast_start_pos == 0) {
        return std::make_tuple(
            _FilePath, "?"s, "<generated code>"s, ""s,
            " <- "s + (colex::fg_bright_white & colex::st_bold) + tag_message);
    }

    // Find the start of the error line: the token immediately after the last newline before this one.
    // genex::position_last returns std::size_t(-1) when nothing is found; +1 then wraps to 0, which
    // is the correct fallback (start of file) because the lexer always prepends "\n" at token 0.
    const auto error_line_start_pos = genex::position_last(
        _Tokens | genex::views::take(ast_start_pos) | genex::to<Vec>(),
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; },
        {}, 0uz) + 1uz;

    // Find the end of the error line: the first newline after the error token (or EOF).
    const auto error_line_end_pos = genex::position(
        _Tokens | genex::views::drop(ast_start_pos) | genex::to<Vec>(),
        [](auto &&token) { return token.type == RawTokenType::TK_LINE_FEED; },
        {}, _Tokens.Len() - ast_start_pos) + ast_start_pos;

    // Build the source line string by concatenating raw token data.
    auto error_line_tokens = Vec(
        _Tokens.begin() + (error_line_start_pos as SSize),
        _Tokens.begin() + (error_line_end_pos as SSize));
    auto error_line_as_string = genex::fold_left(
        error_line_tokens, Str(),
        [](Str const &acc, const lex::RawToken &token) { return acc + token.data; });
    while (!error_line_as_string.empty() and error_line_as_string.back() == ' ') {
        error_line_as_string.pop_back();
    }
    if (!error_line_as_string.empty() and error_line_as_string.front() == '\n') {
        error_line_as_string.erase(0, 1);
    }

    // Count line feeds before this token to get the 1-based line number.
    auto error_line_number = std::to_string(genex::operations::size(_Tokens
        | genex::views::take(ast_start_pos)
        | genex::views::filter([](const lex::RawToken &token) { return token.type == RawTokenType::TK_LINE_FEED; })
        | genex::to<Vec>()));

    // Compute the character offset within the line by summing the data lengths of all raw tokens
    // between the line start and the error token. This correctly handles keywords (one multi-char
    // raw token) and multi-character identifiers (one single-char raw token per letter).
    auto char_offset = 0uz;
    for (auto i = error_line_start_pos; i < ast_start_pos && i < _Tokens.Len(); ++i) {
        char_offset += _Tokens[i].data.length();
    }

    // Compute the character span to underline. Iterate the raw tokens covered by this AST
    // (from ast_start_pos to ast_start_pos + ast_size, i.e. PosEnd()) and sum their data lengths.
    // Clip to the end of the current line to avoid spanning newlines.
    auto char_span = 0uz;
    const auto span_end = std::min(ast_start_pos + ast_size, error_line_end_pos);
    for (auto i = ast_start_pos; i < span_end && i < _Tokens.Len(); ++i) {
        char_span += _Tokens[i].data.length();
    }
    if (char_span < 1) char_span = 1;

    // Build the caret line. +1 on the offset aligns with the display gutter ("N | source")
    // where the caret row is "  |carets" — the source character column is offset by one
    // relative to the caret row's starting position.
    auto carets = Str(char_span, '^');
    carets.insert(0, Str(char_offset + 1, ' '));
    carets += (colex::fg_bright_white & colex::st_bold) + " <- "s + tag_message;
    auto left_padding = Str(error_line_number.length(), ' ');

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

    // file_path = "\033]8;;"s + file_path + "\033" + file_path + "\033]8;;\033"; // Make the file path clickable in supporting terminals.

    const auto line1 = (colex::fg_bright_white & colex::st_bold) + "Error in file '"s + file_path + " ', on line "s + line_number + ":\n";
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
