module;
#include <spp/macros.hpp>

module spp.asts.case_expression_branch_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(Op) &&op,
    decltype(Patterns) &&patterns,
    decltype(Guard) &&guard,
    decltype(Body) &&body) :
    Op(std::move(op)),
    Patterns(std::move(patterns)),
    Guard(std::move(guard)),
    Body(std::move(body)) {
    // Default the body to empty.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Body);
}

spp::asts::CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;

auto spp::asts::CaseExpressionBranchAst::PosStart() const
    -> std::size_t {
    // Use the op or first pattern
    return Op ? Op->PosStart() : Patterns.Front()->PosStart();
}

auto spp::asts::CaseExpressionBranchAst::PosEnd() const
    -> std::size_t {
    // // Use the final pattern.
    return Patterns.Back()->PosEnd();
}

auto spp::asts::CaseExpressionBranchAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast, carrying over the desugaring marker.
    auto cloned = MakeUnique<CaseExpressionBranchAst>(
        AstClone(Op), AstCloneVec(Patterns), AstClone(Guard), AstClone(Body));
    cloned->_ForIterLoopYield = _ForIterLoopYield;
    return cloned;
}

auto spp::asts::CaseExpressionBranchAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Op).append(" ");
    SPP_STRING_EXTEND(Patterns, ", ");
    SPP_STRING_APPEND_RAW(" ");
    SPP_STRING_APPEND(Guard).append(Guard ? " " : "");
    SPP_STRING_APPEND(Body);
    SPP_STRING_END;
}

