module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.mem_utils;
import spp.asts.ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;


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


auto spp::asts::CasePatternVariantExpressionAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(expr.get());
    expr->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the memory of the expression. todo: maybe do this via generated == function?
    expr->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *expr, *expr, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::CasePatternVariantExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Turn the "literal part" into a function argument.
    auto eq_arg_conv = std::make_unique<ConventionRefAst>(nullptr);
    auto eq_arg_val = ast_clone(expr.get());
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
