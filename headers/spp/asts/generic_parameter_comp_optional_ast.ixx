module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterCompOptionalAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

namespace spp::asts::detail {
  template <>
  struct make_optional_param<GenericParameterCompAst> {
    using type = GenericParameterCompOptionalAst;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterCompOptionalAst final : GenericParameterCompAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericParameterCompOptionalAst);

  /// The "=" token separating the parameter from its default.
  Unique<TokenAst> TokAssign;

  /// The default value, used if the argument is not provided.
  Unique<ExpressionAst> DefaultVal;

  GenericParameterCompOptionalAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) &&name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val);

  ~GenericParameterCompOptionalAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterCompOptionalAst);
