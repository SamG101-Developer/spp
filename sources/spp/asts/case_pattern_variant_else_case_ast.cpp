module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
    decltype(TokElse) &&tok_else,
    decltype(CaseExpr) &&case_expr) :
    TokElse(std::move(tok_else)),
    CaseExpr(std::move(case_expr)) {
}

spp::asts::CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;

auto spp::asts::CasePatternVariantElseCaseAst::PosStart() const
    -> std::size_t {
    // Use the "else" token.
    return TokElse->PosStart();
}

auto spp::asts::CasePatternVariantElseCaseAst::PosEnd() const
    -> std::size_t {
    // Use the [case expression]'s condition.
    return CaseExpr->Cond->PosEnd();
}

auto spp::asts::CasePatternVariantElseCaseAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantElseCaseAst>(
        AstClone(TokElse), AstClone(CaseExpr));
}

auto spp::asts::CasePatternVariantElseCaseAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokElse);
    SPP_STRING_APPEND(CaseExpr);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantElseCaseAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the case expression.
    CaseExpr->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::CasePatternVariantElseCaseAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the case expression.
    CaseExpr->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::CasePatternVariantElseCaseAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Get the comptime result from the case expression.
    CaseExpr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::CasePatternVariantElseCaseAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Delegate code generation to the case expression.
    return CaseExpr->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
