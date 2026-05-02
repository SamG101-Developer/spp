module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
}


SPP_EXP_CLS struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
    /**
     * The convention is attached to the self parameter rather than its type, as it is required before the type is
     * necessarily attached.
     */
    Unique<ConventionAst> Conv;

    /**
     * Construct the FunctionParameterSelfAst with the arguments matching the members.
     * @param conv The convention token for this parameter.
     * @param var The local variable declaration for this parameter.
     */
    FunctionParameterSelfAst(
        decltype(Conv) &&conv,
        decltype(Var) &&var);

    ~FunctionParameterSelfAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
