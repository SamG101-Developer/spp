module;
#include <spp/macros.hpp>

module spp.utils.error_formatter;
import spp.asts.ast;
import spp.lex.tokens;
import colex;
import genex;
import std;
import sys;

SPP_MOD_BEGIN
namespace spp::utils::errors {
  namespace {
    /// How many UTF-16 code units a run of UTF-8 encodes to: one for
    /// everything inside the basic multilingual plane, and two for
    /// anything above it, which UTF-16 spells as a surrogate pair.
    /// An editor counts columns in these.
    auto Utf16Length(const StrView text) -> std::size_t {
      auto units = 0uz;
      for (auto const c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if ((byte & 0xC0u) == 0x80u) { continue; }
        units += byte >= 0xF0u ? 2uz : 1uz;
      }
      return units;
    }
  }
}

spp::utils::errors::ErrorFormatter::ErrorFormatter(
  Vec<lex::RawToken> tokens, Str file_path, const std::size_t prelude_token_index) :
  _Tokens(std::move(tokens)),
  _FilePath(std::move(file_path)),
  _PreludeTokenIndex(prelude_token_index) {
}

auto spp::utils::errors::ErrorFormatter::IsPastUserSource(
  const std::size_t token_pos) const
  -> bool {
  return token_pos >= _PreludeTokenIndex;
}

auto spp::utils::errors::ErrorFormatter::ErrorRawPos(
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&message,
  Str &&tag_message)
  -> Str {
  //
  using namespace std::string_literals;

  auto [file_path, location, line_number, error_line, left_padding, carets] = _InternalParseErrorRawPos(
    ast_start_pos, ast_size, std::move(tag_message));

  // Todo: Make the file path clickable in supporting terminals (IntelliJ).
  // file_path = "\033]8;;"s + file_path + "\033" + file_path + "\033]8;;\033";

  const auto line1 = (colex::fg_bright_white & colex::st_bold) + "Error in file '"s + file_path + "', "s +
    location + ":\n";
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
  //
  using namespace std::string_literals;

  auto [file_path, location, line_number, error_line, left_padding, carets] = _InternalParseErrorRawPos(
    ast_start_pos, ast_size, std::move(tag_message));
  const auto line1 = (colex::fg_bright_white & colex::st_bold) + "Context from file '"s + file_path + "', "s +
    location + ":\n";
  const auto line2 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |\n"s;
  const auto line3 = (colex::fg_bright_green & colex::st_bold) + line_number + " | "s + error_line + "\n"s;
  const auto line4 = (colex::fg_bright_white & colex::st_bold) + left_padding + " |"s;
  const auto line5 = (colex::reset & colex::fg_bright_green) + carets + "\n"s;
  return line1 + line2 + line3 + line4 + line5;
}

auto spp::utils::errors::ErrorFormatter::ErrorAst(
  Ast const *ast, Str &&message, Str &&tag_message) -> Str {
  return ErrorRawPos(
    ast->PosStart(), ast->PosEnd() - ast->PosStart(),
    std::move(message), std::move(tag_message));
}

auto spp::utils::errors::ErrorFormatter::ErrorAstMinimal(
  Ast const *ast, Str &&tag_message) -> Str {
  return ErrorRawPosMinimal(
    ast->PosStart(), ast->PosEnd() - ast->PosStart(),
    std::move(tag_message));
}

auto spp::utils::errors::ErrorFormatter::FilePath() const
  -> Str const& {
  // Getter for the filepath.
  return _FilePath;
}

