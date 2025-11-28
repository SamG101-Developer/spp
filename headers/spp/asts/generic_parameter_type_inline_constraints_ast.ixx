module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeInlineConstraintsAst final : virtual Ast {
    /**
     * The @code :@endcode token that introduces the inline constraints.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The constraints for the generic type parameter. Any generic argument passed into the generic parameter must
     * satisfy these constraints.
     */
    std::vector<std::unique_ptr<TypeAst>> constraints;

    /**
     * Construct the GenericParameterTypeInlineConstraintsAst with the arguments matching the members.
     * @param tok_colon The @c : token that introduces the inline constraints.
     * @param constraints The constraints for the generic type parameter.
     */
    GenericParameterTypeInlineConstraintsAst(
        decltype(tok_colon) &&tok_colon,
        decltype(constraints) &&constraints);

    ~GenericParameterTypeInlineConstraintsAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
