module;
#include <spp/macros.hpp>

export module spp.lex.lexer;
import spp.lex.tokens;
import spp.utils.types;
import std;

use(spp::lex, class Lexer);

/// The lexer is responsible for converting source code into a list of tokens.
/// It matches by character, with a few rules for keywords and comments etc.
SPP_EXP_CLS class spp::lex::Lexer {
public:
  /// Build the lexer from the code, and the prelude to append behind it (none
  /// if empty). It goes after the code so line numbers stay the author's, which
  /// leaves the parser's error reporting to hide it ("PreludeTokenIndex").
  explicit Lexer(Str code, StrView prelude = {});

  /// Call the lex method on the built lexer. This detaches the construction of
  /// the lexer from the actual scan and token production.
  SPP_ATTR_NODISCARD auto Lex() -> Vec<RawToken>;

  /// The first token of the prelude, or "npos" if there is no prelude in this
  /// module. The prelude is appended, to maintain correct line reporting, but
  /// can create the issue of having compiler-injected code in syntax error
  /// messages. Hide by index
  SPP_ATTR_NODISCARD auto PreludeTokenIndex() const -> std::size_t { return _PreludeTokenIndex; }

private:
  /// The source code (one module at a time).
  Str _Code;

  /// Where the prelude starts in _Code
  std::size_t _PreludeCharOffset = Str::npos;

  /// Where the prelude starts by token index
  std::size_t _PreludeTokenIndex = Str::npos;
};
