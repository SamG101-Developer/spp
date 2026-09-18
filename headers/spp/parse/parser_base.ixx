module;
#include <spp/macros.hpp>

export module spp.parse.parser_base;
import spp.lex.tokens;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

use(spp::parse::errors, struct SppSyntaxError);
use(spp::parse::errors, template <typename T> struct SyntacticErrorBuilder);
use(spp::parse, class ParserBase);

/// The base class for all parsers. This is so that we can
/// have a consistent set of tools for parsing, and currently
/// there is only the standard s++ parser, but will expand
/// to possibly have a restricted "stub" parser for ffi stub
/// files.
SPP_EXP_CLS class spp::parse::ParserBase {
public:
  explicit ParserBase(
    Vec<lex::RawToken> tokens,
    Shared<utils::errors::ErrorFormatter> const &error_formatter = nullptr);
  virtual ~ParserBase();

protected:
  /// The current position through the token set.
  std::size_t _Pos = 0uz;

  /// The token set being parsed.
  Vec<lex::RawToken> _Tokens = {};

  /// The length of the token set (cached).
  std::size_t _TokensLen = 0uz;

  /// The local error builder.
  Unique<SyntacticErrorBuilder<SppSyntaxError>> _ErrorBuilder;

  /// The error formatter to use, bound to the token set.
  Shared<utils::errors::ErrorFormatter> _ErrorFormatter;

  /// Helper method type.
  template <typename T>
  using parser_method_t = std::function<Unique<T>()>;

  /// Helper method type.
  template <typename T>
  using parser_method_alt_t = std::function<T()>;
};
