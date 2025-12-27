module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.iter_expression_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.iter_expression_branch_ast;
import spp.asts.iter_pattern_variant_ast;
import spp.asts.iter_pattern_variant_else_ast;
import spp.asts.iter_pattern_variant_exception_ast;
import spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.iter_pattern_variant_no_value_ast;
import spp.asts.iter_pattern_variant_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;
import opex.cast;


spp::asts::IterExpressionAst::IterExpressionAst(
    decltype(tok_iter) &&tok_iter,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches) :
    PrimaryExpressionAst(),
    tok_iter(std::move(tok_iter)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_iter, lex::SppTokenType::KW_ITER, "iter", cond ? cond->pos_start() : 0);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_of, lex::SppTokenType::KW_OF, "of", cond ? cond->pos_end() : 0);
}


spp::asts::IterExpressionAst::~IterExpressionAst() = default;


auto spp::asts::IterExpressionAst::pos_start() const
    -> std::size_t {
    return tok_iter->pos_start();
}


auto spp::asts::IterExpressionAst::pos_end() const
    -> std::size_t {
    return tok_of->pos_end();
}


auto spp::asts::IterExpressionAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterExpressionAst>(
        ast_clone(tok_iter),
        ast_clone(cond),
        ast_clone(tok_of),
        ast_clone_vec(branches));
}


spp::asts::IterExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_iter).append(" ");
    SPP_STRING_APPEND(cond).append(" ");
    SPP_STRING_APPEND(tok_of).append(" ");
    SPP_STRING_EXTEND(branches, "\n");
    SPP_STRING_END;
}


auto spp::asts::IterExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_iter).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(tok_of).append(" ");
    SPP_PRINT_EXTEND(branches, "\n");
    SPP_PRINT_END;
}


auto spp::asts::IterExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    SPP_ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Todo: Check that the condition is a generated type.

    // Create the scope for the iteration expression.
    auto scope_name = analyse::scopes::ScopeBlockName("<iter-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    const auto pat_nop = branches
        | genex::views::transform([](auto const &x) { return x->pattern.get(); })
        | genex::views::cast_dynamic<IterPatternVariantNoValueAst*>()
        | genex::views::cast_dynamic<IterPatternVariantAst*>()
        | genex::to<std::vector>();

    const auto pat_err = branches
        | genex::views::transform([](auto const &x) { return x->pattern.get(); })
        | genex::views::cast_dynamic<IterPatternVariantExceptionAst*>()
        | genex::views::cast_dynamic<IterPatternVariantAst*>()
        | genex::to<std::vector>();

    const auto pat_exh = branches
        | genex::views::transform([](auto const &x) { return x->pattern.get(); })
        | genex::views::cast_dynamic<IterPatternVariantExhaustedAst*>()
        | genex::views::cast_dynamic<IterPatternVariantAst*>()
        | genex::to<std::vector>();

    const auto pat_var = branches
        | genex::views::transform([](auto const &x) { return x->pattern.get(); })
        | genex::views::cast_dynamic<IterPatternVariantVariableAst*>()
        | genex::views::cast_dynamic<IterPatternVariantAst*>()
        | genex::to<std::vector>();

    // Ensure there is only one type of each branch variation.
    for (auto const &pat_set : std::vector{&pat_nop, &pat_err, &pat_exh, &pat_var}) {
        if (pat_set->size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternTypeDuplicateError>()
                .with_args(*(*pat_set)[0], *(*pat_set)[1])
                .raises_from(sm->current_scope);
        }
    }

    // Check condition + branch compatibility.
    const auto cond_type = cond->infer_type(sm, meta);

    // IterPatternNoValue -> must be a "GenOpt" condition.
    if (not pat_nop.empty() and not analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GENERATED_OPT, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternIncompatibleError>()
            .with_args(*cond, *cond_type, *pat_nop[0], *generate::common_types_precompiled::GEN_OPT)
            .raises_from(sm->current_scope);
    }

    // IterPatternException -> Must be a "GenRes" condition.
    if (not pat_err.empty() and not analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GENERATED_RES, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternIncompatibleError>()
            .with_args(*cond, *cond_type, *pat_err[0], *generate::common_types_precompiled::GEN_RES)
            .raises_from(sm->current_scope);
    }

    // Analyse each branch of the case expression.
    meta->save();
    meta->case_condition = cond.get();
    for (auto const &x : branches) { x->stage_7_analyse_semantics(sm, meta); }
    meta->restore();

    // Exit the iteration expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the condition.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *cond, *cond, *sm, true, true, false, false, false, false, meta);

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    analyse::utils::mem_utils::validate_inconsistent_memory(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    const auto is_expr = meta->assignment_target != nullptr;
    cond->stage_10_code_gen_2(sm, meta, ctx);

    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);

    // Branching mechanism like "case" but instead of value matching, it's pattern matching on the generator state.
    // 1=>variable, 2=>exhausted, 3=>no value, 4=>exception
    // Also, each branch may have a guard on, so logical AND that with the pattern match.
    const auto func = ctx->builder.GetInsertBlock()->getParent();
    const auto iter_end_bb = llvm::BasicBlock::Create(*ctx->context, "iter.end", func);

    // Handle the potential PHI node for returning a value out of the case expression.
    auto result_alloca = static_cast<llvm::Value*>(nullptr);
    auto result_ty = static_cast<llvm::Type*>(nullptr);
    if (is_expr) {
        const auto ret_type_sym = sm->current_scope->get_type_symbol(infer_type(sm, meta));
        result_ty = codegen::llvm_type(*ret_type_sym, ctx);
        result_alloca = ctx->builder.CreateAlloca(result_ty, nullptr, "iter.result.alloca");
    }
    meta->assignment_target = nullptr;
    meta->assignment_target_type = nullptr;

    // Set "iter" information to the meta struct for branches and patterns to use.
    meta->save();
    meta->case_condition = cond.get();
    meta->end_bb = iter_end_bb;

    // Generate the case entry block for the branches to generate bodies into.
    const auto case_entry_bb = llvm::BasicBlock::Create(*ctx->context, "iter.entry", func);
    ctx->builder.CreateBr(case_entry_bb);
    ctx->builder.SetInsertPoint(case_entry_bb);

    // Generate each branch.
    for (auto &&branch : branches) {
        branch->stage_10_code_gen_2(sm, meta, ctx);
    }

    // Finish the case expression.
    meta->restore();
    ctx->builder.SetInsertPoint(iter_end_bb);
    sm->move_out_of_current_scope();

    if (is_expr) {
        const auto result_load = ctx->builder.CreateLoad(result_ty, result_alloca, "iter.result.load");
        return result_load;
    }
    return nullptr;
}


