#ifndef POSTFIX_EXPRESSION_OPERATOR_FUNCTION_CALL_HPP
#define POSTFIX_EXPRESSION_OPERATOR_FUNCTION_CALL_HPP

#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>


struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The generic argument group that contains the generic arguments for the function call.
     */
    std::unique_ptr<GenericArgumentGroupAst> generic_arg_group;

    /**
     * The function call argument group that contains the arguments for the function call.
     */
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;

    /**
     * The optional @c .. fold token that indicates a fold operation in a function call. This will fold all tuples in
     * the argument group into a single argument and call the function multiples times with each element of the tuple as
     * the argument for the non-tuple parameters it has mapped to.
     */
    std::unique_ptr<FoldExpressionAst> fold;

    /**
     * Construct the PostfixExpressionOperatorFunctionCallAst with the arguments matching the members.
     * @param[in] generic_arg_group The generic argument group that contains the generic arguments for the function
     * call.
     * @param[in] arg_group The function call argument group that contains the arguments for the function call.
     * @param[in] fold The optional @c .. fold token that indicates a fold operation in a function call.
     */
    explicit PostfixExpressionOperatorFunctionCallAst(
        decltype(generic_arg_group) &&generic_arg_group,
        decltype(arg_group) &&arg_group,
        decltype(fold) &&fold);
};


#endif //POSTFIX_EXPRESSION_OPERATOR_FUNCTION_CALL_HPP