auto spp::utils::errors::ErrorFormatter::SpanOfRawPos(
  const std::size_t ast_start_pos,
  const std::size_t ast_size) const
  -> SourceSpan {
  // A position with no place in the author's source has no line
  // or column to report, only the file it was reached from.
  const auto loc = _LocateRawPos(ast_start_pos, ast_size);
  if (loc.Generated) { return SourceSpan{.File = _FilePath}; }

  // The quote counts bytes; an editor counts UTF-16 code units,
  // so the prefix and the underlined part are each measured
  // again in those.
  const auto offset = std::min(loc.ByteOffset, loc.LineText.length());
  const auto span = std::min(loc.ByteSpan, loc.LineText.length() - offset);
  const auto line = loc.Line > 0 ? loc.Line - 1 : 0;
  const auto start_col = Utf16Length(StrView(loc.LineText).substr(0, offset));
  return SourceSpan{
    .File = _FilePath,
    .StartLine = line,
    .StartCol = start_col,
    .EndLine = line,
    .EndCol = start_col + Utf16Length(StrView(loc.LineText).substr(offset, span)),
    .Generated = false
  };
}

auto spp::utils::errors::ErrorFormatter::SpanOfAst(
  asts::Ast const *const ast) const
  -> SourceSpan {
  if (ast == nullptr) { return SourceSpan{.File = _FilePath}; }
  // Forwards into the raw pos overload using the start and ast
  // size.
  return SpanOfRawPos(ast->PosStart(), ast->Size());
}

auto spp::utils::errors::ErrorFormatter::SpanAcross(
  const std::size_t start_pos,
  const std::size_t end_pos) const
  -> SourceSpan {
  // Span from the start position 1 token forward. If this is
  // compiler generated code (pos 0 effectively), then return
  // an empty span.
  const auto start = SpanOfRawPos(start_pos, 1);
  if (start.Generated) { return SourceSpan{.File = _FilePath}; }

  // Otherwise, do the same for the end of the ast. This is
  // because it might be on a separate line to the start of
  // the span request.
  const auto end = SpanOfRawPos(end_pos, 1);
  if (end.Generated) { return start; }

  // Otherwise, span between the two positions queried from
  // the span calls.
  return SourceSpan{
    .File = _FilePath,
    .StartLine = start.StartLine,
    .StartCol = start.StartCol,
    .EndLine = end.StartLine,
    .EndCol = end.StartCol,
    .Generated = false
  };
}

auto spp::utils::errors::ErrorFormatter::SpanOfWholeFile() const
  -> SourceSpan {
  using lex::RawTokenType;
  // Every line the author wrote: the prelude appended behind
  // them is not part of the file anyone is looking at.
  auto lines = 0uz;
  for (auto i = 0uz; i < _Tokens.Len() and not IsPastUserSource(i); ++i) {
    if (_Tokens[i].Type == RawTokenType::TK_LINE_FEED) { ++lines; }
  }
  return SourceSpan{.File = _FilePath, .EndLine = lines, .Generated = false};
}

