module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_expression_branch_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.binary_expression_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import genex;


spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(op) &&op,
    decltype(patterns) &&patterns,
    decltype(guard) &&guard,
    decltype(body) &&body) :
    op(std::move(op)),
    patterns(std::move(patterns)),
    guard(std::move(guard)),
    body(std::move(body)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->body);
}


spp::asts::CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;


auto spp::asts::CaseExpressionBranchAst::pos_start() const
    -> std::size_t {
    return op ? op->pos_start() : patterns.front()->pos_start();
}


auto spp::asts::CaseExpressionBranchAst::pos_end() const
    -> std::size_t {
    return body->pos_end();
}


auto spp::asts::CaseExpressionBranchAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CaseExpressionBranchAst>(
        ast_clone(op),
        ast_clone_vec(patterns),
        ast_clone(guard),
        ast_clone(body));
}


spp::asts::CaseExpressionBranchAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(op).append(" ");
    SPP_STRING_EXTEND(patterns, ", ").append(" ");
    SPP_STRING_APPEND(guard).append(guard ? " " : "");
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionBranchAst::m_codegen_combine_patterns(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) const
    -> llvm::Value* {
    // If there is only one pattern, generate its condition directly.
    // Otherwise, collect all the pattern conditions and combine them with OR.
    auto llvm_combined_pattern = patterns.front()->stage_11_code_gen_2(sm, meta, ctx);
    for (auto const &pattern : patterns | genex::views::ptr | genex::views::drop(1)) {
        const auto llvm_pattern = pattern->stage_11_code_gen_2(sm, meta, ctx);
        llvm_combined_pattern = ctx->builder.CreateOr(llvm_combined_pattern, llvm_pattern);
    }
    if (guard) {
        const auto llvm_guard = guard->stage_11_code_gen_2(sm, meta, ctx);
        llvm_combined_pattern = ctx->builder.CreateAnd(llvm_combined_pattern, llvm_guard, "case.pattern.guard.match");
    }
    return llvm_combined_pattern;
}


auto spp::asts::CaseExpressionBranchAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    auto scope_name = analyse::scopes::ScopeBlockName::from_parts(
        "case-branch", {}, pos_start());
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse the patterns, ensuring comparison methods exist is needed.
    for (auto const &p : patterns) {
        p->stage_7_analyse_semantics(sm, meta);
    }

    if (op.get() and op->token_type != lex::SppTokenType::KW_IS) {
        for (auto &&p : patterns) {
            // Create a dummy function to check the comparison operator exists.
            const auto pe = p->to<CasePatternVariantExpressionAst>();
            const auto bin_ast = std::make_unique<BinaryExpressionAst>(
                std::make_unique<ObjectInitializerAst>(meta->case_condition->infer_type(sm, meta), nullptr),
                ast_clone(op),
                std::make_unique<ObjectInitializerAst>(pe->expr->infer_type(sm, meta), nullptr));
            bin_ast->stage_7_analyse_semantics(sm, meta);
        }
    }

    // Analyse the guard and body.
    if (guard) {
        guard->stage_7_analyse_semantics(sm, meta);
    }
    body->stage_7_analyse_semantics(sm, meta);

    // Exit the scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the branch's scope.
    sm->move_to_next_scope();

    // Check the patterns, guard and body.
    for (auto &&p : patterns) {
        p->stage_8_check_memory(sm, meta);
    }
    if (guard) {
        guard->stage_8_check_memory(sm, meta);
    }
    body->stage_8_check_memory(sm, meta);

    // Move out of the branch's scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Combine the case expression with the pattern to determine if this branch should be taken, at compile-time.
    sm->move_to_next_scope();
    for (auto const& pattern: patterns) {
        pattern->stage_9_comptime_resolution(sm, meta);

        // Determine if this branch is not a match (false).
        const auto cmp_pat_bool = meta->cmp_result->to<BooleanLiteralAst>();
        if (cmp_pat_bool == nullptr or not cmp_pat_bool->is_true()) {
            sm->exhaust_scope();
            continue;
        }

        // Check with the branch guard if it exists.
        if (guard != nullptr) {
            guard->stage_9_comptime_resolution(sm, meta);
            const auto cmp_guard_bool = meta->cmp_result->to<BooleanLiteralAst>();
            if (not cmp_guard_bool->is_true()) {
                sm->exhaust_scope();
                continue;
            }
        }

        // At this point, the correct branch has been identified, so resolve the body.
        body->stage_9_comptime_resolution(sm, meta);
        sm->move_out_of_current_scope();
        return;
    }

    // None of the patterns on this branch matched.
    meta->cmp_result = nullptr;
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the branch architecture.
    sm->move_to_next_scope();
    const auto uid = spp::utils::generate_uid(this);
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto body_bb = llvm::BasicBlock::Create(*ctx->context, "case.branch.body" + uid, func);
    const auto next_bb = llvm::BasicBlock::Create(*ctx->context, "case.branch.next" + uid, func);

    // Get the condition.
    const auto cond = m_codegen_combine_patterns(sm, meta, ctx);
    ctx->builder.CreateCondBr(cond, body_bb, next_bb);
    ctx->builder.SetInsertPoint(body_bb);

    // Generate the body.
    auto llvm_val = body->stage_11_code_gen_2(sm, meta, ctx);
    const auto incoming_bb = ctx->builder.GetInsertBlock();

    // Sometimes, a type is returned from a branch that is part of the variant type on the lhs. For example, a Opt[T]
    // might receive a Some[T] in one branch, and a None in another. In this case, we need to cast the returned
    // value to the variant type.
    if (meta->assignment_target != nullptr and llvm_val != nullptr) {
        const auto target_llvm_type = meta->llvm_phi->getType();
        const auto casted_val = ctx->builder.CreateBitCast(llvm_val, target_llvm_type, "case.branch.cast.ptr");
        llvm_val = casted_val;
    }

    if (incoming_bb->getTerminator() == nullptr) {
        if (meta->llvm_phi != nullptr) { meta->llvm_phi->addIncoming(llvm_val, incoming_bb); }
        ctx->builder.CreateBr(meta->end_bb);
    }

    // Move out of the branch's scope.
    ctx->builder.SetInsertPoint(next_bb);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::CaseExpressionBranchAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Forward type inference to the body.
    return body->infer_type(sm, meta);
}