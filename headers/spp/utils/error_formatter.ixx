module;
#include <spp/macros.hpp>

export module spp.utils.error_formatter;
import spp.lex.tokens;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);
use(spp::utils::errors, class ErrorFormatter);
use(spp::utils::errors, struct SourceSpan);
use(spp::utils::errors, struct LineCol);

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

/// Where a position sits in its file, as debug info and a
/// compiler's own messages count it: a one-based line, and a
/// one-based column in bytes.
SPP_EXP_CLS struct spp::utils::errors::LineCol {
  std::size_t Line = 0;
  std::size_t Column = 0;
};

/// The error formatter is the single place where all errors
/// are processed and displayed. They apply the colours, any
/// formatting, "^^^" strings, etc.
SPP_EXP_CLS class spp::utils::errors::ErrorFormatter {
public:
  /// A formatter that owns no positions: for code the compiler
  /// parses for itself (and the parser's own when given none),
  /// whose asts no file holds, and so read as generated.
  ErrorFormatter(
    Vec<lex::RawToken> tokens, Str file_path,
    std::size_t prelude_token_index = Str::npos);

  ~ErrorFormatter();
  ErrorFormatter(ErrorFormatter const &) = delete;
  ErrorFormatter(ErrorFormatter &&) = delete;
  auto operator=(ErrorFormatter const &) -> ErrorFormatter& = delete;
  auto operator=(ErrorFormatter &&) -> ErrorFormatter& = delete;

  /// The formatter of a source file. A file's positions are its
  /// own ("PositionOf"): each file is given a range no other file
  /// has, so any position names the file it came from ("Owning").
  SPP_ATTR_NODISCARD static auto ForFile(
    Vec<lex::RawToken> tokens, Str file_path,
    std::size_t prelude_token_index) -> Unique<ErrorFormatter>;

  /// The formatter of the file "position" lies in, or none for a
  /// position no file holds (generated code, code the compiler
  /// parsed for itself).
  SPP_ATTR_NODISCARD static auto Owning(
    std::size_t position) -> ErrorFormatter*;

  /// "Owning" the ast's start position.
  SPP_ATTR_NODISCARD static auto Owning(
    Ast const *ast) -> ErrorFormatter*;

  /// An ast no file holds, quoted with no formatter: what it says,
  /// written out, as there is no line to show.
  SPP_ATTR_NODISCARD static auto WithoutSource(
    Ast const *ast, Str const &tag_message) -> Str;

  /// The position an ast at "token_index" of this file has: what
  /// the parser stamps.
  SPP_ATTR_NODISCARD auto PositionOf(
    std::size_t token_index) const -> std::size_t;

  /// Differentiate for when we are handling an error that
  /// might leak into the prelude code attached per module: a
  /// position of this file in the prelude behind the author's
  /// code.
  SPP_ATTR_NODISCARD auto IsPastUserSource(
    std::size_t position) const -> bool;

  /// Given an ast position and size, throw the error with
  /// the string built from the error builder.
  SPP_ATTR_COLD auto ErrorRawPos(
    std::size_t ast_start_pos, std::size_t ast_size,
    Str &&message, Str &&tag_message) -> Str;

  /// Given an ast position and size, throw the error with
  /// the minimally formatted version of the error builder
  /// output.
  SPP_ATTR_COLD auto ErrorRawPosMinimal(
    std::size_t ast_start_pos, std::size_t ast_size,
    Str &&tag_message) -> Str;

  /// Given an entire ast, use the ast properties to
  /// determine what to highlight, and throw the error.
  SPP_ATTR_COLD auto ErrorAst(
    Ast const *ast, Str &&message, Str &&tag_message) -> Str;

  /// Minimal version of the ast scan based highlighting,
  /// typically for ast "context", not the "error" site.
  SPP_ATTR_COLD auto ErrorAstMinimal(
    Ast const *ast, Str &&tag_message) -> Str;

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
  SPP_ATTR_NODISCARD auto SpanOfAst(
    Ast const *ast) const -> SourceSpan;

  /// The whole of what lies between two positions, across as many lines as it takes. The spans above stop at the end
  /// of the line they start on, because a quoted error shows one line; a region an editor asks "is the caret in this"
  /// of cannot.
  SPP_ATTR_NODISCARD auto SpanAcross(
    std::size_t start_pos, std::size_t end_pos) const -> SourceSpan;

  /// The whole file, for a scope that covers it rather than sitting at a position in it.
  SPP_ATTR_NODISCARD auto SpanOfWholeFile() const -> SourceSpan;

  /// The line and column a position of this file is at, or none
  /// for one with no place in the author's source (generated
  /// code, the prelude behind it, another file's position). For
  /// every instruction's debug location, so a lookup in the line
  /// table, not a scan.
  SPP_ATTR_NODISCARD auto LineColOf(
    std::size_t position) const -> Opt<LineCol>;

private:
  /// Everything both the quoted line and the span are built
  /// from: which line the position falls on, that line's text,
  /// and where within it the underlined part starts and how far
  /// it runs, in bytes of "LineText".
  struct _RawLocation {
    bool Generated = false;
    bool PastUserSource = false;
    std::size_t Line = 0;
    Str LineText;
    std::size_t ByteOffset = 0;
    std::size_t ByteSpan = 0;
  };

  /// A position quoted for an error: where it is ("on line 3"),
  /// the line's number and text, the gutter's padding, and the
  /// caret row under the quoted text.
  struct _Quote {
    Str Location;
    Str LineNumber;
    Str LineText;
    Str LeftPadding;
    Str Carets;
  };

  /// Locate a raw position: the shared half of the quote and
  /// the span, with no formatting of any kind.
  SPP_ATTR_NODISCARD auto _LocateRawPos(
    std::size_t ast_start_pos, std::size_t ast_size) const -> _RawLocation;

  /// The token index into this file a position is, if it is one
  /// of this file's.
  SPP_ATTR_NODISCARD auto _TokenIndexOf(
    std::size_t position) const -> Opt<std::size_t>;

  /// The token index of the author's code a position is: none for
  /// a position that is not this file's, or is the lexer's leading
  /// newline, or lies in the prelude behind the author's code.
  SPP_ATTR_NODISCARD auto _UserTokenIndexOf(
    std::size_t position) const -> Opt<std::size_t>;

  /// The one-based line token "index" is on ("_LineStarts").
  SPP_ATTR_NODISCARD auto _LineOf(
    std::size_t index) const -> std::size_t;

  /// The tokens on one-based "line", as [first, last): the line's
  /// own, without the newline that ends it.
  SPP_ATTR_NODISCARD auto _LineBounds(
    std::size_t line) const -> Pair<std::size_t, std::size_t>;

  /// The bytes of tokens [first, last).
  SPP_ATTR_NODISCARD auto _ByteLength(
    std::size_t first, std::size_t last) const -> std::size_t;

  /// Token list to index into.
  Vec<lex::RawToken> _Tokens;

  /// This file's first position (0 for a formatter owning none):
  /// token "i" is at position "_Base + i".
  std::size_t _Base = 0;

  /// The file path of the source file being parsed.
  Str _FilePath;

  /// The index of the first token in the prelude.
  std::size_t _PreludeTokenIndex;

  /// The token index each line starts at, the first line first:
  /// the token after each newline. A newline is any token that is
  /// one, the line feeds between tokens and those inside a string
  /// literal alike (which the lexer gives as characters), so a
  /// line counts as the author's editor counts it.
  Vec<std::size_t> _LineStarts;

  /// The core error formatter. Handles almost all formatting
  /// between the different entry api functions.
  auto _QuoteRawPos(
    std::size_t ast_start_pos, std::size_t ast_size,
    Str &&tag_message) const -> _Quote;

  /// An error ("is_error", red, headed "Error") or its context
  /// (green, headed "Context") quoted at a position.
  SPP_ATTR_NODISCARD auto _Render(
    bool is_error, std::size_t ast_start_pos, std::size_t ast_size,
    Str &&tag_message) const -> Str;
};
