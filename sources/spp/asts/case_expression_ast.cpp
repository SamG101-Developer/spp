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
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

import genex;
import opex.cast;


spp::asts::CaseExpressionAst::CaseExpressionAst(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches) :
    tok_case(std::move(tok_case)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_case, lex::SppTokenType::KW_CASE, "case", cond ? cond->pos_start() : 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_of, lex::SppTokenType::KW_OF, "of", cond ? cond->pos_end() : 0);
}


spp::asts::CaseExpressionAst::~CaseExpressionAst() = default;


auto spp::asts::CaseExpressionAst::new_non_pattern_match(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> &&first,
    decltype(branches) &&branches) -> std::unique_ptr<CaseExpressionAst> {
    // Convert consecutive if/else-if/else branches into case pattern matching.
    auto patterns = std::vector<std::unique_ptr<CasePatternVariantAst>>(1);
    patterns[0] = std::make_unique<CasePatternVariantExpressionAst>(BooleanLiteralAst::True(tok_case->pos_start()));
    auto first_branch = std::make_unique<CaseExpressionBranchAst>(nullptr, std::move(patterns), nullptr, std::move(first));
    branches.insert(branches.begin(), std::move(first_branch));

    // Return the final, newly created, case expression AST.
    auto out = std::make_unique<CaseExpressionAst>(std::move(tok_case), std::move(cond), nullptr, std::move(branches));
    return out;
}


auto spp::asts::CaseExpressionAst::pos_start() const
    -> std::size_t {
    return tok_case->pos_start();
}


auto spp::asts::CaseExpressionAst::pos_end() const
    -> std::size_t {
    return cond->pos_end();
}


auto spp::asts::CaseExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CaseExpressionAst>(
        ast_clone(tok_case),
        ast_clone(cond),
        ast_clone(tok_of),
        ast_clone_vec(branches));
}


spp::asts::CaseExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_case);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_case);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}


auto spp::asts::CaseExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the condition expression.
    SPP_ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Create the scope for the case expression.
    auto scope_name = analyse::scopes::ScopeBlockName("<case-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Analyse eac branch of the case expression.
    for (auto &&branch : branches) {
        // Destructures can only use 1 pattern.
        if (branch->op != nullptr and branch->op->token_type == lex::SppTokenType::KW_IS and branch->patterns.size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppCaseBranchMultipleDestructuresError>()
                .with_args(*branch->patterns[0], *branch->patterns[1])
                .raises_from(sm->current_scope);
        }

        // Check the "else" branch is the last branch (also checks there is only 1 "else" branch).
        if (branch->patterns[0]->to<CasePatternVariantElseAst>() and branch != branches.back()) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppCaseBranchElseNotLastError>()
                .with_args(*branch, *branches.back())
                .raises_from(sm->current_scope);
        }

        // Analyse the branch.
        meta->save();
        meta->case_condition = cond.get();
        branch->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the condition.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *cond, *cond, *sm, true, true, false, false, false, false, meta);

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->move_to_next_scope();
    analyse::utils::mem_utils::validate_inconsistent_memory(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::CaseExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the condition architecture.
    sm->move_to_next_scope();
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto end_bb = llvm::BasicBlock::Create(ctx->context, "case.end", func);
    auto phi = static_cast<llvm::PHINode*>(nullptr);

    // If this expression is being used for assignment, allocate space.
    if (meta->assignment_target != nullptr) {
        const auto ret_type_sym = sm->current_scope->get_type_symbol(infer_type(sm, meta))->llvm_info->llvm_type;
        phi = ctx->builder.CreatePHI(ret_type_sym, branches.size() as U32, "case.phi");
    }

    // Generate each branch.
    meta->save();
    meta->phi_node = phi;
    meta->end_bb = end_bb;
    meta->case_condition = cond.get();
    for (auto &&branch : branches) {
        branch->stage_10_code_gen_2(sm, meta, ctx);
    }
    meta->restore();

    // Finish the case expression.
    ctx->builder.SetInsertPoint(end_bb);
    sm->move_out_of_current_scope();
    return phi;
}


auto spp::asts::CaseExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Ensure consistency across branches.
    auto [master_branch_type_info, branches_type_info] = analyse::utils::type_utils::validate_inconsistent_types(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Ensure there is an "else" branch if the branches are not exhaustive. Todo: Need to investigate how to detect exhaustion.
    if (branches.back()->patterns[0]->to<CasePatternVariantElseAst>() == nullptr and not meta->ignore_missing_else_branch_for_inference) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppCaseBranchMissingElseError>()
            .with_args(*this, *branches.back())
            .raises_from(sm->current_scope);
    }

    // Return the branches' return type. If there are any branches, otherwise Void.
    return branches_type_info.empty()
               ? generate::common_types::void_type(pos_start())
               : std::get<1>(master_branch_type_info);
}
