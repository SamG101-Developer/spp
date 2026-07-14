module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeVariadicAst;
    SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeVariadicAst final : GenericParameterTypeAst {
    /**
     * The token that represents the @c .. ellipsis in the generic parameter. This indicates that the parameter is
     * variadic, meaning it can accept multiple values.
     */
    Unique<TokenAst> TokEllipsis;

    /**
     * Construct the GenericParameterTypeVariadicAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     * @param tok_ellipsis The token that represents the @c .. ellipsis in the generic parameter.
     */
    GenericParameterTypeVariadicAst(
        decltype(TokEllipsis) &&tok_ellipsis,
        decltype(Name) &&name,
        decltype(Constraints) &&constraints);

    ~GenericParameterTypeVariadicAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
