#include <spp/asts/convention_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_keyword_res_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
    decltype(tok_dot) &&tok_dot,
    decltype(tok_res) &&tok_res,
    decltype(arg_group) &&arg_group):
    tok_dot(std::move(tok_dot)),
    tok_res(std::move(tok_res)),
    arg_group(std::move(arg_group)) {
}


spp::asts::PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;


auto spp::asts::PostfixExpressionOperatorKeywordResAst::pos_start() const -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::pos_end() const -> std::size_t {
    return arg_group ? arg_group->pos_end() : tok_res->pos_end();
}


spp::asts::PostfixExpressionOperatorKeywordResAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(tok_res);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordResAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(tok_res);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_END;
}
