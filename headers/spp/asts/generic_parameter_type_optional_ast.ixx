module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_optional_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeOptionalAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

namespace spp::asts::detail {
  template <>
  struct make_optional_param<GenericParameterTypeAst> {
    using type = GenericParameterTypeOptionalAst;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeOptionalAst final : GenericParameterTypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeOptionalAst);

  /// The token that separates the parameter name from the
  /// default value.
  Unique<TokenAst> TokAssign;

  /// The default value for the parameter, used if the
  /// parameter is not provided.
  Shared<TypeAst> DefaultVal;

  GenericParameterTypeOptionalAst(
    decltype(Name) &&name,
    decltype(Constraints) &&constraints,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val);

  ~GenericParameterTypeOptionalAst() override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterTypeOptionalAst);
