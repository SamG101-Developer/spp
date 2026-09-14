module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableSingleIdentifierAliasAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::LocalVariableSingleIdentifierAliasAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(LocalVariableSingleIdentifierAliasAst);

  /// The "as" token separating the identifier from its alias.
  Unique<TokenAst> TokAs;

  /// The alias for the local variable. This will be the name
  /// on the symbol that is introduced.
  Shared<IdentifierAst> Name;

  LocalVariableSingleIdentifierAliasAst(
    decltype(TokAs) &&tok_as,
    decltype(Name) &&name);

  ~LocalVariableSingleIdentifierAliasAst() override;
};
