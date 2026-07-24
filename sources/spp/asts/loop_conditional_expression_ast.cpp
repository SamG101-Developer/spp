module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_conditional_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::LoopConditionalExpressionAst::LoopConditionalExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Cond) &&cond,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block) :
    LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
    Cond(std::move(cond)) {
}

spp::asts::LoopConditionalExpressionAst::~LoopConditionalExpressionAst() = default;

auto spp::asts::LoopConditionalExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "loop" token.
    return TokLoop->PosStart();
}

auto spp::asts::LoopConditionalExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the condition.
    return Cond->PosEnd();
}

auto spp::asts::LoopConditionalExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast, carrying over the desugaring marker.
    auto cloned = MakeUnique<LoopConditionalExpressionAst>(
        AstClone(TokLoop), AstClone(Cond), AstClone(Body), AstClone(ElseBlock));
    if (_IterDesugar) { cloned->MarkAsIterDesugar(); }
    return cloned;
}

auto spp::asts::LoopConditionalExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokLoop).append(" ");
    SPP_STRING_APPEND(Cond).append(" ");
    SPP_STRING_APPEND(Body).append("\n");
    SPP_STRING_APPEND(ElseBlock);
    SPP_STRING_END;
}

auto spp::asts::LoopConditionalExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppExpressionNotBooleanError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::IsTypeBool;

    // Create the loop scope.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "loop-cond-expr", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    // Analyse the condition expression.
    Cond->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Cond, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Cond));

    // Check the loop condition is boolean.
    const auto cond_type = Cond->InferType(sm, meta);
    RaiseIf<SppExpressionNotBooleanError>(
        not IsTypeBool(*cond_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Cond, *cond_type, "loop"));

    // Set the loop level information into the "meta" object.
    meta->Save();
    meta->LoopCurrentDepth += 1;
    meta->LoopCurrentAst = this;
    Body->Stage7_AnalyseSemantics(sm, meta);
    if (meta->LoopReturnTypes->contains(meta->LoopCurrentDepth - 1)) {
        m_loop_exit_type_info = (*meta->LoopReturnTypes)[meta->LoopCurrentDepth - 1];
    }
    meta->Restore();

    // Analyse the else block if it exists.
    if (ElseBlock != nullptr) {
        ElseBlock->Stage7_AnalyseSemantics(sm, meta);
    }

    // Exit the loop scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::LoopConditionalExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Move into the loop scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check twice so that invalidation fails on the second loop.
    // Todo: use the "reset" on "sm" like in TypeStatementAst?
    auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
    tm.Reset(sm->CurrentScope, sm->CurrentIterator());

    ValidateSymbolMemory(*Cond, *TokLoop, *sm, true, true, true, true, meta);
    for (auto &m : {sm, &tm}) {
        Cond->Stage8_CheckMemory(m, meta);
        Body->Stage8_CheckMemory(m, meta);
    }

    // Check the else block if it exists.
    if (ElseBlock != nullptr) {
        ElseBlock->Stage8_CheckMemory(sm, meta);
    }

    // Exit the loop scope.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::LoopConditionalExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::utils::type_utils::IsTypeNever;
    using analyse::utils::type_utils::IsTypeVoid;

    // Move into the loop scope.
    sm->MoveToNextScope();

    // Determine if this loop will be yielding an expression. A "Void" loop (used as a statement) and a "Never" loop
    // ("loop true" with no "exit"s) have no value to merge, so no phi node.
    const auto uid = "." + spp::utils::Uid(this);
    const auto ret_type = InferType(sm, meta);
    const auto is_expr = meta->AssignmentTarget != nullptr
        and not IsTypeVoid(*ret_type, *sm->CurrentScope)
        and not IsTypeNever(*ret_type, *sm->CurrentScope);

    // Create the key blocks, and as the "else" block is optional, the "not taken" branch either points to the "else"
    // block, or otherwise the end block.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto loop_cond_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.cond" + uid, func);
    const auto loop_body_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.body" + uid, func);
    const auto loop_end_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.end" + uid, func);
    const auto loop_else_bb = ElseBlock != nullptr
        ? llvm::BasicBlock::Create(*ctx->Context, "loop.else" + uid, func)
        : nullptr;
    const auto loop_not_taken_bb = ElseBlock != nullptr
        ? llvm::BasicBlock::Create(*ctx->Context, "loop.not_taken" + uid, func)
        : loop_end_bb;

    // The "else" block runs only when the loop never took an iteration, so track whether the body has been entered.
    auto entered_flag = static_cast<llvm::Value*>(nullptr);
    if (ElseBlock != nullptr) {
        const auto i1_type = llvm::Type::getInt1Ty(*ctx->Context);
        entered_flag = codegen::llvm_entry_alloca(i1_type, "loop.entered" + uid, ctx);
        ctx->Builder.CreateStore(llvm::ConstantInt::getFalse(i1_type), entered_flag);
    }
    ctx->Builder.CreateBr(loop_cond_bb);

    // The phi merges the value yielded by the "else" block with the values yielded by every "exit" statement.
    auto phi = static_cast<llvm::PHINode*>(nullptr);
    if (is_expr) {
        ctx->Builder.SetInsertPoint(loop_end_bb);
        const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ret_type.get());
        phi = ctx->Builder.CreatePHI(codegen::GetLlvmType(*ret_type_sym, ctx), 2U, "loop.phi" + uid);
    }

    // Register this loop so that nested "exit"/"skip" statements can branch to the correct blocks. The stack is
    // ordered outermost-first, so "exit exit" pops 2 frames off the back to find its target.
    meta->Save();
    meta->LlvmEndBB = loop_end_bb;
    meta->LlvmPhi = phi;
    meta->LlvmLoopStack.EmplaceBack(loop_cond_bb, loop_end_bb, phi, entered_flag);
    meta->AssignmentTargetType = ret_type;

    // Generate the condition. This block is branched back to at the end of every iteration, so the condition is
    // re-evaluated each time round.
    ctx->Builder.SetInsertPoint(loop_cond_bb);
    const auto llvm_cond = Cond->Stage11_CodeGen(sm, meta, ctx);
    const auto cond_end_bb = ctx->Builder.GetInsertBlock();
    ctx->Builder.CreateCondBr(llvm_cond, loop_body_bb, loop_not_taken_bb);

    // Generate the loop body block. The body itself never yields the loop's value (only "exit" does), so the
    // assignment target is cleared to stop the final body statement being treated as a yielded value.
    ctx->Builder.SetInsertPoint(loop_body_bb);
    if (entered_flag != nullptr and not _IterDesugar) {
        ctx->Builder.CreateStore(llvm::ConstantInt::getTrue(*ctx->Context), entered_flag);
    }
    meta->Save();
    meta->AssignmentTarget = nullptr;
    meta->LlvmAssignmentTarget = nullptr;
    Body->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    if (ctx->Builder.GetInsertBlock()->getTerminator() == nullptr) {
        ctx->Builder.CreateBr(loop_cond_bb);
    }

    if (ElseBlock != nullptr) {
        // The condition failed: run the else block only if no iteration was ever taken, otherwise leave the loop.
        ctx->Builder.SetInsertPoint(loop_not_taken_bb);
        const auto was_entered = ctx->Builder.CreateLoad(
            llvm::Type::getInt1Ty(*ctx->Context), entered_flag, "loop.was_entered" + uid);
        ctx->Builder.CreateCondBr(was_entered, loop_end_bb, loop_else_bb);
        if (phi != nullptr) { phi->addIncoming(llvm::UndefValue::get(phi->getType()), loop_not_taken_bb); }

        // Generate the else block itself.
        ctx->Builder.SetInsertPoint(loop_else_bb);
        const auto else_val = ElseBlock->Stage11_CodeGen(sm, meta, ctx);
        const auto else_end_bb = ctx->Builder.GetInsertBlock();
        if (else_end_bb->getTerminator() == nullptr) {
            if (phi != nullptr) { phi->addIncoming(else_val, else_end_bb); }
            ctx->Builder.CreateBr(loop_end_bb);
        }
    }
    else {
        // The failing condition targets the end block directly, but the phi still needs a value for it.
        if (phi != nullptr) { phi->addIncoming(llvm::UndefValue::get(phi->getType()), cond_end_bb); }
    }

    // Finish the loop expression.
    meta->Restore();
    sm->MoveOutOfCurrentScope();
    ctx->Builder.SetInsertPoint(loop_end_bb);
    return phi;
}

