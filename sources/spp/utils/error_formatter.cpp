module;
#include <spp/macros.hpp>

module spp.utils.error_formatter;
import spp.asts.ast;
import spp.lex.tokens;
import spp.utils.ice;
import colex;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::asts, Ast);

namespace spp::utils::errors {
  /// How far apart two files' positions start: more tokens than
  /// any file has, so ranges never overlap.
  static constexpr auto kFilePositionStride = 1uz << 24;

  /// How many UTF-16 code units a run of UTF-8 encodes to: one for
  /// everything inside the basic multilingual plane, and two for
  /// anything above it, which UTF-16 spells as a surrogate pair.
  /// An editor counts columns in these.
  static auto Utf16Length(const StrView text) -> std::size_t;

  /// The formatters that own positions, in the order they were
  /// made; each removes itself when it goes.
  static auto Registered() -> Vec<ErrorFormatter*>&;

  /// The base the next file's formatter is given. Never reused,
  /// so an ast of a freed file is owned by no later one.
  static auto NextBase() -> std::size_t&;
}

auto spp::utils::errors::Utf16Length(const StrView text) -> std::size_t {
  auto units = 0uz;
  for (auto const c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if ((byte & 0xC0u) == 0x80u) { continue; }
    units += byte >= 0xF0u ? 2uz : 1uz;
  }
  return units;
}

auto spp::utils::errors::Registered() -> Vec<ErrorFormatter*>& {
  static auto formatters = Vec<ErrorFormatter*>();
  return formatters;
}

auto spp::utils::errors::NextBase() -> std::size_t& {
  static auto next = kFilePositionStride;
  return next;
}

SPP_MOD_BEGIN
spp::utils::errors::ErrorFormatter::ErrorFormatter(
  Vec<lex::RawToken> tokens, Str file_path, const std::size_t prelude_token_index) :
  _Tokens(std::move(tokens)),
  _FilePath(std::move(file_path)),
  _PreludeTokenIndex(prelude_token_index) {
  // The lexer starts every file with a newline (token 0), so the
  // author's first line starts at token 1.
  for (auto i = 0uz; i < _Tokens.size(); ++i) {
    if (_Tokens[i].Data == "\n") { _LineStarts.emplace_back(i + 1); }
  }
}

spp::utils::errors::ErrorFormatter::~ErrorFormatter() {
  if (_Base != 0) { std::erase(Registered(), this); }
}

auto spp::utils::errors::ErrorFormatter::ForFile(
  Vec<lex::RawToken> tokens, Str file_path,
  const std::size_t prelude_token_index) -> Unique<ErrorFormatter> {
  // Take the next range, and register the formatter as owning
  // it. This provides unique indexes per formatter.
  SPP_ASSERT(tokens.size() < kFilePositionStride);
  auto formatter = MakeUnique<ErrorFormatter>(
    std::move(tokens), std::move(file_path), prelude_token_index);

  // Increment the base and assign the formatter into the
  // "registered" list.
  formatter->_Base = std::exchange(NextBase(), NextBase() + kFilePositionStride);
  Registered().emplace_back(formatter.get());
  return formatter;
}

auto spp::utils::errors::ErrorFormatter::Owning(
  const std::size_t position) -> ErrorFormatter* {
  // Extract a formatter from the "registered" list by the
  // position.
  for (const auto formatter : Registered()) {
    if (formatter->_TokenIndexOf(position).has_value()) { return formatter; }
  }
  return nullptr;
}

auto spp::utils::errors::ErrorFormatter::Owning(
  Ast const *ast) -> ErrorFormatter* {
  // Map into the error formatter "owning" by the ast's
  // start position.
  return ast != nullptr ? Owning(ast->PosStart()) : nullptr;
}

auto spp::utils::errors::ErrorFormatter::WithoutSource(
  Ast const *ast, Str const &tag_message) -> Str {
  using namespace std::string_literals;
  return (colex::fg_bright_white & colex::st_bold) + "In generated code: "s + ast->ToString() + " <- "s + tag_message
    + "\n"s;
}

