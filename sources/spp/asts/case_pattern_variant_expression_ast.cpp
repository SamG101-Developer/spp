module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
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


spp::asts::CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
    decltype(expr) &&expr) :
    expr(std::move(expr)) {
}


spp::asts::CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;


auto spp::asts::CasePatternVariantExpressionAst::pos_start() const
    -> std::size_t {
    return expr->pos_start();
}


auto spp::asts::CasePatternVariantExpressionAst::pos_end() const
    -> std::size_t {
    return expr->pos_end();
}


auto spp::asts::CasePatternVariantExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantExpressionAst>(ast_clone(expr));
}


spp::asts::CasePatternVariantExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);

    analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_dummy_core(
        {this}, sm, meta);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression. todo: maybe do this via generated == function?
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *expr, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Transform the pattern into comptime values; all need to be true.
    auto comptime_tranforms = analyse::utils::case_utils::create_and_analyse_pattern_eq_comptime(
        {this}, sm, meta);

    // Return the single result (only one expression will be here).
    meta->cmp_result = std::move(comptime_tranforms[0]);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    const auto llvm_master_transform = analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_llvm(
        {this}, sm, meta, ctx);
    return llvm_master_transform[0];
}