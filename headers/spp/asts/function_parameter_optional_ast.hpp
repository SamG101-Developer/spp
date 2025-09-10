#pragma once
#include <spp/asts/function_parameter_ast.hpp>


/**
 * The FunctionParameterOptionalAst represents an optional parameter in a function prototype. It is used to define
 * parameters that are not required, and can be omitted when calling the function.
 */
struct spp::asts::FunctionParameterOptionalAst final : FunctionParameterAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that separates the parameter name from the default value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
     */
    std::unique_ptr<ExpressionAst> default_val;

    /**
     * Construct the FunctionParameterOptionalAst with the arguments matching the members.
     * @param var The local variable declaration for this parameter.
     * @param tok_colon The token that represents the @c : colon in the function parameter.
     * @param type The type of the parameter.
     * @param tok_assign The token that separates the parameter name from the default value.
     * @param default_val The default value for the parameter.
     */
    FunctionParameterOptionalAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type,
        decltype(tok_assign) &&tok_assign,
        decltype(default_val) &&default_val);

    ~FunctionParameterOptionalAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
