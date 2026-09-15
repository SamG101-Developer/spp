module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeInlineConstraintsAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::GenericParameterTypeInlineConstraintsAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeInlineConstraintsAst);

  /// The ":" token that introduces the inline constraints.
  Unique<TokenAst> TokColon;

  /// The constraints for the generic type parameter. Any
  /// generic argument passed into the generic parameter must
  /// satisfy these constraints.
  Vec<Shared<TypeAst>> Constraints;

  static auto NewEmpty() -> Unique<GenericParameterTypeInlineConstraintsAst>;

  GenericParameterTypeInlineConstraintsAst(
    decltype(TokColon) &&tok_colon,
    Vec<Unique<TypeAst>> &&constraints);

  ~GenericParameterTypeInlineConstraintsAst() override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
