module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.iter_expression_ast;
import spp.asts.iter_expression_branch_ast;
import spp.asts.iter_pattern_variant_else_ast;
import spp.asts.iter_pattern_variant_variable_ast;
import spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.loop_condition_ast;
import spp.asts.loop_condition_boolean_ast;
import spp.asts.loop_condition_iterable_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import llvm;


spp::asts::LoopExpressionAst::LoopExpressionAst(
    decltype(tok_loop) &&tok_loop,
    decltype(cond) &&cond,
    decltype(body) &&body,
    decltype(else_block) &&else_block) :
    PrimaryExpressionAst(),
    tok_loop(std::move(tok_loop)),
    cond(std::move(cond)),
    body(std::move(body)),
    else_block(std::move(else_block)) {
}


spp::asts::LoopExpressionAst::~LoopExpressionAst() = default;


auto spp::asts::LoopExpressionAst::pos_start() const
    -> std::size_t {
    return tok_loop->pos_start();
}


auto spp::asts::LoopExpressionAst::pos_end() const
    -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::LoopExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LoopExpressionAst>(
        ast_clone(tok_loop),
        ast_clone(cond),
        ast_clone(body),
        ast_clone(else_block));
}


spp::asts::LoopExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_loop).append(" ");
    SPP_STRING_APPEND(cond).append(" ");
    SPP_STRING_APPEND(body).append("\n");
    SPP_STRING_APPEND(else_block);
    SPP_STRING_END;
}


auto spp::asts::LoopExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_loop).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(body).append("\n");
    SPP_PRINT_APPEND(else_block);
    SPP_PRINT_END;
}


auto spp::asts::LoopExpressionAst::m_codegen_condition_bool(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Determine if this "case" will be yielding an expression.
    const auto is_expr = meta->assignment_target != nullptr;

    // Get the function, and create the basic blocks.
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto loop_cond_entry_bb = llvm::BasicBlock::Create(*ctx->context, "loop.cond.entry", func);
    const auto loop_body_bb = llvm::BasicBlock::Create(*ctx->context, "loop.body", func);
    const auto loop_cond_next_bb = llvm::BasicBlock::Create(*ctx->context, "loop.cond.next", func);
    const auto loop_else_bb = llvm::BasicBlock::Create(*ctx->context, "loop.else", func);
    const auto loop_end_bb = llvm::BasicBlock::Create(*ctx->context, "loop.end", func);

    // Handle the potential PHI node for returning a value out of the loop expression.
    auto phi = static_cast<llvm::PHINode*>(nullptr);
    if (is_expr) {
        const auto ret_type_sym = sm->current_scope->get_type_symbol(infer_type(sm, meta))->llvm_info->llvm_type;
        phi = ctx->builder.CreatePHI(ret_type_sym, 2, "loop.phi");
    }

    // Jump to the condition entry block.
    ctx->builder.CreateBr(loop_cond_entry_bb);
    ctx->builder.SetInsertPoint(loop_cond_entry_bb);
    const auto llvm_initial_cond = cond->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateCondBr(llvm_initial_cond, loop_body_bb, loop_else_bb);

    // Generate the loop body block.
    ctx->builder.SetInsertPoint(loop_body_bb);
    const auto llvm_body_val = body->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateBr(loop_cond_next_bb);

    // Generate the loop continuation block.
    ctx->builder.SetInsertPoint(loop_cond_next_bb);
    const auto llvm_cond = cond->stage_10_code_gen_2(sm, meta, ctx);
    ctx->builder.CreateCondBr(llvm_cond, loop_body_bb, loop_end_bb);

    // Generate the else block if it exists.
    ctx->builder.SetInsertPoint(loop_else_bb);
    auto llvm_else_val = static_cast<llvm::Value*>(nullptr);
    if (else_block != nullptr) {
        llvm_else_val = else_block->stage_10_code_gen_2(sm, meta, ctx);
    }
    ctx->builder.CreateBr(loop_end_bb);

    // Finish the loop expression.
    ctx->builder.SetInsertPoint(loop_end_bb);
    if (is_expr) {
        phi->addIncoming(llvm_body_val, loop_body_bb);
        if (else_block != nullptr) {
            phi->addIncoming(llvm_else_val, loop_else_bb);
        }
    }

    return phi;
}


