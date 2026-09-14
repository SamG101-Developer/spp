module;
#include <magic_enum/magic_enum.hpp>
#include <spp/macros.hpp>

export module spp.asts.token_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.lex.tokens;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TokenAst);
use(spp::asts, struct TypeIdentifierAst);

/// A low level token created by the lexer, such as "=" or "+"
/// in statements and expressions. Associated token data is
/// needed when strings or numbers are created, for example.
/// This ast is also the terminator for "pos_end()" recursive
/// calls; the end position is the start position plus the
/// length of the associated data.
SPP_EXP_CLS struct spp::asts::TokenAst final : Ast {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TokenAst);

  /// Very similar to the constructor, but required for the
  /// macro'd unified "new_empty" caller for defaulting
  /// attributes. The pos isn't always given so is optional.
  static auto NewEmpty(lex::SppTokenType token_type, Str &&token_data, std::size_t pos = 0) -> Unique<TokenAst>;

  /// The token type (part of the enum) this ast is wrapping.
  lex::SppTokenType TokenType;

  /// The associated data to that token (normally the string
  /// representation).
  Str TokenData;

  TokenAst(
    std::size_t pos,
    lex::SppTokenType token_type,
    Str &&token_data);

  ~TokenAst() override;

  /// Two tokens are equal if their token types are equal.
  auto operator==(TokenAst const &that) const -> bool;

  auto PatchPos(std::size_t pos) -> void;

private:
  std::size_t _Pos;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TokenAst)
