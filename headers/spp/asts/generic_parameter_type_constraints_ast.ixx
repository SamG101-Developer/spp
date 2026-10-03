module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeConstraintsAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::GenericParameterTypeConstraintsAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeConstraintsAst);

  /// The ":" token that introduces the inline constraints.
  Unique<TokenAst> TokColon;

  /// The constraints for the generic type parameter. Any
  /// generic argument passed into the generic parameter must
  /// satisfy these constraints.
  Vec<Shared<TypeAst>> Constraints;

  static auto NewEmpty() -> Unique<GenericParameterTypeConstraintsAst>;

  GenericParameterTypeConstraintsAst(
    decltype(TokColon) &&tok_colon,
    Vec<Unique<TypeAst>> &&constraints);

  ~GenericParameterTypeConstraintsAst() override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
