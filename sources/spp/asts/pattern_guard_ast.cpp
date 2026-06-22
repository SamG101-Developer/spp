module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.pattern_guard_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::PatternGuardAst::PatternGuardAst(
    decltype(TokAnd) &&tok_and,
    decltype(Expr) &&expression) :
    TokAnd(std::move(tok_and)),
    Expr(std::move(expression)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAnd, lex::SppTokenType::KW_AND, "and", Expr ? Expr->PosStart() : 0);
}

spp::asts::PatternGuardAst::~PatternGuardAst() = default;

auto spp::asts::PatternGuardAst::PosStart() const
    -> std::size_t {
    // Use the "and" token.
    return TokAnd->PosStart();
}

auto spp::asts::PatternGuardAst::PosEnd() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosEnd();
}

auto spp::asts::PatternGuardAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PatternGuardAst>(
        AstClone(TokAnd), AstClone(Expr));
}

auto spp::asts::PatternGuardAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokAnd);
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::PatternGuardAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppExpressionNotBooleanError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::IsTypeBool;

    // Check the expression in the pattern guard.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Expr, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Expr.get()));
    Expr->Stage7_AnalyseSemantics(sm, meta);

    // Check the guard's type is boolean.
    const auto expr_type = Expr->InferType(sm, meta);
    RaiseIf<SppExpressionNotBooleanError>(
        not IsTypeBool(*expr_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Expr, *expr_type, "pattern guard"));
}

auto spp::asts::PatternGuardAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the memory of the expression.
    // Todo: how is this even applied? just truth check => barely any mem checks needed
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *this, *sm, true, true, false, false, false, meta);
}

auto spp::asts::PatternGuardAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Resolve the expression at compile-time.
    Expr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PatternGuardAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the expression.
    return Expr->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