auto spp::utils::errors::ErrorFormatter::PositionOf(
  const std::size_t token_index) const
  -> std::size_t {
  return _Base + token_index;
}

auto spp::utils::errors::ErrorFormatter::_TokenIndexOf(
  const std::size_t position) const
  -> Opt<std::size_t> {
  if (position < _Base or position - _Base >= _Tokens.size()) { return Nullopt; }
  return position - _Base;
}

auto spp::utils::errors::ErrorFormatter::_UserTokenIndexOf(
  const std::size_t position) const
  -> Opt<std::size_t> {
  const auto index = _TokenIndexOf(position);
  if (not index.has_value() or *index == 0 or *index >= _PreludeTokenIndex) { return Nullopt; }
  return index;
}

auto spp::utils::errors::ErrorFormatter::_LineOf(
  const std::size_t index) const
  -> std::size_t {
  // How many lines start at or before the token.
  return static_cast<std::size_t>(genex::upper_bound(_LineStarts, index) - _LineStarts.begin());
}

auto spp::utils::errors::ErrorFormatter::_LineBounds(
  const std::size_t line) const
  -> Pair<std::size_t, std::size_t> {
  // The next line's start is just past this line's newline.
  const auto first = _LineStarts[line - 1];
  const auto last = line < _LineStarts.size() ? _LineStarts[line] - 1 : _Tokens.size();
  return {first, last};
}

auto spp::utils::errors::ErrorFormatter::_ByteLength(
  const std::size_t first, const std::size_t last) const
  -> std::size_t {
  auto bytes = 0uz;
  for (auto i = first; i < last; ++i) { bytes += _Tokens[i].Data.length(); }
  return bytes;
}

auto spp::utils::errors::ErrorFormatter::LineColOf(
  const std::size_t position) const
  -> Opt<LineCol> {
  const auto index = _UserTokenIndexOf(position);
  if (not index.has_value()) { return Nullopt; }
  const auto line = _LineOf(*index);
  return LineCol{.Line = line, .Column = 1 + _ByteLength(_LineBounds(line).first, *index)};
}

auto spp::utils::errors::ErrorFormatter::IsPastUserSource(
  const std::size_t position) const -> bool {
  const auto index = _TokenIndexOf(position);
  return index.has_value() and *index >= _PreludeTokenIndex;
}

auto spp::utils::errors::ErrorFormatter::ErrorRawPos(
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&message,
  Str &&tag_message)
  -> Str {
  using namespace std::string_literals;
  return _Render(true, ast_start_pos, ast_size, std::move(tag_message))
    + (colex::reset & colex::fg_bright_red) + message + "\n"s;
}

auto spp::utils::errors::ErrorFormatter::ErrorRawPosMinimal(
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&tag_message)
  -> Str {
  return _Render(false, ast_start_pos, ast_size, std::move(tag_message));
}

auto spp::utils::errors::ErrorFormatter::_Render(
  const bool is_error,
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&tag_message) const
  -> Str {
  //
  using namespace std::string_literals;
  const auto quote = _QuoteRawPos(ast_start_pos, ast_size, std::move(tag_message));
  const auto heading = is_error ? "Error in file '"s : "Context from file '"s;
  const auto quoted = is_error ? (colex::fg_bright_red & colex::st_bold) : (colex::fg_bright_green & colex::st_bold);
  const auto carets = is_error ? (colex::reset & colex::fg_bright_red) : (colex::reset & colex::fg_bright_green);

  // Todo: Make the file path clickable in supporting terminals (IntelliJ).
  // file_path = "\033]8;;"s + file_path + "\033" + file_path + "\033]8;;\033";
  const auto line1 = (colex::fg_bright_white & colex::st_bold) + heading + _FilePath + "', "s +
    quote.Location + ":\n";
  const auto line2 = (colex::fg_bright_white & colex::st_bold) + quote.LeftPadding + " |\n"s;
  const auto line3 = quoted + quote.LineNumber + " | "s + quote.LineText + "\n"s;
  const auto line4 = (colex::fg_bright_white & colex::st_bold) + quote.LeftPadding + " |"s;
  const auto line5 = carets + quote.Carets + "\n"s;
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
  // Every line the author wrote, to the one after their last
  // newline: the prelude appended behind them is not part of the
  // file anyone is looking at. Lines are zero-based here.
  const auto user_end = std::min(_PreludeTokenIndex, _Tokens.size());
  const auto lines = static_cast<std::size_t>(
    genex::upper_bound(_LineStarts, user_end) - _LineStarts.begin());
  return SourceSpan{.File = _FilePath, .EndLine = lines > 0 ? lines - 1 : 0, .Generated = false};
}

