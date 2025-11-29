module;
#include <spp/macros.hpp>

module spp.asts.case_expression_branch_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.asts.binary_expression_ast;
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
    SPP_STRING_APPEND(op);
    SPP_STRING_EXTEND(patterns);
    SPP_STRING_APPEND(guard);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionBranchAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_EXTEND(patterns);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}


auto spp::asts::CaseExpressionBranchAst::m_codegen_combine_patterns(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // If there is only one pattern, generate its condition directly.
    // Otherwise, collect all the pattern conditions and combine them with OR.
    auto combined_cond = patterns.front()->stage_10_code_gen_2(sm, meta, ctx);
    for (auto const &pattern : patterns | genex::views::ptr | genex::views::drop(1)) {
        const auto pattern_cond = pattern->stage_10_code_gen_2(sm, meta, ctx);
        combined_cond = ctx->builder.CreateOr(combined_cond, pattern_cond);
    }
    if (guard) {
        const auto guard_cond = guard->stage_10_code_gen_2(sm, meta, ctx);
        combined_cond = ctx->builder.CreateAnd(combined_cond, guard_cond);
    }
    return combined_cond;
}


auto spp::asts::CaseExpressionBranchAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    auto scope_name = analyse::scopes::ScopeBlockName("<case-pattern#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse the patterns, ensuring comparison methods exist is needed.
    patterns | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });

    if (op.get() and op->token_type != lex::SppTokenType::KW_IS) {
        for (auto &&p : patterns) {
            // Create a dummy function to check the comparison operator exists.
            const auto pe = ast_cast<CasePatternVariantExpressionAst>(p.get());
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
    patterns | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
    if (guard) {
        guard->stage_8_check_memory(sm, meta);
    }
    body->stage_8_check_memory(sm, meta);

    // Move out of the branch's scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionBranchAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the branch architecture.
    sm->move_to_next_scope();
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto branch_bb = llvm::BasicBlock::Create(ctx->context, "case.branch", func);

    // Get the condition.
    const auto match_cond = m_codegen_combine_patterns(sm, meta, ctx);
    const auto next_bb = llvm::BasicBlock::Create(ctx->context, "case.next", func);
    ctx->builder.CreateCondBr(match_cond, branch_bb, next_bb);
    ctx->builder.SetInsertPoint(branch_bb);

    // Generate the body.
    const auto branch_val = body->stage_10_code_gen_2(sm, meta, ctx);
    meta->phi_node->addIncoming(branch_val, branch_bb);
    ctx->builder.CreateBr(meta->end_bb);
    ctx->builder.SetInsertPoint(next_bb);

    // Move out of the branch's scope.
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
