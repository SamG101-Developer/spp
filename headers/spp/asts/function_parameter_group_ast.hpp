#ifndef FUNCTION_PARAMETER_GROUP_AST_HPP
#define FUNCTION_PARAMETER_GROUP_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * A FunctionParameterGroupAst is used to represent a group of function parameters in a function prototype.
 */
struct spp::asts::FunctionParameterGroupAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

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
};


#endif //FUNCTION_PARAMETER_GROUP_AST_HPP
