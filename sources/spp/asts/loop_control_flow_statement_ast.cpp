module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_control_flow_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.loop_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.lex.tokens;
import genex;
import llvm;

SPP_MOD_BEGIN

spp::asts::LoopControlFlowStatementAst::LoopControlFlowStatementAst(
    decltype(TokSeqExit) &&tok_seq_exit,
    decltype(TokSkip) &&tok_skip,
    decltype(Expr) &&expr) :
    TokSeqExit(std::move(tok_seq_exit)),
    TokSkip(std::move(tok_skip)),
    Expr(std::move(expr)) {
}

spp::asts::LoopControlFlowStatementAst::~LoopControlFlowStatementAst() = default;

auto spp::asts::LoopControlFlowStatementAst::PosStart() const
    -> std::size_t {
    // Use the first "exit" or "skip".
    return TokSeqExit.IsEmpty() ? TokSkip->PosStart() : TokSeqExit.Front()->PosStart();
}

auto spp::asts::LoopControlFlowStatementAst::PosEnd() const
    -> std::size_t {
    // Use expression or "skip" or last "exit".
    return Expr ? Expr->PosEnd() : TokSkip ? TokSkip->PosEnd() : TokSeqExit.Back()->PosEnd();
}

auto spp::asts::LoopControlFlowStatementAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<LoopControlFlowStatementAst>(
        AstCloneVec(TokSeqExit), AstClone(TokSkip), AstClone(Expr));
}

auto spp::asts::LoopControlFlowStatementAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(TokSeqExit, " ").append(" ");
    SPP_STRING_APPEND(TokSkip).append(" ");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::LoopControlFlowStatementAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppLoopTooManyControlFlowStatementsError;
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types::VoidType;

    // Get the number of control flow statements, and the loop's nesting level.
    const auto has_skip = TokSkip != nullptr;
    const auto num_controls = TokSeqExit.Len() + (has_skip ? 1 : 0);
    const auto nested_loop_depth = meta->LoopCurrentDepth;

    // Check the depth of the loop is greater than or equal to the number of control statements.
    RaiseIf<SppLoopTooManyControlFlowStatementsError>(
        num_controls > nested_loop_depth, {sm->CurrentScope},
        ERR_ARGS(*meta->LoopCurrentAst->TokLoop, *this, num_controls, nested_loop_depth));

    // Save and compare the loop's "exiting" type against other nested loop's exit statement types.
    if (not has_skip) {
        auto expr_type = VoidType(PosStart());

        // Analyse the expression if it is present.
        if (Expr != nullptr) {
            Expr->Stage7_AnalyseSemantics(sm, meta);
            RaiseIf<SppInvalidPrimaryExpressionError>(
                Expr and not IsPrimaryExprTypeValid(*Expr, *sm),
                {sm->CurrentScope}, ERR_ARGS(*Expr.get()));

            expr_type = Expr->InferType(sm, meta);
        }

        // Insert or check the depth's corresponding exit type.
        const auto depth = nested_loop_depth - num_controls;
        if (meta->LoopReturnTypes->contains(depth)) {
            // If the type is already set, check it matches the current expression's type.
            auto [that_expr, that_expr_type, that_scope] = meta->LoopReturnTypes->at(depth);
            RaiseIf<SppTypeMismatchError>(
                not TypeEq(*expr_type, *that_expr_type, *sm->CurrentScope, *that_scope),
                {sm->CurrentScope, that_scope}, ERR_ARGS(*Expr, *expr_type, *that_expr, *that_expr_type));
        }
        else {
            auto stored_expr = Expr ? Expr.get() : nullptr;
            (*meta->LoopReturnTypes)[depth] = std::make_tuple(stored_expr, expr_type, sm->CurrentScope);
        }
    }
}

auto spp::asts::LoopControlFlowStatementAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    if (Expr == nullptr) { return; }

    // Check the memory state of the expression if it is present. Expression is being moved into outer context, so
    // strict memory checks.
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *TokSeqExit.Back(), *sm, true, true, true, true, true, meta);
}

auto spp::asts::LoopControlFlowStatementAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    //
    using analyse::errors::SppInternalCompilerError;

    // The loop stack is ordered outermost-first, so the innermost enclosing loop is the back element.
    const auto num_exits = TokSeqExit.Len();
    const auto has_skip = TokSkip != nullptr;
    const auto num_loops = meta->LlvmLoopStack.Len();

    // Generate the attached expression (if present) before the branch, so the value is available to the phi node.
    const auto llvm_val = Expr != nullptr ? Expr->Stage11_CodeGen(sm, meta, ctx) : nullptr;

    // For "skip" statements, branch to the condition check of the target loop, starting the next iteration. Any
    // preceding "exit" tokens select an outer loop. A "skip" never has a value, so no phi node is involved.
    if (has_skip) {
        const auto &target = meta->LlvmLoopStack[num_loops - num_exits - 1];
        ctx->Builder.CreateBr(target.CondBB);
        return nullptr;
    }

    // For "exit" statements, branch to the end block of the Nth innermost loop, N being the number of exit tokens.
    // The exited loop's phi node collects the yielded value from this edge.
    const auto &target = meta->LlvmLoopStack[num_loops - num_exits];
    if (target.Phi != nullptr) {
        const auto incoming_val = llvm_val;
        const auto incoming_bb = ctx->Builder.GetInsertBlock();
        target.Phi->addIncoming(incoming_val != nullptr ? incoming_val : llvm::UndefValue::get(target.Phi->getType()), incoming_bb);
    }
    ctx->Builder.CreateBr(target.EndBB);
    return nullptr;
}

auto spp::asts::LoopControlFlowStatementAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst> {
    // If there is an attached expression, return its type, otherwise Void.
    using generate::common_types::VoidType;
    return Expr != nullptr ? Expr->InferType(sm, meta) : VoidType(PosStart());
}

SPP_MOD_END