auto spp::asts::IterExpressionAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Ensure consistency across branches.
    auto [master_branch_type_info, branches_type_info] = analyse::utils::type_utils::validate_inconsistent_types(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Ensure there is a full set of branches for the corresponding generator type (unless there is an "else" present).
    const auto cond_type = cond->infer_type(sm, meta);

    const auto pat_nop_present = genex::any_of(
        branches, [](auto const &x) { return x->pattern->template to<IterPatternVariantNoValueAst>() != nullptr; });
    const auto pat_err_present = genex::any_of(
        branches, [](auto const &x) { return x->pattern->template to<IterPatternVariantExceptionAst>() != nullptr; });
    const auto pat_exh_present = genex::any_of(
        branches, [](auto const &x) { return x->pattern->template to<IterPatternVariantExhaustedAst>() != nullptr; });
    const auto pat_var_present = genex::any_of(
        branches, [](auto const &x) { return x->pattern->template to<IterPatternVariantVariableAst>() != nullptr; });
    const auto pat_else_present = genex::any_of(
        branches, [](auto const &x) { return x->pattern->template to<IterPatternVariantElseAst>() != nullptr; });

    if (not meta->ignore_missing_else_branch_for_inference) {
        // The GenOpt type requires "else || (var && nov && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GENERATED_OPT, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_nop_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>()
                    .with_args(*cond, *cond_type)
                    .raises_from(sm->current_scope);
            }
        }

        // The GenRes type requires "else || (var && exc && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GENERATED_RES, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_err_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>()
                    .with_args(*cond, *cond_type)
                    .raises_from(sm->current_scope);
            }
        }

        // The Gen type requires "else || (var && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GENERATED, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>()
                    .with_args(*cond, *cond_type)
                    .raises_from(sm->current_scope);
            }
        }
    }

    // Return the branch's return type (there is always 1+ branch).
    return branches_type_info.empty()
               ? generate::common_types::void_type(pos_start())
               : std::get<1>(master_branch_type_info);
}
