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
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;


spp::asts::LoopConditionalExpressionAst::LoopConditionalExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(cond) &&cond,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
    cond(std::move(cond)) {
}


spp::asts::LoopConditionalExpressionAst::~LoopConditionalExpressionAst() = default;


auto spp::asts::LoopConditionalExpressionAst::pos_start() const
    -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopConditionalExpressionAst::pos_end() const
    -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::LoopConditionalExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopConditionalExpressionAst>(
        ast_clone(tok_loop),
        ast_clone(cond),
        ast_clone(body),
        ast_clone(else_block));
}


spp::asts::LoopConditionalExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop).append(" ");
    SPP_STRING_APPEND(cond).append(" ");
    SPP_STRING_APPEND(body).append("\n");
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopConditionalExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(body).append("\n");
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
}


auto spp::asts::LoopConditionalExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the loop scope.
    auto scope_name = analyse::scopes::ScopeBlockName("<loop-cond-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Analyse the condition expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Check the loop condition is boolean.
    const auto cond_type = cond->infer_type(sm, meta);
    const auto target_type = generate::common_types_precompiled::BOOL;
    if (not analyse::utils::type_utils::is_type_boolean(*cond_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>()
            .with_args(*cond, *cond_type, "loop")
            .raises_from(sm->current_scope);
    }

    // Set the loop level information into the "meta" object.
    meta->save();
    meta->current_loop_depth += 1;
    meta->current_loop_ast = this;
    body->stage_7_analyse_semantics(sm, meta);
    if (meta->loop_return_types->contains(meta->current_loop_depth - 1)) {
        m_loop_exit_type_info = (*meta->loop_return_types)[meta->current_loop_depth - 1];
    }
    meta->restore();

    // Analyse the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_7_analyse_semantics(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopConditionalExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the loop scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Check twice so that invalidation fails on the second loop.
    // Todo: use the "reset" on "sm" like in TypeStatementAst?
    auto tm = ScopeManager(sm->global_scope, sm->current_scope);
    tm.reset(sm->current_scope, sm->current_iterator());
    for (auto &m : {sm, &tm}) {
        cond->stage_8_check_memory(m, meta);
        body->stage_8_check_memory(m, meta);
    }

    // Check the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_8_check_memory(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopConditionalExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move into the loop scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Determine if this "case" will be yielding an expression.
    const auto is_expr = meta->assignment_target != nullptr;
    const auto func = ctx->builder.GetInsertBlock()->getParent();

    // Get the function, and create the basic blocks.
    const auto loop_cond_entry_bb = llvm::BasicBlock::Create(*ctx->context, "loop.cond.entry", func);
    const auto loop_body_bb = llvm::BasicBlock::Create(*ctx->context, "loop.body", func);
    const auto loop_cond_next_bb = llvm::BasicBlock::Create(*ctx->context, "loop.cond.next", func);
    const auto loop_else_bb = llvm::BasicBlock::Create(*ctx->context, "loop.else", func);
    const auto loop_end_bb = llvm::BasicBlock::Create(*ctx->context, "loop.end", func);

    // Handle the potential PHI node for returning a value out of the loop expression.
    auto result_alloca = static_cast<llvm::Value*>(nullptr);
    auto result_ty = static_cast<llvm::Type*>(nullptr);

    if (is_expr) {
        const auto ret_type_sym = sm->current_scope->get_type_symbol(infer_type(sm, meta));
        result_ty = codegen::llvm_type(*ret_type_sym, ctx);
        result_alloca = ctx->builder.CreateAlloca(result_ty, nullptr, "loop.result.alloca");
    }
    meta->assignment_target = nullptr;
    meta->assignment_target_type = nullptr;

    // Jump to the condition entry block.
    ctx->builder.CreateBr(loop_cond_entry_bb);
    ctx->builder.SetInsertPoint(loop_cond_entry_bb);
    const auto llvm_initial_cond = cond->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateCondBr(llvm_initial_cond, loop_body_bb, loop_else_bb);

    // Generate the loop body block.
    ctx->builder.SetInsertPoint(loop_body_bb);
    body->stage_10_code_gen_2(sm, meta, ctx);
    if (not ctx->builder.GetInsertBlock()->getTerminator()) {
        ctx->builder.CreateBr(loop_cond_next_bb);
    }

    // Generate the loop continuation block.
    ctx->builder.SetInsertPoint(loop_cond_next_bb);
    const auto llvm_cond = llvm_initial_cond;
    ctx->builder.CreateCondBr(llvm_cond, loop_body_bb, loop_end_bb);

    // Generate the else block if it exists.
    ctx->builder.SetInsertPoint(loop_else_bb);
    if (else_block != nullptr) {
        else_block->stage_10_code_gen_2(sm, meta, ctx);
    }
    if (not ctx->builder.GetInsertBlock()->getTerminator()) {
        ctx->builder.CreateBr(loop_end_bb);
    }

    // Finish the loop expression.
    ctx->builder.SetInsertPoint(loop_end_bb);
    sm->move_out_of_current_scope();
    if (is_expr) {
        const auto result_load = ctx->builder.CreateLoad(result_ty, result_alloca, "loop.result.load");
        return result_load;
    }
    return nullptr;
}


auto spp::asts::LoopConditionalExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // If the condition is a boolean literal "true" and no flow control statements exist, return the never type.
    // if (const auto cond_bool_lit = cond->to<BooleanLiteralAst>()) {
    //     if (cond_bool_lit->tok_bool->token_type == lex::SppTokenType::KW_TRUE) {
    //         if (m_loop_exit_type_info.has_value()) {
    //             const auto [exit_expr, _, _] = *m_loop_exit_type_info;
    //             if (exit_expr == nullptr) {
    //                 return generate::common_types::never_type(pos_start());
    //             }
    //         }
    //     }
    // }

    return LoopExpressionAst::infer_type(sm, meta);
}
