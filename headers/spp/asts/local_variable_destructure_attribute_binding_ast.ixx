module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureAttributeBindingAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureAttributeBindingAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureAttributeBindingAst);

  /// The name of the attribute on the object being
  /// destructured.
  Shared<IdentifierAst> Name;

  /// The "=" token that separates the attribute name from
  /// the value.
  Unique<TokenAst> TokAssign;

  /// The value of the attribute: a further destructure or a
  /// literal.
  Unique<LocalVariableAst> Val;

  LocalVariableDestructureAttributeBindingAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~LocalVariableDestructureAttributeBindingAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;
};
