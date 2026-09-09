module;
#include <spp/macros.hpp>

export module spp.lex.lexer;
import spp.lex.tokens;
import spp.utils.types;
import std;

namespace spp::lex {
  SPP_EXP_CLS class Lexer;
}

/**
 * The Lexer is responsible for converting source code into a list of tokens. It takes a string of source code as input
 * and produces a vector of @c RawToken objects as output.
 */
SPP_EXP_CLS class spp::lex::Lexer {
  Str m_code;
  std::size_t m_PreludeCharOffset = Str::npos; // Where the prelude starts in m_code
  std::size_t m_PreludeTokenIndex = Str::npos; // Where the prelude starts by token index

public:
  explicit Lexer(Str code, bool add_prelude = false);
  SPP_ATTR_NODISCARD auto Lex() -> Vec<RawToken>;

  /**
   * The first token of the prelude, or @c npos if there is no prelude in this module. The prelude is appended to the
   * source rather than prepended, so that it does not shift the line numbers of the code someone actually wrote. This
   * creates the issue of having compiler-injected code in syntax error messages. Hide by index.
   */
  SPP_ATTR_NODISCARD auto PreludeTokenIndex() const -> std::size_t { return m_PreludeTokenIndex; }
};
