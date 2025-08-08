#include <spp/asts/convention_ast.hpp>
#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
    decltype(generic_arg_group) &&generic_arg_group,
    decltype(arg_group) &&arg_group,
    decltype(fold) &&fold):
    PostfixExpressionOperatorAst(),
    generic_arg_group(std::move(generic_arg_group)),
    arg_group(std::move(arg_group)),
    fold(std::move(fold)) {
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_start() const -> std::size_t {
    return generic_arg_group->pos_start();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_end() const -> std::size_t {
    return fold ? fold->pos_end() : arg_group->pos_end();
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(generic_arg_group);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_APPEND(fold);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(generic_arg_group);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_APPEND(fold);
    SPP_PRINT_END;
}
