module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_early_return_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.is_expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.gen_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_materialize;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
    decltype(TokQst) &&tok_qst) :
    TokQst(std::move(tok_qst)) {
}

spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosStart() const
    -> std::size_t {
    // Use the "?" token.
    return TokQst->PosStart();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::PosEnd() const
    -> std::size_t {
    // Use the "?" token.
    return TokQst->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionOperatorEarlyReturnAst>(
        AstClone(TokQst));
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokQst);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::GetTryType;
    using analyse::utils::type_utils::TypeEq;
    using analyse::utils::type_utils::GetGenAndYieldTypes;

    // Get the left-hand-side information.
    const auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_type = lhs->InferType(sm, meta);

    // Ensure the left-hand-side superimposes the Try type.
    const auto try_type = GetTryType(*lhs_type, *lhs, *sm);

    // Check the Residual type is compatible with the function's return type.
    const auto residual_type = try_type->TypeParts().Back()->GnArgGroup->TypeAt("Residual")->Val;

    // Subroutine return type check.
    if (meta->EnclosingFunctionFlavour->TokenType == lex::SppTokenType::KW_FUN) {
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*meta->EnclosingFunctionRetType[0], *residual_type, *meta->EnclosingFunctionScope, *sm->CurrentScope),
            {meta->EnclosingFunctionScope, sm->CurrentScope},
            ERR_ARGS(*meta->EnclosingFunctionSourceRetType[0], *meta->EnclosingFunctionRetType[0], *lhs, *residual_type));
    }

    // Coroutine return type check.
    else {
        auto [_, yield_type, _] = GetGenAndYieldTypes(*meta->EnclosingFunctionRetType[0], *sm->CurrentScope, *lhs, "early return");
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*yield_type, *residual_type, *meta->EnclosingFunctionScope, *sm->CurrentScope),
            {meta->EnclosingFunctionScope, sm->CurrentScope},
            ERR_ARGS(*meta->EnclosingFunctionSourceRetType[0], *yield_type, *lhs, *residual_type));
    }
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::type_utils::GetTryType;

    // Extract the "try" information for the type.
    auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_type = lhs->InferType(sm, meta);
    const auto try_type = GetTryType(*lhs_type, *lhs, *sm);

    // Temp holder for non-symbolic condition.
    if (sm->CurrentScope->GetVarSymbolOutermost(*lhs).First == nullptr) {
        lhs = codegen::llvm_materialize(*lhs, sm, meta, ctx);
    }

    // Create the condition by attempting an "is" against the residual.
    const auto type_check = ( {
        auto is_expr_lhs = AstClone(lhs);
        auto is_expr_rhs = try_type->TypeParts().Back()->GnArgGroup->TypeAt("Residual")->Val;

        auto skip_all = MakeUnique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(nullptr, nullptr);
        auto destructures = Vec<Unique<CasePatternVariantAst>>{};
        destructures.EmplaceBack(std::move(skip_all));

        auto is_expr_rhs_wrap = MakeUnique<CasePatternVariantDestructureObjectAst>(std::move(is_expr_rhs), nullptr, std::move(destructures), nullptr);
        auto is_expr = MakeUnique<IsExpressionAst>(std::move(is_expr_lhs), nullptr, std::move(is_expr_rhs_wrap));
        std::move(is_expr);
    });

    // Ret statement if we are in a subroutine, otherwise
    // a gen statement, followed by a no-expr ret statement.
    // Todo: Test coroutines in test suite with "?"
    auto extra_statements = Vec<Unique<StatementAst>>();
    if (meta->EnclosingFunctionFlavour->TokenType == lex::SppTokenType::KW_FUN) {
        auto ret_expr = AstClone(lhs);
        auto ret_stmt = MakeUnique<RetStatementAst>(nullptr, std::move(ret_expr));
        extra_statements.EmplaceBack(std::move(ret_stmt));
    }
    else {
        auto ret_expr = AstClone(lhs);
        auto gen_expr = MakeUnique<GenExpressionAst>(nullptr, nullptr, std::move(ret_expr));
        auto ret_stmt = MakeUnique<RetStatementAst>(nullptr, nullptr);
        extra_statements.EmplaceBack(std::move(gen_expr));
        extra_statements.EmplaceBack(std::move(ret_stmt));
    }

    // Convert the "is" expression into a case condition with destructure.
    const auto current_scope = sm->CurrentScope;
    const auto current_scope_iterator = sm->CurrentIterator();

    type_check->Stage7_AnalyseSemantics(sm, meta);
    const auto check = type_check->GetMappedFunc();
    check->Branches[0]->Body->Members.AppendRange(std::move(extra_statements));
    sm->Reset(current_scope, current_scope_iterator);

    meta->Save();
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    check->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();

    // Next, in case the try succeeded, we need to extract the Output value.
    const auto output_extract = ( {
        auto output_field_name = MakeUnique<IdentifierAst>(PosStart(), "op_as_value");
        auto output_field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(output_field_name));
        auto postfix_output_field = MakeUnique<PostfixExpressionAst>(AstClone(lhs), std::move(output_field));
        auto call_output = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
        call_output->Source.OriginalExpr = this;
        auto postfix_call_output = MakeUnique<PostfixExpressionAst>(std::move(postfix_output_field), std::move(call_output));
        std::move(postfix_call_output);
    });

    output_extract->Stage7_AnalyseSemantics(sm, meta); // Never going to create a scope.
    return output_extract->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Get the left-hand-side information.
    using analyse::utils::type_utils::GetTryType;
    const auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_type = lhs->InferType(sm, meta);

    // Get the Try type's Output generic argument.
    const auto try_type = GetTryType(*lhs_type, *lhs, *sm);
    return try_type->TypeParts().Back()->GnArgGroup->TypeAt("Value")->Val;
}

SPP_MOD_END
