#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(tok_dbl_colon) &&tok_dbl_colon,
    decltype(name) &&name):
    tok_dbl_colon(std::move(tok_dbl_colon)),
    name(std::move(name)) {
}


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_start() const -> std::size_t {
    return tok_dbl_colon->pos_start();
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dbl_colon);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dbl_colon);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}
