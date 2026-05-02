module;
#include <spp/macros.hpp>

module spp.asts.type_parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeParenthesisedExpressionAst::TypeParenthesisedExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Expr) expr,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Expr(std::move(expr)),
    TokR(std::move(tok_r)) {
}

spp::asts::TypeParenthesisedExpressionAst::~TypeParenthesisedExpressionAst() = default;

auto spp::asts::TypeParenthesisedExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "(" token.
    return TokL->PosStart();
}

auto spp::asts::TypeParenthesisedExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the ")" token.
    return TokR->PosEnd();
}

auto spp::asts::TypeParenthesisedExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeParenthesisedExpressionAst>(
        AstClone(TokL), AstCloneShared(Expr), AstClone(TokR));
}

auto spp::asts::TypeParenthesisedExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(Expr);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::TypeParenthesisedExpressionAst::Convert()
    -> Unique<TypeAst> {
    return Unique<TypeAst>(Expr.get()); // TODO: Release?
}

SPP_MOD_END
