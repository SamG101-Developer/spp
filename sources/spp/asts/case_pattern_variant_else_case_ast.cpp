module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;


spp::asts::CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
    decltype(tok_else) &&tok_else,
    decltype(case_expr) &&case_expr) :
    tok_else(std::move(tok_else)),
    case_expr(std::move(case_expr)) {
}


spp::asts::CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;


auto spp::asts::CasePatternVariantElseCaseAst::pos_start() const
    -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::CasePatternVariantElseCaseAst::pos_end() const
    -> std::size_t {
    return case_expr->pos_end();
}


auto spp::asts::CasePatternVariantElseCaseAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantElseCaseAst>(
        ast_clone(tok_else),
        ast_clone(case_expr));
}


spp::asts::CasePatternVariantElseCaseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_APPEND(case_expr);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantElseCaseAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_APPEND(case_expr);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantElseCaseAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the case expression.
    case_expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantElseCaseAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the case expression.
    case_expr->stage_8_check_memory(sm, meta);
}


auto spp::asts::CasePatternVariantElseCaseAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Delegate code generation to the case expression.
    return case_expr->stage_10_code_gen_2(sm, meta, ctx);
}
