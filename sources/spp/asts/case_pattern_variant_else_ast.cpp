module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_ast;
import spp.lex.tokens;
import spp.asts.boolean_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantElseAst::CasePatternVariantElseAst(
    decltype(TokElse) &&tok_else) :
    TokElse(std::move(tok_else)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokElse, lex::SppTokenType::KW_ELSE, "else");
}

spp::asts::CasePatternVariantElseAst::~CasePatternVariantElseAst() = default;

auto spp::asts::CasePatternVariantElseAst::PosStart() const
    -> std::size_t {
    // Use the "else" token.
    return TokElse->PosStart();
}

auto spp::asts::CasePatternVariantElseAst::PosEnd() const
    -> std::size_t {
    // Use the "else" token,
    return TokElse->PosEnd();
}

auto spp::asts::CasePatternVariantElseAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CasePatternVariantElseAst>(
        AstClone(TokElse));
}

auto spp::asts::CasePatternVariantElseAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokElse);
    SPP_STRING_END;
}

auto spp::asts::CasePatternVariantElseAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *meta)
    -> void {
    // The "else" pattern always matches, so return "true".
    meta->CmpResult = BooleanLiteralAst::True(TokElse->PosStart());
}

auto spp::asts::CasePatternVariantElseAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // The "else" pattern always matches, so return "true".
    return llvm::ConstantInt::getTrue(*ctx->Context);
}

auto spp::asts::CasePatternVariantElseAst::MarkForIterLoopExit()
    -> void {
    _ForIterLoopExit = true;
}

auto spp::asts::CasePatternVariantElseAst::MarkedForIterLoopExit() const
    -> bool {
    return _ForIterLoopExit;
}

SPP_MOD_END
