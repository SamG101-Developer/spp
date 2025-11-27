module;
#include <spp/macros.hpp>

module spp.asts.type_parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;


spp::asts::TypeParenthesisedExpressionAst::TypeParenthesisedExpressionAst(
    decltype(tok_l) &&tok_l,
    decltype(expr) &&expr,
    decltype(tok_r) &&tok_r) :
    TempTypeAst(),
    tok_l(std::move(tok_l)),
    expr(std::move(expr)),
    tok_r(std::move(tok_r)) {
}


spp::asts::TypeParenthesisedExpressionAst::~TypeParenthesisedExpressionAst() = default;


auto spp::asts::TypeParenthesisedExpressionAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::TypeParenthesisedExpressionAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::TypeParenthesisedExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<TypeParenthesisedExpressionAst>(
        ast_clone(tok_l),
        ast_clone(expr),
        ast_clone(tok_r));
}


spp::asts::TypeParenthesisedExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(expr);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::TypeParenthesisedExpressionAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::TypeParenthesisedExpressionAst::convert()
    -> std::unique_ptr<TypeAst> {
    return std::unique_ptr<TypeAst>(expr.get());
}
