module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.parenthesised_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::ParenthesisedExpressionAst::ParenthesisedExpressionAst(
    decltype(TokL) &&tok_open_paren,
    decltype(Expr) &&expr,
    decltype(TokR) &&tok_close_paren) :
    TokL(std::move(tok_open_paren)),
    Expr(std::move(expr)),
    TokR(std::move(tok_close_paren)) {
}

spp::asts::ParenthesisedExpressionAst::~ParenthesisedExpressionAst() = default;

auto spp::asts::ParenthesisedExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "(" token.
    return TokL->PosStart();
}

auto spp::asts::ParenthesisedExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the ")" token.
    return TokR->PosEnd();
}

auto spp::asts::ParenthesisedExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ParenthesisedExpressionAst>(
        AstClone(TokL), AstClone(Expr), AstClone(TokR));
}

auto spp::asts::ParenthesisedExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_APPEND(Expr);
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::ParenthesisedExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppExpressionTypeInvalidError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Forward analysis into the expression.
    RaiseIf<SppExpressionTypeInvalidError>(
        not IsPrimaryExprTypeValid(*Expr),
        {sm->CurrentScope}, ERR_ARGS(*Expr.get()));
    Expr->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ParenthesisedExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the memory of the expression.
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(
        *Expr, *this, *sm, true, true, true, false, false, meta);
}

auto spp::asts::ParenthesisedExpressionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward comptime resolution into the expression.
    Expr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::ParenthesisedExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the inner expression.
    return Expr->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::ParenthesisedExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Get the inner expression's type.
    return Expr->InferType(sm, meta);
}

SPP_MOD_END
