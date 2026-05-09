module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.assignment_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.assignment_utils;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::AssignmentStatementAst::AssignmentStatementAst(
    decltype(Lhs) &&lhs,
    decltype(TokAssign) &&tok_assign,
    decltype(Rhs) &&rhs) :
    Lhs(std::move(lhs)),
    TokAssign(std::move(tok_assign)),
    Rhs(std::move(rhs)) {
    // Default the assignment token.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

spp::asts::AssignmentStatementAst::~AssignmentStatementAst() = default;

auto spp::asts::AssignmentStatementAst::PosStart() const
    -> std::size_t {
    // Use the leftmost assignment target.
    return Lhs.Front()->PosStart();
}

auto spp::asts::AssignmentStatementAst::PosEnd() const
    -> std::size_t {
    // Use the rightmost assignment value.
    return Rhs.Back()->PosEnd();
}

auto spp::asts::AssignmentStatementAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<AssignmentStatementAst>(
        AstCloneVec(Lhs), AstClone(TokAssign), AstCloneVec(Rhs));
}

auto spp::asts::AssignmentStatementAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Lhs, ", ").append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_EXTEND(Rhs, ", ");
    SPP_STRING_END;
}

auto spp::asts::AssignmentStatementAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::errors::SppInvalidMutationError;
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::assignment_utils::IsAttr;
    using analyse::utils::assignment_utils::IsIdentifier;
    using analyse::utils::type_utils::TypeEq;

    // Ensure the LHS is semantically valid.
    for (auto const &lhs_expr : Lhs) {
        SPP_DEREF_ALLOW_MOVE_HELPER(lhs_expr) {
            meta->Save();
            meta->AllowMoveDeref = true;
            lhs_expr->Stage7_AnalyseSemantics(sm, meta);
            meta->Restore();
        }
        else {
            lhs_expr->Stage7_AnalyseSemantics(sm, meta);
        }
    }

    // Ensure the RHS is semantically valid.
    for (auto [i, rhs_expr] : Rhs | genex::views::ptr | genex::views::enumerate) {
        meta->Save();

        // Handle return type overloading matching for the lhs target types.
        if (const auto pf = rhs_expr->To<PostfixExpressionAst>(); pf != nullptr) {
            if (const auto fc = pf->Op->To<PostfixExpressionOperatorFunctionCallAst>(); fc != nullptr) {
                meta->ReturnTypeOverloadResolverType = Lhs[i]->InferType(sm, meta);
            }
        }

        // Analyse the RHS expression.
        meta->AssignmentTarget = AstCloneShared(Lhs[i]->To<IdentifierAst>());
        meta->AssignmentTargetType = Lhs[i]->InferType(sm, meta);
        rhs_expr->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }

    // For each assignment, get the outermost symbol of the expression.
    auto lhs_syms = Lhs
        | genex::views::transform([sm](auto const &x) { return sm->CurrentScope->GetVarSymbolOutermost(*x); });

    // Create quick access derefs for the looping.
    for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(Lhs | genex::views::ptr, Rhs | genex::views::ptr, lhs_syms)) {
        auto const &[lhs_sym, _] = lhs_sym_and_scope;

        // Full assignment (ie "x" = "y") requires the "x" symbol to be marked as "mut" or never initialized.
        RaiseIf<SppInvalidMutationError>(
            IsIdentifier(lhs_expr)
            and not(lhs_sym->IsMutable or lhs_sym->MemInfo->InitializationCounter == 0),
            {sm->CurrentScope}, ERR_ARGS(*lhs_sym->Name, *TokAssign, *std::get<0>(lhs_sym->MemInfo->AstInitialization)));

        // Attribute assignment (ie "x.y = z"), for a non-borrowed symbol, requires an outermost "mut" symbol.
        RaiseIf<SppInvalidMutationError>(
            IsAttr(lhs_expr, sm)
            and not(std::get<0>(lhs_sym->MemInfo->AstBorrowed) or lhs_sym->IsMutable),
            {sm->CurrentScope}, ERR_ARGS(*lhs_sym->Name, *TokAssign, *std::get<0>(lhs_sym->MemInfo->AstInitialization)));

        // Attribute assignment (ie "x.y = z"), for a borrowed symbol, cannot be immutably borrowed.
        RaiseIf<SppInvalidMutationError>(
            IsAttr(lhs_expr, sm)
            and lhs_sym->Type->GetConvention()
            and *lhs_sym->Type->GetConvention() == ConventionTag::REF,
            {sm->CurrentScope}, ERR_ARGS(*lhs_sym->Name, *TokAssign, *std::get<0>(lhs_sym->MemInfo->AstInitialization)));

        // Prevent double initializations to immutable uninitialized let statements.
        if (IsIdentifier(lhs_expr)) {
            lhs_sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
        }

        // Ensure the lhs and rhs have the same type.
        auto lhs_type = lhs_expr->InferType(sm, meta);
        auto rhs_type = rhs_expr->InferType(sm, meta);
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*lhs_type, *rhs_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*lhs_expr, *lhs_type, *rhs_expr, *rhs_type));
    }
}

