module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import opex.cast;

SPP_MOD_BEGIN
spp::asts::CaseExpressionAst::CaseExpressionAst(
    decltype(TokCase) &&tok_case,
    decltype(Cond) &&cond,
    decltype(TokOf) &&tok_of,
    decltype(Branches) &&branches) :
    TokCase(std::move(tok_case)),
    Cond(std::move(cond)),
    TokOf(std::move(tok_of)),
    Branches(std::move(branches)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCase, lex::SppTokenType::KW_CASE, "case", cond ? cond->PosStart() : 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokOf, lex::SppTokenType::KW_OF, "of", cond ? cond->PosEnd() : 0);
}

spp::asts::CaseExpressionAst::~CaseExpressionAst() = default;

auto spp::asts::CaseExpressionAst::NewNonPatternMatch(
    decltype(TokCase) &&tok_case,
    decltype(Cond) &&cond,
    Unique<InnerScopeExpressionAst> &&first,
    decltype(Branches) &&branches) -> Unique<CaseExpressionAst> {
    // Convert consecutive if/else-if/else branches into case pattern matching.
    auto patterns = UniqueVec<CasePatternVariantAst>(1);
    patterns[0] = MakeUnique<CasePatternVariantExpressionAst>(BooleanLiteralAst::True(tok_case->PosStart()));
    auto first_branch = MakeUnique<CaseExpressionBranchAst>(nullptr, std::move(patterns), nullptr, std::move(first));
    branches.Insert(branches.begin(), std::move(first_branch));

    // Return the final, newly created, case expression AST.
    auto out = MakeUnique<CaseExpressionAst>(std::move(tok_case), std::move(cond), nullptr, std::move(branches));
    return out;
}

auto spp::asts::CaseExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "case" token.
    return TokCase->PosStart();
}

auto spp::asts::CaseExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the "of" token, or condition.
    return TokOf != nullptr ? TokOf->PosEnd() : Cond->PosEnd();
}

auto spp::asts::CaseExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CaseExpressionAst>(
        AstClone(TokCase), AstClone(Cond), AstClone(TokOf), AstCloneVec(Branches));
}

auto spp::asts::CaseExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokCase).append(" ");
    SPP_STRING_APPEND(Cond).append(" ");
    SPP_STRING_APPEND(TokOf).append(" ");
    SPP_STRING_EXTEND(Branches, "\n");
    SPP_STRING_END;
}

