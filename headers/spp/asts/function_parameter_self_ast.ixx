module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_ast;

import std;


SPP_EXP_CLS struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
    /**
     * The convention is attached to the self parameter rather than its type, as it is required before the type is
     * necessarily attached.
     */
    std::unique_ptr<ConventionAst> conv;

    /**
     * Construct the FunctionParameterSelfAst with the arguments matching the members.
     * @param conv The convention token for this parameter.
     * @param var The local variable declaration for this parameter.
     */
    FunctionParameterSelfAst(
        decltype(conv) &&conv,
        decltype(var) &&var);

    ~FunctionParameterSelfAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
