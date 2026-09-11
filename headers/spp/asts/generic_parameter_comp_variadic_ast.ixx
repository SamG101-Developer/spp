module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.ast_kind;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterCompVariadicAst) {
  SPP_EXP_CLS struct TokenAst;
}

namespace spp::asts::detail {
  template <>
  struct make_variadic_param<GenericParameterCompAst> {
    using type = GenericParameterCompVariadicAst;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterCompVariadicAst final : GenericParameterCompAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericParameterCompVariadicAst);

  /**
     * The token that represents the @c .. ellipsis in the generic parameter. This indicates that the parameter is
     * variadic, meaning it can accept multiple values.
     */
  Unique<TokenAst> TokEllipsis;

  /**
     * Construct the GenericParameterCompVariadicAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the generic parameter.
     */
  GenericParameterCompVariadicAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type);

  ~GenericParameterCompVariadicAst() override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterCompVariadicAst)
