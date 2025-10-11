#include <spp/pch.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/iter_expression_ast.hpp>
#include <spp/asts/iter_expression_branch_ast.hpp>
#include <spp/asts/iter_pattern_variant_else_ast.hpp>
#include <spp/asts/iter_pattern_variant_exception_ast.hpp>
#include <spp/asts/iter_pattern_variant_exhausted_ast.hpp>
#include <spp/asts/iter_pattern_variant_no_value_ast.hpp>
#include <spp/asts/iter_pattern_variant_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include <genex/to_container.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/ptr.hpp>


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


auto spp::asts::IterExpressionAst::pos_start() const -> std::size_t {
    return tok_iter->pos_start();
}


auto spp::asts::IterExpressionAst::pos_end() const -> std::size_t {
    return tok_of->pos_end();
}


auto spp::asts::IterExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IterExpressionAst>(
        ast_clone(tok_iter),
        ast_clone(cond),
        ast_clone(tok_of),
        ast_clone_vec(branches));
}


spp::asts::IterExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_iter);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_iter);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}


auto spp::asts::IterExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    ENFORCE_EXPRESSION_SUBTYPE(cond.get());
    cond->stage_7_analyse_semantics(sm, meta);

    // Create the scope for the iteration expression.
    auto scope_name = analyse::scopes::ScopeBlockName("<iter-expr#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Ensure there is only one type of each branch variation.
    if (const auto bs = branches
        | genex::views::ptr
        | genex::views::cast_dynamic<IterPatternVariantExceptionAst*>()
        | genex::to<std::vector>(); bs.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternTypeDuplicateError>().with_args(
            *bs[0], *bs[1]).with_scopes({sm->current_scope}).raise();
    }

    if (const auto bs = branches
        | genex::views::ptr
        | genex::views::cast_dynamic<IterPatternVariantExhaustedAst*>()
        | genex::to<std::vector>(); bs.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternTypeDuplicateError>().with_args(
            *bs[0], *bs[1]).with_scopes({sm->current_scope}).raise();
    }

    if (const auto bs = branches
        | genex::views::ptr
        | genex::views::cast_dynamic<IterPatternVariantNoValueAst*>()
        | genex::to<std::vector>(); bs.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternTypeDuplicateError>().with_args(
            *bs[0], *bs[1]).with_scopes({sm->current_scope}).raise();
    }

    if (const auto bs = branches
        | genex::views::ptr
        | genex::views::cast_dynamic<IterPatternVariantVariableAst*>()
        | genex::to<std::vector>(); bs.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternTypeDuplicateError>().with_args(
            *bs[0], *bs[1]).with_scopes({sm->current_scope}).raise();
    }

    // Check condition + branch compatibility.
    const auto cond_type = cond->infer_type(sm, meta);

    // IterPatternNoValue -> must be a GenOpt condition.
    {
        const auto pat = branches
            | genex::views::ptr
            | genex::views::cast_dynamic<IterPatternVariantNoValueAst*>()
            | genex::to<std::vector>();

        if (not pat.empty() and not analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_OPT, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternIncompatibleError>().with_args(
                *cond, *cond_type, *pat[0], *generate::common_types_precompiled::GEN_OPT).with_scopes({sm->current_scope}).raise();
        }
    }

    // IterPatternException -> Must be a GenRes condition.
    {
        const auto pat = branches
            | genex::views::ptr
            | genex::views::cast_dynamic<IterPatternVariantExceptionAst*>()
            | genex::to<std::vector>();

        if (not pat.empty() and not analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_RES, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternIncompatibleError>().with_args(
                *cond, *cond_type, *pat[0], *generate::common_types_precompiled::GEN_RES).with_scopes({sm->current_scope}).raise();
        }
    }

    // Analyse each branch of the case expression.
    meta->save();
    meta->case_condition = cond.get();
    for (auto const &x: branches) { x->stage_7_analyse_semantics(sm, meta); }
    meta->restore();

    // Exit the iteration expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the condition.
    cond->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*cond, *cond, *sm, true, true, true, false, false, false, meta);

    // Move into the "case" scope and check the memory satus of the symbols in the branches.
    sm->move_to_next_scope();
    analyse::utils::mem_utils::validate_inconsistent_memory(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Move out of the case expression scope.
    sm->move_out_of_current_scope();
}


auto spp::asts::IterExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Ensure consistency across branches.
    auto [master_branch_type_info, branches_type_info] = analyse::utils::type_utils::validate_inconsistent_types(
        branches | genex::views::ptr | genex::to<std::vector>(), sm, meta);

    // Ensure there is a full set of branches for the corresponding generator type (unless there is an "else" present).
    const auto cond_type = cond->infer_type(sm, meta);

    const auto pat_nov_present = genex::algorithms::any_of(branches, [](auto &&x) { return ast_cast<IterPatternVariantNoValueAst>(x.get()) != nullptr; });
    const auto pat_exc_present = genex::algorithms::any_of(branches, [](auto &&x) { return ast_cast<IterPatternVariantExceptionAst>(x.get()) != nullptr; });
    const auto pat_exh_present = genex::algorithms::any_of(branches, [](auto &&x) { return ast_cast<IterPatternVariantExhaustedAst>(x.get()) != nullptr; });
    const auto pat_var_present = genex::algorithms::any_of(branches, [](auto &&x) { return ast_cast<IterPatternVariantVariableAst>(x.get()) != nullptr; });
    const auto pat_else_present = genex::algorithms::any_of(branches, [](auto &&x) { return ast_cast<IterPatternVariantElseAst>(x.get()) != nullptr; });

    if (not meta->ignore_missing_else_branch_for_inference) {
        // The GenOpt type requires "else || (var && nov && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_OPT, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_nov_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>().with_args(
                    *cond, *cond_type).with_scopes({sm->current_scope}).raise();
            }
        }

        // The GenRes type requires "else || (var && exc && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN_RES, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_exc_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>().with_args(
                    *cond, *cond_type).with_scopes({sm->current_scope}).raise();
            }
        }

        // The Gen type requires "else || (var && exh)".
        if (analyse::utils::type_utils::symbolic_eq(*generate::common_types_precompiled::GEN, *cond_type->without_generics(), *sm->current_scope, *sm->current_scope)) {
            if ((not pat_var_present or not pat_exh_present) and not pat_else_present) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppIterExpressionPatternMissingError>().with_args(
                    *cond, *cond_type).with_scopes({sm->current_scope}).raise();
            }
        }
    }

    // Return the branch's return type (there is always 1+ branch).
    return branches[0]->infer_type(sm, meta);
}
