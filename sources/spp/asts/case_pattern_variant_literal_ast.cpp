module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_literal_ast;
import spp.analyse.utils.case_utils;
import spp.asts.convention_ref_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;


spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
    decltype(literal) &&literal) :
    CasePatternVariantAst(),
    literal(std::move(literal)) {
}


spp::asts::CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;


auto spp::asts::CasePatternVariantLiteralAst::pos_start() const
    -> std::size_t {
    return literal->pos_start();
}


auto spp::asts::CasePatternVariantLiteralAst::pos_end() const
    -> std::size_t {
    return literal->pos_end();
}


auto spp::asts::CasePatternVariantLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantLiteralAst>(
        ast_clone(literal));
}


spp::asts::CasePatternVariantLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(literal);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantLiteralAst::convert_to_variable(
    CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable literal binding AST.
    const auto uid = spp::utils::generate_uid(this);
    auto var_name = std::make_unique<IdentifierAst>(pos_start(), uid);
    auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
    var->mark_from_case_pattern();
    return var;
}


auto spp::asts::CasePatternVariantLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the literal.
    literal->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantLiteralAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the literal.
    literal->stage_8_check_memory(sm, meta);
}


auto spp::asts::CasePatternVariantLiteralAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Transform the pattern into comptime values; all need to be true.
    auto comptime_tranforms = analyse::utils::case_utils::create_and_analyse_pattern_eq_comptime(
        {this}, sm, meta);

    // Return the single result (only one literal will be here).
    meta->cmp_result = std::move(comptime_tranforms[0]);
}


auto spp::asts::CasePatternVariantLiteralAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    const auto llvm_master_transform = analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_llvm(
        {this}, sm, meta, ctx);
    return llvm_master_transform[0];
}
