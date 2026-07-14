module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericParameterCompOptionalAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::GenericParameterCompOptionalAst final : GenericParameterCompAst {
    /**
     * The token that separates the parameter name from the default value.
     */
    Unique<TokenAst> TokAssign;

    /**
     * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
     */
    Unique<ExpressionAst> DefaultVal;

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
        decltype(TokCmp) &&tok_cmp,
        decltype(Name) &&name,
        decltype(TokColon) &&tok_colon,
        decltype(Type) &&type,
        decltype(TokAssign) &&tok_assign,
        decltype(DefaultVal) &&default_val);

    ~GenericParameterCompOptionalAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};
