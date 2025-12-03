module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_literal_ast;
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


auto spp::asts::CasePatternVariantLiteralAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(literal);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantLiteralAst::convert_to_variable(
    CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable literal binding AST.
    auto var_name = std::make_unique<IdentifierAst>(pos_start(), std::format("$_{}", reinterpret_cast<std::uintptr_t>(this)));
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


auto spp::asts::CasePatternVariantLiteralAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Turn the "literal part" into a function argument.
    auto eq_arg_conv = std::make_unique<ConventionRefAst>(nullptr);
    auto eq_arg_val = ast_clone(literal.get());
    auto eq_arg = std::make_unique<FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(eq_arg_val));

    // Create the ".eq" part.
    auto eq_field_name = std::make_unique<IdentifierAst>(0, "eq");
    auto eq_field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
    auto eq_pf_expr = std::make_unique<PostfixExpressionAst>(ast_clone(meta->case_condition), std::move(eq_field));

    // Make the ".eq" part callable, as ".eq()" (no arguments right now)
    auto eq_call = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
    const auto eq_call_expr = std::make_unique<PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));
    eq_call->arg_group->args.emplace_back(std::move(eq_arg));

    // Generate the equality check.
    const auto llvm_call = eq_call_expr->stage_10_code_gen_2(sm, meta, ctx);
    return llvm_call;
}