auto spp::asts::CaseExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::errors::SppCaseBranchElseNotLastError;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Create the scope for the case expression.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "case-expr", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    SPP_DEREF_ALLOW_MOVE_HELPER(Cond) {
        meta->Save();
        meta->AllowMoveDeref = true;
        Cond->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }
    else {
        Cond->Stage7_AnalyseSemantics(sm, meta);
    }

    // Analyse the condition expression.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Cond, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Cond));

    // Analyse eac branch of the case expression.
    for (auto const &branch : Branches) {
        // Check the "else" branch is the last branch (also checks there is only 1 "else" branch).
        RaiseIf<SppCaseBranchElseNotLastError>(
            branch->Patterns[0]->To<CasePatternVariantElseAst>() and branch != Branches.Back(),
            {sm->CurrentScope}, ERR_ARGS(*branch, *Branches.Back()));

        // Analyse the branch.
        meta->Save();
        meta->CaseCondition = Cond.get();
        branch->Stage7_AnalyseSemantics(sm, meta);
        meta->Restore();
    }

    // Move out of the case expression scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Alias the common utils functions and types.
    using analyse::utils::mem_utils::ValidateInconsistentMemory;
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->MoveToNextScope();

    // Check the memory state of the condition.
    Cond->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Cond, *Cond, *sm, true, true, false, false, false, meta);

    // Validate the memory state across all branches (also calls stage 8 from within).
    meta->Save();
    meta->CaseCondition = Cond.get();
    ValidateInconsistentMemory(
        this, Branches | genex::views::ptr | genex::to<Vec>(), sm, meta);
    meta->Restore();

    // Move out of the case expression scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Scopes.
    sm->MoveToNextScope();

    // Load meta information for compile-time resolution.
    meta->Save();
    meta->CaseCondition = Cond.get();

    // Delegate to the branches' compile-time resolution (break at first match).
    meta->CmpResult = nullptr;
    for (auto const &branch : Branches) {
        branch->Stage9_CompTimeResolve(sm, meta);
        if (meta->CmpResult != nullptr) { break; }
    }

    // Otherwise, if no branches matched, this is non-returning, so nullptr is fine.
    meta->Restore();
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Scope shift.
    sm->MoveToNextScope();

    // Determine if this "case" will be yielding an expression, and generate the condition.
    const auto uid = spp::utils::Uid(this);
    const auto is_expr = meta->AssignmentTarget != nullptr;
    Cond->Stage11_CodeGen(sm, meta, ctx);

    // Get the function, and create the end basic block.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto case_entry_bb = llvm::BasicBlock::Create(*ctx->Context, "case.entry" + uid, func);
    const auto case_end_bb = llvm::BasicBlock::Create(*ctx->Context, "case.end" + uid, func);
    ctx->Builder.CreateBr(case_entry_bb);

    auto phi = static_cast<llvm::PHINode*>(nullptr);
    auto ret_type = Shared<TypeAst>(nullptr);
    if (is_expr) {
        // The phi merges the value yielded by each branch, so it belongs in the end block (where every
        // branch body branches to), not the entry block.
        ctx->Builder.SetInsertPoint(case_end_bb);
        ret_type = InferType(sm, meta);
        const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ret_type);
        phi = ctx->Builder.CreatePHI(codegen::GetLlvmType(*ret_type_sym, ctx), Branches.Len() as U32, "case.phi" + uid);
    }

    // Set "case" information to the meta struct for branches and patterns to use.
    meta->Save();
    meta->CaseCondition = Cond.get();
    meta->LlvmEndBB = case_end_bb;
    meta->LlvmPhi = phi;

    // Branches need the yielded type, not just the phi's llvm type, so they can wrap a member value into a variant.
    meta->AssignmentTargetType = ret_type;

    // Generate each branch (no return value because phi is modified by the branch).
    ctx->Builder.SetInsertPoint(case_entry_bb);
    for (auto const &branch : Branches) {
        branch->Stage11_CodeGen(sm, meta, ctx);
    }

    // After the last branch, the insert point is the fall-through block reached when no pattern matched; it
    // needs a terminator.
    if (ctx->Builder.GetInsertBlock()->getTerminator() == nullptr) {
        if (is_expr) { ctx->Builder.CreateUnreachable(); }
        else { ctx->Builder.CreateBr(case_end_bb); }
    }

    meta->Restore();
    sm->MoveOutOfCurrentScope();
    ctx->Builder.SetInsertPoint(case_end_bb);
    return phi;
}

auto spp::asts::CaseExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Alias the common utils functions and types.
    using analyse::errors::SppCaseBranchMissingElseError;
    using analyse::utils::type_utils::ValidateInconsistentTypes;
    using generate::common_types::VoidType;

    // Ensure consistency across branches.
    auto [master_branch_type_info, branches_type_info] = ValidateInconsistentTypes(
        Branches | genex::views::ptr | genex::to<Vec>(), sm, meta);

    // Ensure there is an "else" branch if the branches are not exhaustive.
    // Todo: Need to investigate how to detect exhaustion.
    RaiseIf<SppCaseBranchMissingElseError>(
        Branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>() == nullptr and not meta->IgnoreMissingElseBranchForInference,
        {sm->CurrentScope}, ERR_ARGS(*this, *Branches.Back()));

    // Return the branches' return type. If there are any branches, otherwise Void.
    return branches_type_info.IsEmpty() ? VoidType(PosStart()) : master_branch_type_info.Second;
}

auto spp::asts::CaseExpressionAst::Terminates() const
    -> bool {
    // The case expression only terminates if all branches terminate.
    return not genex::any_of(
        Branches, [](auto const &branch) { return not branch->Body->Terminates(); });
}

SPP_MOD_END
