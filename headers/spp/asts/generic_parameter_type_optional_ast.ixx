module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeOptionalAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::GenericParameterTypeOptionalAst final : GenericParameterTypeAst {
    /**
     * The token that separates the parameter name from the default value.
     */
    Unique<TokenAst> TokAssign;

    /**
     * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
     */
    Unique<TypeAst> DefaultVal;

    /**
     * Construct the GenericParameterTypeOptionalAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     * @param tok_assign The token that separates the parameter name from the default value.
     * @param default_val The default value for the parameter.
     */
    GenericParameterTypeOptionalAst(
        decltype(Name) &&name,
        decltype(Constraints) &&constraints,
        decltype(TokAssign) &&tok_assign,
        decltype(DefaultVal) &&default_val);

    ~GenericParameterTypeOptionalAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};
