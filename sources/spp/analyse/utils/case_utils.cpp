module spp.analyse.utils.case_utils;
import spp.analyse.scopes.scope_manager;
import spp.asts.utils.ast_utils;
import spp.asts.ast;
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
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import genex;
import std;

template <typename T>
auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsCore(
    Vec<asts::CasePatternVariantAst*> const &elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    std::copyable_function<T(asts::Ast *)> &&mapper)
    -> Vec<T> {
    auto transformed = Vec<T>();
    transformed.reserve(elems.Len());

    if (not elems.IsEmpty() and elems[0]->To<asts::CasePatternVariantExpressionAst>()) {
        // For expression patterns, just generate the expression once.
        const auto expr_part = elems[0]->To<asts::CasePatternVariantExpressionAst>();
        auto transform = mapper(expr_part->Expr.get());
        transformed.EmplaceBack(std::move(transform));
        return transformed;
    }

    for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks.
        if (part->To<asts::CasePatternVariantLiteralAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = MakeShared<asts::IdentifierAst>(0uz, std::to_string(i));
            auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = MakeUnique<asts::PostfixExpressionAst>(asts::AstClone(meta->CaseCondition), std::move(field));

            // Turn the "literal part" into a function argument.
            auto eq_arg_conv = MakeUnique<asts::ConventionRefAst>(nullptr);
            auto eq_arg_val = asts::AstClone(part->To<asts::CasePatternVariantLiteralAst>()->Literal->To<asts::ExpressionAst>());
            auto eq_arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(eq_arg_val));

            // Create the ".eq" part.
            auto eq_field_name = MakeShared<asts::IdentifierAst>(0uz, "eq");
            auto eq_field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
            auto eq_pf_expr = MakeUnique<asts::PostfixExpressionAst>(std::move(pf_expr), std::move(eq_field));

            // Make the ".eq" part callable, as ".eq()" (no arguments right now)
            auto eq_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
            eq_call->FnArgGroup->Args.EmplaceBack(std::move(eq_arg));
            const auto eq_call_expr = MakeUnique<asts::PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));

            const auto current_scope = sm->CurrentScope;
            const auto current_scope_iter = sm->CurrentIterator();
            eq_call_expr->Stage7_AnalyseSemantics(sm, meta);
            sm->Reset(current_scope, current_scope_iter);

            // Generate the equality check.
            auto transform = mapper(eq_call_expr.get());
            transformed.EmplaceBack(std::move(transform));
        }

        // For nested objects (array, tuple, object)
        else if (
            part->To<asts::CasePatternVariantDestructureArrayAst>() != nullptr or
            part->To<asts::CasePatternVariantDestructureTupleAst>() != nullptr or
            part->To<asts::CasePatternVariantDestructureObjectAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = MakeShared<asts::IdentifierAst>(0uz, std::to_string(i));
            auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = MakeUnique<asts::PostfixExpressionAst>(asts::AstClone(meta->CaseCondition), std::move(field));

            // Update the "meta->cond" with the "pf_expr", and analyse against the inner part.
            meta->Save();
            meta->CaseCondition = pf_expr.get();

            // Combine the result.
            auto transform = mapper(part);
            transformed.EmplaceBack(std::move(transform));
            meta->Restore();
        }
    }

    return transformed;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsLlvm(
    Vec<asts::CasePatternVariantAst*> const &elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> Vec<llvm::Value*> {
    // Get the expression and map then to LLVM values.
    std::copyable_function<llvm::Value*(asts::Ast *)> map = [&](asts::Ast *x) {
        return x->Stage11_CodeGen(sm, meta, ctx);
    };

    auto asts = CreateAndAnalysePatternEqFuncsCore(elems, sm, meta, std::move(map));
    return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime(
    Vec<asts::CasePatternVariantAst*> const &elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Vec<Unique<asts::ExpressionAst>> {
    // Get the expression and map then to Comptime values.
    std::copyable_function<Unique<asts::ExpressionAst>(asts::Ast *)> map = [&](asts::Ast *x) {
        x->Stage9_CompTimeResolve(sm, meta);
        return std::move(meta->CmpResult);
    };

    auto asts = CreateAndAnalysePatternEqFuncsCore(elems, sm, meta, std::move(map));
    return asts;
}

auto spp::analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore(
    Vec<asts::CasePatternVariantAst*> const &elems,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    for (auto const &[i, part] : elems | genex::views::enumerate) {
        // For literals and expressions, generate the equality checks using dummy values.

        if (part->To<asts::CasePatternVariantLiteralAst>() != nullptr) {
            // Generate the extraction on the condition for this part, like "cond.0".
            auto field_name = MakeShared<asts::IdentifierAst>(0uz, std::to_string(i));
            auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
            auto pf_expr = MakeUnique<asts::PostfixExpressionAst>(asts::AstClone(meta->CaseCondition), std::move(field));
            auto lhs_type = pf_expr->InferType(sm, meta);

            // Turn the "literal part" into a function argument.
            auto eq_arg_conv = MakeUnique<asts::ConventionRefAst>(nullptr);
            auto eq_arg_val = asts::AstClone(part->To<asts::CasePatternVariantLiteralAst>()->Literal->To<asts::ExpressionAst>());
            auto rhs_type = eq_arg_val->InferType(sm, meta);

            // Create dummy values of the lhs and rhs types.
            auto lhs_dummy = MakeUnique<asts::ObjectInitializerAst>(std::move(lhs_type), nullptr);
            auto rhs_dummy = MakeUnique<asts::ObjectInitializerAst>(std::move(rhs_type), nullptr);
            auto eq_arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(std::move(eq_arg_conv), nullptr, std::move(rhs_dummy));

            // Create the ".eq" part.
            auto eq_field_name = MakeShared<asts::IdentifierAst>(0uz, "eq");
            auto eq_field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(eq_field_name));
            auto eq_pf_expr = MakeUnique<asts::PostfixExpressionAst>(std::move(lhs_dummy), std::move(eq_field));

            // Make the ".eq" part callable, as ".eq()" (no arguments right now)
            auto eq_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
            eq_call->FnArgGroup->Args.EmplaceBack(std::move(eq_arg));
            const auto eq_call_expr = MakeUnique<asts::PostfixExpressionAst>(std::move(eq_pf_expr), std::move(eq_call));
            eq_call_expr->Stage7_AnalyseSemantics(sm, meta);
        }
    }
}
