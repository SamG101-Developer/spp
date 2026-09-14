module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterTypeVariadicAst);
use(spp::asts, struct TokenAst);

namespace spp::asts::detail {
  template <>
  struct make_variadic_param<GenericParameterTypeAst> {
    using type = GenericParameterTypeVariadicAst;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeVariadicAst final : GenericParameterTypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericParameterTypeVariadicAst);

  /// The ".." token marking the generic parameter as
  /// variadic, meaning it can accept multiple values.
  Unique<TokenAst> TokEllipsis;

  GenericParameterTypeVariadicAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Name) &&name,
    decltype(Constraints) &&constraints);

  ~GenericParameterTypeVariadicAst() override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterTypeVariadicAst)
