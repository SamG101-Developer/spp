module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.unary_expression_operator_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::UnaryExpressionAst::UnaryExpressionAst(
    decltype(Op) &&tok_op,
    decltype(Expr) &&expr) :
    Op(std::move(tok_op)),
    Expr(std::move(expr)) {
}

spp::asts::UnaryExpressionAst::~UnaryExpressionAst() = default;

auto spp::asts::UnaryExpressionAst::PosStart() const
    -> std::size_t {
    // Use the operator.
    return Op->PosStart();
}

auto spp::asts::UnaryExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosEnd();
}

auto spp::asts::UnaryExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<UnaryExpressionAst>(
        AstClone(Op), AstClone(Expr));
}

auto spp::asts::UnaryExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Op);
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::UnaryExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppExpressionTypeInvalidError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Analyse the semantics of the right-hand-side.
    RaiseIf<SppExpressionTypeInvalidError>(
        not IsPrimaryExprTypeValid(*Expr),
        {sm->CurrentScope}, ERR_ARGS(*Expr));

    // Analyse the operator and right-hand-side expression.
    Expr->Stage7_AnalyseSemantics(sm, meta);

    meta->Save();
    meta->UnaryExpressionRhs = Expr.get();
    Op->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();
}

auto spp::asts::UnaryExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the right-hand-side.
    Expr->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::UnaryExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the right-hand-side expression.
    meta->Save();
    meta->UnaryExpressionRhs = Expr.get();
    const auto lhs_val = Op->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    return lhs_val;
}

auto spp::asts::UnaryExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Infer the type of the right-hand-side expression, adjusted by the operator.
    meta->Save();
    meta->UnaryExpressionRhs = Expr.get();
    auto type = Op->InferType(sm, meta);
    meta->Restore();

    return type;
}

SPP_MOD_END
