module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_group_ast;
import spp.asts.ast;

import std;


/**
 * A FunctionParameterGroupAst is used to represent a group of function parameters in a function prototype.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterGroupAst final : virtual Ast {
    /**
     * The token that represents the left parenthesis @code (@endcode in the function parameter group. This introduces
     * the function parameter group.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of parameters in the function parameter group. This can contain both required and optional parameters.
     */
    std::vector<std::unique_ptr<FunctionParameterAst>> params;

    /**
     * The token that represents the right parenthesis @code )@endcode in the function parameter group. This closes
     * the function parameter group.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the FunctionParameterGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the function parameter group.
     * @param params The list of parameters in the function parameter group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the function parameter group.
     */
    FunctionParameterGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(params) &&params,
        decltype(tok_r) &&tok_r);

    ~FunctionParameterGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto get_all_params() const -> std::vector<FunctionParameterAst*>;

    auto get_self_param() const -> FunctionParameterSelfAst*;

    auto get_required_params() const -> std::vector<FunctionParameterRequiredAst*>;

    auto get_optional_params() const -> std::vector<FunctionParameterOptionalAst*>;

    auto get_variadic_param() const -> FunctionParameterVariadicAst*;

    auto get_non_self_params() const -> std::vector<FunctionParameterAst*>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