auto spp::asts::CaseExpressionBranchAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a scope for the branch - this is where destructures of patterns will reside.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "case-branch", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);

    // Analyse the patterns, ensuring comparison methods exist is needed.
    for (auto const &p : Patterns) {
        p->Stage7_AnalyseSemantics(sm, meta);
    }

    // Ensure the functions exist for the comparisons (whichever op is used except "is").
    if (Op.get() and Op->TokenType != lex::SppTokenType::KW_IS) {
        for (auto const &p : Patterns) {
            const auto pe = p->To<CasePatternVariantExpressionAst>();
            const auto bin_ast = MakeUnique<BinaryExpressionAst>(
                MakeUnique<ObjectInitializerAst>(meta->CaseCondition->InferType(sm, meta), nullptr),
                AstClone(Op),
                MakeUnique<ObjectInitializerAst>(pe->Expr->InferType(sm, meta), nullptr));
            bin_ast->Stage7_AnalyseSemantics(sm, meta);
        }
    }

    // Analyse the guard and body.
    if (Guard) { Guard->Stage7_AnalyseSemantics(sm, meta); }
    Body->Stage7_AnalyseSemantics(sm, meta);

    // Exit the scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the branch's scope.
    sm->MoveToNextScope();

    // Check the patterns, guard and body.
    for (auto const &p : Patterns) { p->Stage8_CheckMemory(sm, meta); }
    if (Guard) { Guard->Stage8_CheckMemory(sm, meta); }
    Body->Stage8_CheckMemory(sm, meta);

    // Move out of the branch's scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Combine the case expression with the pattern to determine if this branch should be taken, at compile-time.
    sm->MoveToNextScope();
    for (auto const &pattern : Patterns) {
        pattern->Stage9_CompTimeResolve(sm, meta);

        // Determine if this branch is not a match (false).
        const auto cmp_pat_bool = meta->CmpResult ? meta->CmpResult->To<BooleanLiteralAst>() : nullptr;
        if (cmp_pat_bool == nullptr or not cmp_pat_bool->IsTrue()) {
            sm->ExhaustScope();
            continue;
        }

        // Check with the branch guard if it exists.
        if (Guard != nullptr) {
            Guard->Stage9_CompTimeResolve(sm, meta);
            const auto cmp_guard_bool = meta->CmpResult ? meta->CmpResult->To<BooleanLiteralAst>() : nullptr;
            if (not cmp_guard_bool or not cmp_guard_bool->IsTrue()) {
                sm->ExhaustScope();
                continue;
            }
        }

        // At this point, the correct branch has been identified, so resolve the body.
        Body->Stage9_CompTimeResolve(sm, meta);
        sm->MoveOutOfCurrentScope();
        return;
    }

    // None of the patterns on this branch matched.
    meta->CmpResult = nullptr;
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::CaseExpressionBranchAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the branch architecture.
    sm->MoveToNextScope();
    const auto uid = spp::utils::Uid(this);
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto body_bb = llvm::BasicBlock::Create(*ctx->Context, "case.branch.body" + uid, func);
    const auto next_bb = llvm::BasicBlock::Create(*ctx->Context, "case.branch.next" + uid, func);

    // Get the condition.
    const auto cond = _CodegenCombinePatterns(sm, meta, ctx);
    ctx->Builder.CreateCondBr(cond, body_bb, next_bb);
    ctx->Builder.SetInsertPoint(body_bb);

    // For a desugared iterable loop, reaching this branch means the generator yielded, so the enclosing loop counts as
    // having been entered and its "else" block must not run.
    if (_ForIterLoopYield and not meta->LlvmLoopStack.IsEmpty()) {
        if (const auto entered_flag = meta->LlvmLoopStack.Back().EnteredFlag; entered_flag != nullptr) {
            ctx->Builder.CreateStore(llvm::ConstantInt::getTrue(*ctx->Context), entered_flag);
        }
    }

    // Generate the body.
    auto llvm_val = Body->Stage11_CodeGen(sm, meta, ctx);
    const auto incoming_bb = ctx->Builder.GetInsertBlock();

    // Sometimes, a type is returned from a branch that is part of the variant type on the lhs. For example, a Opt[T]
    // might receive a Some[T] in one branch, and a None in another. In this case, we need to cast the returned
    // value to the variant type.
    if (meta->AssignmentTarget != nullptr and llvm_val != nullptr) {
        const auto target_llvm_type = meta->LlvmPhi->getType();
        const auto casted_val = ctx->Builder.CreateBitCast(llvm_val, target_llvm_type, "case.branch.cast.ptr");
        llvm_val = casted_val;
    }

    if (incoming_bb->getTerminator() == nullptr) {
        if (meta->LlvmPhi != nullptr) { meta->LlvmPhi->addIncoming(llvm_val, incoming_bb); }
        ctx->Builder.CreateBr(meta->LlvmEndBB);
    }

    // Move out of the branch's scope.
    ctx->Builder.SetInsertPoint(next_bb);
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::CaseExpressionBranchAst::MarkForIterLoopYield()
    -> void {
    _ForIterLoopYield = true;
}

auto spp::asts::CaseExpressionBranchAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // Forward type inference to the body.
    return Body->InferType(sm, meta);
}

auto spp::asts::CaseExpressionBranchAst::_CodegenCombinePatterns(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {
    // If there is only one pattern, generate its condition directly.
    // Otherwise, collect all the pattern conditions and combine them with OR.
    auto llvm_combined_pattern = Patterns.Front()->Stage11_CodeGen(sm, meta, ctx);
    for (auto const &pattern : Patterns | genex::views::ptr | genex::views::drop(1)) {
        const auto llvm_pattern = pattern->Stage11_CodeGen(sm, meta, ctx);
        llvm_combined_pattern = ctx->Builder.CreateOr(llvm_combined_pattern, llvm_pattern);
    }
    if (Guard) {
        const auto llvm_guard = Guard->Stage11_CodeGen(sm, meta, ctx);
        llvm_combined_pattern = ctx->Builder.CreateAnd(llvm_combined_pattern, llvm_guard, "case.pattern.guard.match");
    }
    return llvm_combined_pattern;
}

SPP_MOD_END