auto spp::asts::LoopExpressionAst::m_codegen_condition_iter(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    /*
     * Iteration looping is syntactic sugar. The translation looks like this:
     *
     *      loop [x, y] in z.iter_ref() { body } else { other }
     *
     * becomes
     *
     *      let __iter = z.iter_ref()
     *      let __first = true
     *      loop true {
     *          let __res = __iter.res()
     *          iter __res {
     *              [x, y] { body }  # variable binding
     *
     *              !! && first {
     *                  other
     *                  exit
     *              }  # first iteration and exhausted -> run else block
     *
     *              !! { exit }  # exhausted -> break from the loop
     *
     *              else { skip }  # continue the loop for opt/res types
     *          }
     *      }
     *
     */

    // Simple statements to move from.
    auto skip_stmt = LoopControlFlowStatementAst::Skip(pos_start());
    auto exit_stmt = LoopControlFlowStatementAst::Exit(pos_start());
    auto iter_cond = cond->to<LoopConditionIterableAst>();

    // Create the initial let statement to materialize the condition being iterated.
    auto iterable_name = std::make_shared<IdentifierAst>(pos_start(), std::format("$_iter_{}", reinterpret_cast<std::uintptr_t>(this)));
    auto iterable_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, iterable_name, nullptr);
    auto iterable_val = std::move(iter_cond->iterable);
    auto iterable_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));

    // Create the "let" statement that increments the iterator.
    auto resume_name = std::make_shared<IdentifierAst>(pos_start(), std::format("$_res_{}", reinterpret_cast<std::uintptr_t>(this)));
    auto resume_var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, resume_name, nullptr);
    auto resume_val_op = std::make_unique<PostfixExpressionOperatorKeywordResAst>(nullptr, nullptr, nullptr);
    auto resume_val = std::make_unique<PostfixExpressionAst>(ast_clone(iterable_name), std::move(resume_val_op));
    auto resume_let = std::make_unique<LetStatementInitializedAst>(nullptr, std::move(resume_var), nullptr, nullptr, std::move(resume_val));

    // Iter:Else => skip over GenOpt no-value and GenRes error yields.
    auto iter_else_pattern = std::make_unique<IterPatternVariantElseAst>(nullptr);
    auto iter_else_body = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();
    iter_else_body->members.emplace_back(std::move(skip_stmt));
    auto iter_else_branch = std::make_unique<IterExpressionBranchAst>(std::move(iter_else_pattern), std::move(iter_else_body), nullptr);

    // Iter:Exhausted AND first => run the else block once, then exit the loop.
    // TODO

    // Iter:Exhausted => exit the loop, as iteration has finished.
    auto iter_exhausted_pattern = std::make_unique<IterPatternVariantExhaustedAst>(nullptr);
    auto iter_exhausted_body = InnerScopeExpressionAst<std::unique_ptr<StatementAst>>::new_empty();
    iter_exhausted_body->members.emplace_back(std::move(exit_stmt));
    auto iter_exhausted_branch = std::make_unique<IterExpressionBranchAst>(std::move(iter_exhausted_pattern), std::move(iter_exhausted_body), nullptr);

    // Iter:Variable => bind to the same variable as in the original loop.
    auto iter_var_pattern = std::make_unique<IterPatternVariantVariableAst>(std::move(iter_cond->var));
    auto iter_var_body = std::move(body);
    auto iter_var_branch = std::make_unique<IterExpressionBranchAst>(std::move(iter_var_pattern), std::move(iter_var_body), nullptr);

    // Iter block to handle the resume value.
    auto iter_branches = std::vector<std::unique_ptr<IterExpressionBranchAst>>();
    iter_branches.emplace_back(std::move(iter_var_branch));
    iter_branches.emplace_back(std::move(iter_exhausted_branch));
    iter_branches.emplace_back(std::move(iter_else_branch));
    auto iter_expr = std::make_unique<IterExpressionAst>(nullptr, ast_clone(resume_name), nullptr, std::move(iter_branches));

    // New boolean loop that manually iterates the iterable.
    auto loop_cond_new = std::make_unique<LoopConditionBooleanAst>(BooleanLiteralAst::True(pos_start()));
    auto loop_new = std::make_unique<LoopExpressionAst>(nullptr, std::move(loop_cond_new), nullptr, nullptr);
    loop_new->body->members.emplace_back(std::move(resume_let));
    loop_new->body->members.emplace_back(std::move(iter_expr));

    // Generate the code for the iterable let statement, then the new loop.
    iterable_let->stage_10_code_gen_2(sm, meta, ctx);
    return loop_new->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::LoopExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the loop scope.
    auto scope_name = analyse::scopes::ScopeBlockName("<loop-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    cond->stage_7_analyse_semantics(sm, meta);

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


auto spp::asts::LoopExpressionAst::stage_8_check_memory(
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
        meta->save();
        meta->loop_double_check_active = m == &tm;
        cond->stage_8_check_memory(m, meta);
        meta->restore();

        body->stage_8_check_memory(m, meta);
    }

    // Check the else block if it exists.
    if (else_block != nullptr) {
        else_block->stage_8_check_memory(sm, meta);
    }

    // Exit the loop scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::LoopExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code based on the condition type.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    const auto val = cond->to<LoopConditionBooleanAst>()
                         ? m_codegen_condition_bool(sm, meta, ctx)
                         : m_codegen_condition_iter(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return val;
}


auto spp::asts::LoopExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the loop's exit type (or Void if there are no exits fro inside the loop).
    auto [exit_expr, loop_type, _] = m_loop_exit_type_info.has_value()
                                         ? *m_loop_exit_type_info
                                         : std::make_tuple(nullptr, generate::common_types::void_type(pos_start()), nullptr);
    exit_expr = exit_expr ? exit_expr : this;

    // Check the else block's type is the same as the loop exit type.
    if (else_block != nullptr and not meta->ignore_missing_else_branch_for_inference) {
        const auto else_type = else_block->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*loop_type, *else_type, *sm->current_scope, *sm->current_scope)) {
            const auto final_member = else_block->body->final_member();
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                .with_args(*exit_expr, *loop_type, *final_member, *else_type)
                .raises_from(sm->current_scope);
        }
    }

    // If the return type is "Void", but the condition is comptime "true", then the loop is infinite.
    // Todo: change this to count the number of "exit" statements, as "exit" (void) is possible.
    if (analyse::utils::type_utils::symbolic_eq(*loop_type, *generate::common_types_precompiled::VOID, *sm->current_scope, *sm->current_scope)) {
        if (cond->to<LoopConditionBooleanAst>() != nullptr) {
            // Non-symbolic expressions, or compile time symbolic boolean expressions, can never change (infinite loop)
            const auto is_symbolic = cond->to<IdentifierAst>();
            if (not is_symbolic or sm->current_scope->get_var_symbol(is_symbolic->shared_from_this())->memory_info->ast_comptime != nullptr) {
                loop_type = generate::common_types::never_type(pos_start());
            }
        }
    }

    // Return the loop type.
    return loop_type;
}
