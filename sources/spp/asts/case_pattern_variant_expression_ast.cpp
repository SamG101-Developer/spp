module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
    decltype(Expr) &&expr) :
    Expr(std::move(expr)) {
}

spp::asts::CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;

auto spp::asts::CasePatternVariantExpressionAst::PosStart() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosStart();
}

auto spp::asts::CasePatternVariantExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosEnd();
}

auto spp::asts::CasePatternVariantExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantExpressionAst>(AstClone(Expr));
}

auto spp::asts::CasePatternVariantExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Forward analysis into the expression.
    Expr->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Expr, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Expr));

    CreateAndAnalysePatternEqFuncsDummyCore(
        {this}, sm, meta);
}

auto spp::asts::CasePatternVariantExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Check the memory of the expression. todo: maybe do this via generated == function?
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *Expr, *sm, true, true, true, true, true, meta);
}

auto spp::asts::CasePatternVariantExpressionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Transform the pattern into comptime values; all need to be true.
    using analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime;
    auto comptime_transforms = CreateAndAnalysePatternEqCompTime(
        {this}, sm, meta);

    // Return the single result (only one expression will be here).
    meta->CmpResult = std::move(comptime_transforms[0]);
}

auto spp::asts::CasePatternVariantExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm;

    //
    const auto llvm_master_transform = CreateAndAnalysePatternEqFuncsLlvm(
        {this}, sm, meta, ctx);
    return llvm_master_transform[0];
}

SPP_MOD_END
