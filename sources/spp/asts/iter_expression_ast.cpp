module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.iter_expression_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
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
import genex;


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
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_iter).append(" ");
    SPP_PRINT_APPEND(cond).append(" ");
    SPP_PRINT_APPEND(tok_of).append(" ");
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}


auto spp::asts::IterExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    SPP_ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Create the scope for the iteration expression.
    auto scope_name = analyse::scopes::ScopeBlockName("<iter-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

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
    analyse::utils::mem_utils::validate_inconsistent_memory(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
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
