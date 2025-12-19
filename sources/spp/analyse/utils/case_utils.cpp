module spp.analyse.utils.case_utils;
import spp.analyse.scopes.scope_manager;
import spp.asts.utils.ast_utils;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.case_pattern_variant_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import genex;
import std;


auto spp::analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs(
    std::vector<asts::CasePatternVariantAst*> elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> std::vector<llvm::Value*> {
    auto llvm_values = std::vector<llvm::Value*>();
    llvm_values.reserve(elems.size());

    if (not elems.empty() and elems[0]->to<asts::CasePatternVariantExpressionAst>()) {
        // For expression patterns, just generate the expression once.
        const auto expr_part = elems[0]->to<asts::CasePatternVariantExpressionAst>();
        const auto llvm_call = expr_part->expr->stage_10_code_gen_2(sm, meta, ctx);
        llvm_values.emplace_back(llvm_call);
        return llvm_values;
    }

    for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks.

        if (part->to<asts::CasePatternVariantLiteralAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = std::make_unique<asts::IdentifierAst>(0, std::to_string(i));
            auto field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = std::make_unique<asts::PostfixExpressionAst>(ast_clone(meta->case_condition), std::move(field));

            // Turn the "literal part" into a function argument.
            auto eq_arg_conv = std::make_unique<asts::ConventionRefAst>(nullptr);
            auto eq_arg_val = asts::ast_clone(part->to<asts::CasePatternVariantLiteralAst>()->literal->to<asts::ExpressionAst>());
            auto eq_arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(eq_arg_val));

            // Create the ".eq" part.
            auto eq_field_name = std::make_unique<asts::IdentifierAst>(0, "eq");
            auto eq_field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
            auto eq_pf_expr = std::make_unique<asts::PostfixExpressionAst>(std::move(pf_expr), std::move(eq_field));

            // Make the ".eq" part callable, as ".eq()" (no arguments right now)
            auto eq_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
            eq_call->arg_group->args.emplace_back(std::move(eq_arg));
            const auto eq_call_expr = std::make_unique<asts::PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));

            // Generate the equality check.
            const auto current_scope = sm->current_scope;
            const auto current_scope_iterator = sm->current_iterator();
            eq_call_expr->stage_7_analyse_semantics(sm, meta);
            sm->reset(current_scope, current_scope_iterator);

            const auto llvm_call = eq_call_expr->stage_10_code_gen_2(sm, meta, ctx);
            llvm_values.emplace_back(llvm_call);
        }

        // For nested objects (array, tuple, object)
        else if (
            part->to<asts::CasePatternVariantDestructureArrayAst>() != nullptr or
            part->to<asts::CasePatternVariantDestructureTupleAst>() != nullptr or
            part->to<asts::CasePatternVariantDestructureObjectAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = std::make_unique<asts::IdentifierAst>(0, std::to_string(i));
            auto field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = std::make_unique<asts::PostfixExpressionAst>(ast_clone(meta->case_condition), std::move(field));

            // Update the "meta->cond" with the "pf_expr", and analyse against the inner part.
            meta->save();
            meta->case_condition = pf_expr.get();

            // Combine the result.
            const auto llvm_call = part->stage_10_code_gen_2(sm, meta, ctx);
            llvm_values.emplace_back(llvm_call);
            meta->restore();
        }
    }

    return llvm_values;
}


auto spp::analyse::utils::case_utils::create_and_analyse_pattern_eq_funcs_dummy(
    std::vector<asts::CasePatternVariantAst*> elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks using dummy values.

        if (part->to<asts::CasePatternVariantLiteralAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = std::make_unique<asts::IdentifierAst>(0, std::to_string(i));
            auto field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = std::make_unique<asts::PostfixExpressionAst>(asts::ast_clone(meta->case_condition), std::move(field));
            auto lhs_type = pf_expr->infer_type(sm, meta);

            // Turn the "literal part" into a function argument.
            auto eq_arg_conv = std::make_unique<asts::ConventionRefAst>(nullptr);
            auto eq_arg_val = asts::ast_clone(part->to<asts::CasePatternVariantLiteralAst>()->literal->to<asts::ExpressionAst>());
            auto rhs_type = eq_arg_val->infer_type(sm, meta);

            // Create dummy values of the lhs and rhs types.
            auto lhs_dummy = std::make_unique<asts::ObjectInitializerAst>(std::move(lhs_type), nullptr);
            auto rhs_dummy = std::make_unique<asts::ObjectInitializerAst>(std::move(rhs_type), nullptr);
            auto eq_arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(rhs_dummy));

            // Create the ".eq" part.
            auto eq_field_name = std::make_unique<asts::IdentifierAst>(0, "eq");
            auto eq_field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
            auto eq_pf_expr = std::make_unique<asts::PostfixExpressionAst>(std::move(lhs_dummy), std::move(eq_field));

            // Make the ".eq" part callable, as ".eq()" (no arguments right now)
            auto eq_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
            eq_call->arg_group->args.emplace_back(std::move(eq_arg));
            const auto eq_call_expr = std::make_unique<asts::PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));

            eq_call_expr->stage_7_analyse_semantics(sm, meta);
        }
    }
}