auto spp::asts::LoopConditionalExpressionAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    //
    using generate::common_types::NeverType;

    // If the condition is a boolean literal "true" and no flow control statements exist, return the never type.
    // if (const auto cond_bool_lit = cond->To<BooleanLiteralAst>()) {
    //     if (cond_bool_lit->tok_bool->token_type == lex::SppTokenType::KW_TRUE) {
    //         if (m_loop_exit_type_info.has_value()) {
    //             const auto [exit_expr, _, _] = *m_loop_exit_type_info;
    //             if (exit_expr == nullptr) {
    //                 return generate::common_types::never_type(PosStart());
    //             }
    //         }
    //     }
    // }

    // A "loop true" with no exit statements returns "Never".
    const auto cond_lit = Cond->To<BooleanLiteralAst>();
    if (cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE) {
        // Check the internal flow controls.
        if (not m_loop_exit_type_info.has_value()) {
            return NeverType(PosStart());
        }
    }

    return LoopExpressionAst::InferType(sm, meta);
}

auto spp::asts::LoopConditionalExpressionAst::MarkAsIterDesugar()
    -> void {
    _IterDesugar = true;
}

auto spp::asts::LoopConditionalExpressionAst::Terminates() const
    -> bool {
    // The loop conditional expression only terminates if the body terminates.
    return Body->Terminates();
}

SPP_MOD_END
