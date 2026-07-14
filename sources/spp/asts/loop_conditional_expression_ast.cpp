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
    // Clone all the members of the ast.
    return MakeUnique<LoopConditionalExpressionAst>(
        AstClone(TokLoop), AstClone(Cond), AstClone(Body), AstClone(ElseBlock));
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
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
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
    if (meta->LoopReturnTypes.contains(meta->LoopCurrentDepth - 1)) {
        _LoopExitTypeInfo = &meta->LoopReturnTypes[meta->LoopCurrentDepth - 1];
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
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Move into the loop scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check twice so that invalidation fails on the second loop.
    // Todo: use the "reset" on "sm" like in TypeStatementAst?
    auto tm = analyse::scopes::ScopeManager(sm->GlobalScope, sm->CurrentScope);
    tm.Reset(sm->CurrentScope, sm->CurrentIterator());

    ValidateSymbolMemory(*Cond, *TokLoop, *sm, true, true, true, true, true, meta);
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
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move into the loop scope.
    sm->MoveToNextScope();

    // Determine if this "case" will be yielding an expression.
    const auto uid = spp::utils::Uid(this);
    const auto is_expr = meta->AssignmentTarget != nullptr;

    // Get the function, and create the basic blocks.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto loop_cond_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.cond" + uid, func);
    const auto loop_body_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.body" + uid, func);
    const auto loop_else_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.else" + uid, func);
    const auto loop_end_bb = llvm::BasicBlock::Create(*ctx->Context, "loop.end" + uid, func);
    ctx->Builder.CreateBr(loop_cond_bb);

    auto phi = static_cast<llvm::PHINode*>(nullptr);
    if (is_expr) {
        ctx->Builder.SetInsertPoint(loop_cond_bb);
        const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(InferType(sm, meta));
        phi = ctx->Builder.CreatePHI(codegen::llvm_type(*ret_type_sym, ctx), 2, "loop.phi" + uid);
    }

    // Jump to the condition entry block.
    meta->Save();
    meta->LlvmEndBB = loop_end_bb;
    meta->LlvmPhi = phi;

    // Generate the initial condition.
    ctx->Builder.SetInsertPoint(loop_cond_bb);
    const auto llvm_cond = Cond->Stage11_CodeGen(sm, meta, ctx);
    ctx->Builder.CreateCondBr(llvm_cond, loop_body_bb, loop_else_bb);

    // Generate the loop body block.
    ctx->Builder.SetInsertPoint(loop_body_bb);
    Body->Stage11_CodeGen(sm, meta, ctx);
    if (ctx->Builder.GetInsertBlock()->getTerminator() == nullptr) {
        ctx->Builder.CreateBr(loop_cond_bb);
    }

    // Generate the else block if it exists.
    ctx->Builder.SetInsertPoint(loop_else_bb);
    if (ElseBlock != nullptr) {
        const auto else_val = ElseBlock->Stage11_CodeGen(sm, meta, ctx);
        const auto else_end_bb = ctx->Builder.GetInsertBlock();
        if (else_end_bb->getTerminator() == nullptr) {
            phi->addIncoming(else_val, else_end_bb);
            ctx->Builder.CreateBr(loop_end_bb);
        }
    }
    else {
        ctx->Builder.CreateBr(loop_end_bb);
    }

    // Finish the loop expression.
    meta->Restore();
    sm->MoveOutOfCurrentScope();
    ctx->Builder.SetInsertPoint(loop_end_bb);
    return phi;
}

auto spp::asts::LoopConditionalExpressionAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    using generate::common_types::NeverType;
    USE_CACHED_TYPE_INFERENCE;

    // A "loop true" with no exit statements returns "Never".
    const auto cond_lit = Cond->To<BooleanLiteralAst>();
    if (cond_lit != nullptr and cond_lit->TokBool->TokenType == lex::SppTokenType::KW_TRUE) {
        if (not _LoopExitTypeInfo) { CACHE_TYPE_INFERENCE_AND_RETURN(NeverType(PosStart())); }
    }

    const auto inferred = LoopExpressionAst::InferType(sm, meta);
    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(inferred));
}

auto spp::asts::LoopConditionalExpressionAst::Terminates() const
    -> bool {
    // The loop conditional expression only terminates if the body terminates.
    return Body->Terminates();
}

SPP_MOD_END