auto spp::utils::errors::ErrorFormatter::_LocateRawPos(
  std::size_t ast_start_pos,
  std::size_t ast_size) const
  -> _RawLocation {
  using lex::RawTokenType;

  // Failsafe on ast sizes, which needs looking into - should
  // never actually hit this. When we do fuzzing, convert this
  // into an error and check it never throws.
  ast_size = ast_size > 1000 ? 1 : ast_size;
  ast_start_pos = ast_start_pos > _Tokens.Len() ? _Tokens.Len() - 1 : ast_start_pos;

  // Synthetic/generated ASTs have no real source position
  // (pos == 0 is the lexer's prepended newline sentinel),
  // and neither has anything past the end of what the author
  // wrote, which is the prelude appended behind it. Quoting
  // either back would point at a line the user didn't type.
  if (ast_start_pos == 0 or IsPastUserSource(ast_start_pos)) {
    auto nowhere = _RawLocation();
    nowhere.Generated = true;
    nowhere.PastUserSource = ast_start_pos != 0;
    return nowhere;
  }

  // Find the start of the error line: the token immediately
  // after the last newline before this one.
  auto error_line_start_pos = 1uz;
  for (auto i = ast_start_pos; i-- > 0;) {
    if (_Tokens[i].Type == RawTokenType::TK_LINE_FEED) {
      error_line_start_pos = i + 1uz;
      break;
    }
  }

  // Find the end too: the first newline after it, or the end
  // of the file
  auto error_line_end_pos = _Tokens.Len();
  for (auto i = ast_start_pos; i < _Tokens.Len(); ++i) {
    if (_Tokens[i].Type == RawTokenType::TK_LINE_FEED) {
      error_line_end_pos = i;
      break;
    }
  }

  // Build the source line string by concatenating raw token
  // data.
  auto error_line_tokens = Vec(
    _Tokens.begin() + static_cast<sys::ssize_t>(error_line_start_pos),
    _Tokens.begin() + static_cast<sys::ssize_t>(error_line_end_pos));
  auto error_line_as_string = genex::fold_left(
    error_line_tokens, Str(),
    [](Str const &acc, const lex::RawToken &token) { return acc + token.Data; });
  while (!error_line_as_string.empty() and error_line_as_string.back() == ' ') {
    error_line_as_string.pop_back();
  }

  // Count line feeds before this token to get the 1-based line number.
  auto error_line_number = 0uz;
  for (auto i = 0uz; i < ast_start_pos; ++i) {
    if (_Tokens[i].Type == RawTokenType::TK_LINE_FEED) { ++error_line_number; }
  }

  // Compute the character offset within the line by summing the data lengths of all raw tokens
  // between the line start and the error token. This correctly handles keywords (one multi-char
  // raw token) and multi-character identifiers (one single-char raw token per letter).
  auto char_offset = 0uz;
  for (auto i = error_line_start_pos; i < ast_start_pos && i < _Tokens.Len(); ++i) {
    char_offset += _Tokens[i].Data.length();
  }

  // Compute the character span to underline. Iterate the raw tokens covered by this AST
  // (from ast_start_pos to ast_start_pos + ast_size, i.e. PosEnd()) and sum their data lengths.
  // Clip to the end of the current line to avoid spanning newlines.
  auto char_span = 0uz;
  const auto span_end = std::min(ast_start_pos + ast_size, error_line_end_pos);
  for (auto i = ast_start_pos; i < span_end && i < _Tokens.Len(); ++i) {
    char_span += _Tokens[i].Data.length();
  }
  if (char_span < 1) char_span = 1;

  // The quoted line drops a leading line feed; an offset into
  // that text has to drop it too, while the caret row is drawn
  // from the offset as counted.
  auto byte_offset = char_offset;
  if (!error_line_as_string.empty() and error_line_as_string.front() == '\n') {
    error_line_as_string.erase(0, 1);
    if (byte_offset > 0) { --byte_offset; }
  }

  return {
    .Generated = false,
    .PastUserSource = false,
    .Line = error_line_number,
    .LineText = std::move(error_line_as_string),
    .ByteOffset = byte_offset,
    .RenderOffset = char_offset,
    .ByteSpan = char_span
  };
}

auto spp::utils::errors::ErrorFormatter::_InternalParseErrorRawPos(
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&tag_message)
  -> Tup<Str, Str, Str, Str, Str, Str> {
  using namespace std::literals;

  const auto loc = _LocateRawPos(ast_start_pos, ast_size);
  if (loc.Generated) {
    return {
      _FilePath,
      loc.PastUserSource ? "at the end of the file"_str : "in generated code"_str,
      ""_str,
      loc.PastUserSource ? "<end of file>"_str : "<generated code>"_str,
      ""_str,
      " <- "s + (colex::fg_bright_white & colex::st_bold) + tag_message
    };
  }

  // Build the caret line. +1 on the offset aligns with the display gutter ("N | source")
  // where the caret row is "  |carets" - the source character column is offset by one
  // relative to the caret row's starting position.
  auto carets = Str(loc.ByteSpan, '^');
  carets.insert(0, Str(loc.RenderOffset + 1, ' '));
  carets += (colex::fg_bright_white & colex::st_bold) + " <- "s + tag_message;

  const auto error_line_number = std::to_string(loc.Line);
  const auto left_padding = Str(error_line_number.length(), ' ');
  return {
    _FilePath, "on line "s + error_line_number, error_line_number, loc.LineText, left_padding, carets
  };
}

SPP_MOD_END