auto spp::utils::errors::ErrorFormatter::_LocateRawPos(
  std::size_t ast_start_pos,
  std::size_t ast_size) const
  -> _RawLocation {
  // A span of over 1000 tokens (a whole function or class) is
  // quoted by its first token, so the report stays readable.
  ast_size = ast_size > 1000 ? 1 : ast_size;

  // Synthetic/generated ASTs have no real source position
  // (token 0 is the lexer's prepended newline sentinel), and
  // neither has anything past the end of what the author wrote,
  // which is the prelude appended behind it. Quoting either back
  // would point at a line the user didn't type. A position that
  // is not this file's (another file's ast, or code the compiler
  // parsed for itself) has none here either.
  const auto index = _UserTokenIndexOf(ast_start_pos);
  if (not index.has_value()) {
    auto nowhere = _RawLocation();
    nowhere.Generated = true;
    nowhere.PastUserSource = IsPastUserSource(ast_start_pos);
    return nowhere;
  }

  // The line the token is on, quoted as its tokens' text, with
  // the trailing spaces dropped. Its offset is the bytes before
  // the token on that line, and the underlined part the bytes of
  // the ast's tokens, clipped to the line and never empty.
  const auto line = _LineOf(*index);
  const auto [first, last] = _LineBounds(line);
  auto text = Str();
  for (auto i = first; i < last; ++i) { text += _Tokens[i].Data; }
  while (not text.empty() and text.back() == ' ') { text.pop_back(); }

  return {
    .Generated = false,
    .PastUserSource = false,
    .Line = line,
    .LineText = std::move(text),
    .ByteOffset = _ByteLength(first, *index),
    .ByteSpan = std::max(1uz, _ByteLength(*index, std::min(*index + ast_size, last)))
  };
}

auto spp::utils::errors::ErrorFormatter::_QuoteRawPos(
  const std::size_t ast_start_pos,
  const std::size_t ast_size,
  Str &&tag_message) const
  -> _Quote {
  using namespace std::literals;

  const auto loc = _LocateRawPos(ast_start_pos, ast_size);
  if (loc.Generated) {
    return {
      .Location = loc.PastUserSource ? "at the end of the file"_str : "in generated code"_str,
      .LineNumber = ""_str,
      .LineText = loc.PastUserSource ? "<end of file>"_str : "<generated code>"_str,
      .LeftPadding = ""_str,
      .Carets = " <- "s + (colex::fg_bright_white & colex::st_bold) + tag_message
    };
  }

  // The caret row is drawn under the quoted text, one column in
  // from the gutter ("N | source" against "  |carets").
  auto carets = Str(loc.ByteSpan, '^');
  carets.insert(0, Str(loc.ByteOffset + 1, ' '));
  carets += (colex::fg_bright_white & colex::st_bold) + " <- "s + tag_message;

  const auto line_number = std::to_string(loc.Line);
  return {
    .Location = "on line "s + line_number,
    .LineNumber = line_number,
    .LineText = loc.LineText,
    .LeftPadding = Str(line_number.length(), ' '),
    .Carets = std::move(carets)
  };
}

SPP_MOD_END
