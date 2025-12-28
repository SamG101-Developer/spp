module;
#include <spp/macros.hpp>

module spp.asts.iter_expression_branch_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.inner_scope_expression_ast;
import spp.asts.iter_pattern_variant_ast;
import spp.asts.iter_pattern_variant_else_ast;
import spp.asts.iter_pattern_variant_exception_ast;
import spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.iter_pattern_variant_no_value_ast;
import spp.asts.iter_pattern_variant_variable_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.utils.uid;


spp::asts::IterExpressionBranchAst::IterExpressionBranchAst(
    decltype(pattern) &&pattern,
    decltype(guard) &&guard,
    decltype(body) &&body) :
    pattern(std::move(pattern)),
    guard(std::move(guard)),
    body(std::move(body)) {
}


spp::asts::IterExpressionBranchAst::~IterExpressionBranchAst() = default;


auto spp::asts::IterExpressionBranchAst::pos_start() const
    -> std::size_t {
    return pattern->pos_start();
}


auto spp::asts::IterExpressionBranchAst::pos_end() const
    -> std::size_t {
    return body->pos_end();
}


auto spp::asts::IterExpressionBranchAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterExpressionBranchAst>(
        ast_clone(pattern),
        ast_clone(guard),
        ast_clone(body));
}


spp::asts::IterExpressionBranchAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(pattern);
    SPP_STRING_APPEND(body);
    SPP_STRING_APPEND(guard);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionBranchAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(pattern);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_END;
}


auto spp::asts::IterExpressionBranchAst::m_codegen_combine_patterns(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {
    // Based on the environment generator state, generate a u8 equality.
    // If there is a guard, generate it and logical AND it with the pattern match.
    auto gen_env = meta->case_condition->stage_10_code_gen_2(sm, meta, ctx);
    const auto uid = spp::utils::generate_uid(this);
    const auto gen_type = llvm::PointerType::get(*ctx->context, 0);
    const auto yield_ptr = ctx->builder.CreateStructGEP(gen_type, gen_env, 0, "gen.yield.state" + uid);
    const auto yield_val = ctx->builder.CreateLoad(llvm::Type::getInt8Ty(*ctx->context), yield_ptr, "gen.state.val" + uid);
    auto llvm_combined_pattern = static_cast<llvm::Value*>(nullptr);

    // Based on the pattern, determine the match condition.
    pattern->stage_10_code_gen_2(sm, meta, ctx);
    if (pattern->to<IterPatternVariantVariableAst>() != nullptr) {
        constexpr auto state = static_cast<std::uint8_t>(codegen::CoroutineState::VARIABLE);
        const auto constant = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), state);
        llvm_combined_pattern = ctx->builder.CreateICmpEQ(yield_val, constant, "iter.pattern.match" + uid);
    }
    else if (pattern->to<IterPatternVariantExhaustedAst>() != nullptr) {
        constexpr auto state = static_cast<std::uint8_t>(codegen::CoroutineState::EXHAUSTED);
        const auto constant = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), state);
        llvm_combined_pattern = ctx->builder.CreateICmpEQ(yield_val, constant, "iter.pattern.match" + uid);
    }
    else if (pattern->to<IterPatternVariantNoValueAst>() != nullptr) {
        constexpr auto state = static_cast<std::uint8_t>(codegen::CoroutineState::NO_VALUE);
        const auto constant = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), state);
        llvm_combined_pattern = ctx->builder.CreateICmpEQ(yield_val, constant, "iter.pattern.match" + uid);
    }
    else if (pattern->to<IterPatternVariantExceptionAst>() != nullptr) {
        constexpr auto state = static_cast<std::uint8_t>(codegen::CoroutineState::EXCEPTION);
        const auto constant = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->context), state);
        llvm_combined_pattern = ctx->builder.CreateICmpEQ(yield_val, constant, "iter.pattern.match" + uid);
    }
    else if (pattern->to<IterPatternVariantElseAst>() != nullptr) {
        // Always matches.
        llvm_combined_pattern = llvm::ConstantInt::getTrue(*ctx->context);
    }

    // If there is a guard, combine it with the pattern match.
    if (guard) {
        const auto llvm_guard = guard->stage_10_code_gen_2(sm, meta, ctx);
        return ctx->builder.CreateAnd(llvm_combined_pattern, llvm_guard, "iter.pattern.guard.match" + uid);
    }

    return llvm_combined_pattern;
}


auto spp::asts::IterExpressionBranchAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the scope for the iteration branch.
    auto scope_name = analyse::scopes::ScopeBlockName("<iter-branch#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse the pattern, guard and body.
    pattern->stage_7_analyse_semantics(sm, meta);
    if (guard) { guard->stage_7_analyse_semantics(sm, meta); }
    body->stage_7_analyse_semantics(sm, meta);

    // Exit the scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionBranchAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the branch's scope.
    sm->move_to_next_scope();

    // Check the patterns, guard and body.
    pattern->stage_8_check_memory(sm, meta);
    if (guard) { guard->stage_8_check_memory(sm, meta); }
    body->stage_8_check_memory(sm, meta);

    // Move out of the branch's scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionBranchAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the branch architecture.
    sm->move_to_next_scope();
    const auto uid = spp::utils::generate_uid(this);
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto body_bb = llvm::BasicBlock::Create(*ctx->context, "iter.branch.body" + uid, func);
    const auto next_bb = llvm::BasicBlock::Create(*ctx->context, "iter.branch.next" + uid, func);

    // Get the condition.
    const auto cond = m_codegen_combine_patterns(sm, meta, ctx);
    ctx->builder.CreateCondBr(cond, body_bb, next_bb);
    ctx->builder.SetInsertPoint(body_bb);

    // Generate the body.
    const auto llvm_val = body->stage_10_code_gen_2(sm, meta, ctx);
    const auto incoming_bb = ctx->builder.GetInsertBlock();

    if (incoming_bb->getTerminator() == nullptr) {
        if (meta->llvm_phi != nullptr) { meta->llvm_phi->addIncoming(llvm_val, incoming_bb); }
        ctx->builder.CreateBr(meta->end_bb);
    }

    // Move out of the branch's scope.
    ctx->builder.SetInsertPoint(next_bb);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::IterExpressionBranchAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // The type of the branch is the type of the body.
    return body->infer_type(sm, meta);
}
