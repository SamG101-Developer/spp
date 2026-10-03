module;
#include <spp/macros.hpp>

export module spp.utils.error_formatter;
import spp.lex.tokens;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);
use(spp::utils::errors, class ErrorFormatter);
use(spp::utils::errors, struct SourceSpan);

/// Where a piece of source sits, as an editor counts it: a
/// zero-based line, and zero-based columns measured in UTF-16
/// code units, which is what the language server protocol
/// measures a position in.
SPP_EXP_CLS struct spp::utils::errors::SourceSpan {
  Str File;
  std::size_t StartLine = 0;
  std::size_t StartCol = 0;
  std::size_t EndLine = 0;
  std::size_t EndCol = 0;
  bool Generated = true; /// Compiler generated (not in source).
};

/// The error formatter is the single place where all errors
/// are processed and displayed. They apply the colours, any
/// formatting, "^^^" strings, etc.
SPP_EXP_CLS class spp::utils::errors::ErrorFormatter {
public:
  ErrorFormatter(Vec<lex::RawToken> tokens, Str file_path, std::size_t prelude_token_index = Str::npos);

  /// Differentiate for when we are handling an error that
  /// might leak into the prelude code attached per module.
  SPP_ATTR_NODISCARD auto IsPastUserSource(std::size_t token_pos) const -> bool;

  /// Given an ast position and size, throw the error with
  /// the string built from the error builder.
  SPP_ATTR_COLD auto ErrorRawPos(std::size_t ast_start_pos, std::size_t ast_size, Str &&message, Str &&tag_message) -> Str;

  /// Given an ast position and size, throw the error with
  /// the minimally formatted version of the error builder
  /// output.
  SPP_ATTR_COLD auto ErrorRawPosMinimal(std::size_t ast_start_pos, std::size_t ast_size, Str &&tag_message) -> Str;

  /// Given an entire ast, use the ast properties to
  /// determine what to highlight, and throw the error.
  SPP_ATTR_COLD auto ErrorAst(Ast const *ast, Str &&message, Str &&tag_message) -> Str;

  /// Minimal version of the ast scan based highlighting,
  /// typically for ast "context", not the "error" site.
  SPP_ATTR_COLD auto ErrorAstMinimal(Ast const *ast, Str &&tag_message) -> Str;

  /// The span an ast covers, for a consumer that wants the
  /// position itself rather than a quote of the line: the json
  /// diagnostics, and the language server behind them. Same
  /// numbers the carets are drawn from, in the units an editor
  /// counts in.
  SPP_ATTR_NODISCARD auto SpanOfRawPos(
    std::size_t ast_start_pos, std::size_t ast_size) const -> SourceSpan;

  /// The file this formatter reports positions in. Answers whether a position is worth locating at all, without
  /// locating it.
  SPP_ATTR_NODISCARD auto FilePath() const -> Str const&;

  /// "SpanOfRawPos" over an ast's own start and end.
  SPP_ATTR_NODISCARD auto SpanOfAst(Ast const *ast) const -> SourceSpan;

  /// The whole of what lies between two positions, across as many lines as it takes. The spans above stop at the end
  /// of the line they start on, because a quoted error shows one line; a region an editor asks "is the caret in this"
  /// of cannot.
  SPP_ATTR_NODISCARD auto SpanAcross(
    std::size_t start_pos, std::size_t end_pos) const -> SourceSpan;

  /// The whole file, for a scope that covers it rather than sitting at a position in it.
  SPP_ATTR_NODISCARD auto SpanOfWholeFile() const -> SourceSpan;

private:
  /// Everything both the quoted line and the span are built
  /// from: which line the position falls on, that line's text,
  /// and where within it the underlined part starts and how far
  /// it runs. "RenderOffset" is the offset the caret row is
  /// drawn from, which counts the line feed token the quoted
  /// text drops; "ByteOffset" indexes "LineText" itself.
  struct _RawLocation {
    bool Generated = false;
    bool PastUserSource = false;
    std::size_t Line = 0;
    Str LineText;
    std::size_t ByteOffset = 0;
    std::size_t RenderOffset = 0;
    std::size_t ByteSpan = 0;
  };

  /// Locate a raw position: the shared half of the quote and
  /// the span, with no formatting of any kind.
  SPP_ATTR_NODISCARD auto _LocateRawPos(std::size_t ast_start_pos, std::size_t ast_size) const -> _RawLocation;

  /// Token list to index into.
  Vec<lex::RawToken> _Tokens;

  /// The file path of the source file being parsed.
  Str _FilePath;

  /// The index of the first token in the prelude.
  std::size_t _PreludeTokenIndex;

  /// The core error formatter. Handles almost all formatting
  /// between the different entry api functions.
  auto _InternalParseErrorRawPos(
    std::size_t ast_start_pos, std::size_t ast_size, Str &&tag_message) -> Tup<Str, Str, Str, Str, Str, Str>;
};
