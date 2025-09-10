#pragma once
#include <spp/asts/function_parameter_ast.hpp>


struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
    SPP_AST_KEY_FUNCTIONS;

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

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
