module;
#include <spp/macros.hpp>

export module spp.utils.error_formatter;
import spp.lex.tokens;
import spp.utils.types;
import std;

use(spp::asts, struct Ast);
use(spp::utils::errors, class ErrorFormatter);

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
  auto ErrorRawPos(std::size_t ast_start_pos, std::size_t ast_size, Str &&message, Str &&tag_message) -> Str;

  /// Given an ast position and size, throw the error with
  /// the minimally formatted version of the error builder
  /// output.
  auto ErrorRawPosMinimal(std::size_t ast_start_pos, std::size_t ast_size, Str &&tag_message) -> Str;

  /// Given an entire ast, use the ast properties to
  /// determine what to highlight, and throw the error.
  auto ErrorAst(Ast const *ast, Str &&message, Str &&tag_message) -> Str;

  /// Minimal version of the ast scan based highlighting,
  /// typically for ast "context", not the "error" site.
  auto ErrorAstMinimal(Ast const *ast, Str &&tag_message) -> Str;

private:
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
