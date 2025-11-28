module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericParameterCompOptionalAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterCompOptionalAst final : GenericParameterCompAst {
    /**
     * The token that separates the parameter name from the default value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
     */
    std::unique_ptr<ExpressionAst> default_val;

    /**
     * Construct the GenericParameterCompOptionalAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     * @param tok_assign The token that separates the parameter name from the default value.
     * @param default_val The default value for the parameter.
     */
    GenericParameterCompOptionalAst(
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type,
        decltype(tok_assign) &&tok_assign,
        decltype(default_val) &&default_val);

    ~GenericParameterCompOptionalAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