auto spp::asts::AssignmentStatementAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::assignment_utils::IsAttr;
    using analyse::utils::assignment_utils::IsIdentifier;
    using analyse::utils::mem_utils::PreventBorrowLifetimeExtension;
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // For each assignment, check the memory status and resolve any (partial-)moves.
    auto lhs_syms = Lhs
        | genex::views::transform([sm](auto const &x) { return sm->CurrentScope->GetVarSymbolOutermost(*x); })
        | genex::to<Vec>();

    for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(Lhs | genex::views::ptr, Rhs | genex::views::ptr, lhs_syms)) {
        auto const &[lhs_sym, _] = lhs_sym_and_scope;

        // Partially validate the memory of the right-hand-side expression, if it is an attribute being set. Don't mark
        // the move, but do some checks before calling the internal memory checker on the postfix expression.
        ValidateSymbolMemory(
            *rhs_expr, *TokAssign, *sm,
            IsAttr(lhs_expr, sm), false, true, true, false, meta);

        meta->Save();
        meta->AssignmentTarget = AstCloneShared(lhs_expr->To<IdentifierAst>());
        meta->AssignmentTargetType = lhs_expr->InferType(sm, meta);
        rhs_expr->Stage8_CheckMemory(sm, meta);
        meta->Restore();

        // Fully validate the memory of the right-hand-side expression, marking the move.
        ValidateSymbolMemory(
            *rhs_expr, *TokAssign, *sm, true, true, true, true, true, meta);

        if (IsAttr(lhs_expr, sm)) {
            const auto pf = lhs_expr->To<PostfixExpressionAst>();
            const auto check_partial_move = IsAttr(pf->Lhs.get(), sm);
            ValidateSymbolMemory(
                *lhs_expr, *TokAssign, *sm, true, check_partial_move, false, true, false, meta);
        }

        // Resolve moved identifiers to the "initialised" state, otherwise resolve a partial move.
        if (IsAttr(lhs_expr, sm)) {
            lhs_sym->MemInfo->RemovePartialMoves(*lhs_expr, sm->CurrentScope);
        }
        else if (IsIdentifier(lhs_expr)) {
            lhs_sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
        }

        // Ensure a borrow is not increasing its lifetime.
        const auto lhs_outermost = sm->CurrentScope->GetVarSymbolOutermost(*lhs_expr).First;
        const auto rhs_outermost = sm->CurrentScope->GetVarSymbolOutermost(*rhs_expr).First;
        PreventBorrowLifetimeExtension(
            lhs_outermost.get(), rhs_outermost.get(), this, *sm);
    }
}

auto spp::asts::AssignmentStatementAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::assignment_utils::IsAttr;
    using analyse::utils::assignment_utils::IsIdentifier;
    using analyse::utils::cmp_utils::SetCompTimeAttrValue;

    // Wrap the rhs value and move it into the value of the variable symbol.
    for (auto i = 0uz; i < Lhs.Len(); ++i) {
        Rhs[i]->Stage9_CompTimeResolve(sm, meta);
        const auto lhs_sym = sm->CurrentScope->GetVarSymbolOutermost(*Lhs[i]).First;

        // Assign to a full identifier.
        if (IsIdentifier(Lhs[i].get())) {
            lhs_sym->CompTimeValue = std::move(meta->CmpResult);
        }

        // Assign to an attribute.
        else if (IsAttr(Lhs[i].get(), sm)) {
            SetCompTimeAttrValue(
                lhs_sym->CompTimeValue->To<ObjectInitializerAst>(),
                Lhs[i].get(), std::move(meta->CmpResult), sm);
        }

        // Otherwise, unsupported in the comptime context.
        else {
            Raise<analyse::errors::SppInvalidComptimeOperationError>(
                {sm->CurrentScope}, ERR_ARGS(*Lhs[i]));
        }
    }
}

auto spp::asts::AssignmentStatementAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for each assignment in sequence.
    for (auto i = 0uz; i < Lhs.Len(); ++i) {
        // Set the meta information for generating with values.
        meta->Save();
        meta->AssignmentTarget = AstCloneShared(Lhs[i]->To<IdentifierAst>());
        meta->AssignmentTargetType = Lhs[i]->InferType(sm, meta);

        // Generate the RHS value.
        const auto llvm_rhs = Rhs[i]->Stage11_CodeGen(sm, meta, ctx);
        meta->Restore();

        // Generate the LHS location and store the RHS value into it.
        const auto llvm_lhs = Lhs[i]->Stage11_CodeGen(sm, meta, ctx);
        ctx->Builder.CreateStore(llvm_rhs, llvm_lhs);
    }

    // Statements are always generated into a builder so no need to return anything.
    return nullptr;
}

SPP_MOD_END
