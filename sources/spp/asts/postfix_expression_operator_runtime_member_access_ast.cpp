#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(tok_dot) &&tok_dot,
    decltype(name) &&name):
    tok_dot(std::move(tok_dot)),
    name(std::move(name)) {
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_start() const -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}
