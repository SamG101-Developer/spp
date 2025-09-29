#include <spp/pch.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/assignment_statement_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/address.hpp>
#include <genex/views/enumerate.hpp>
#include <genex/views/indirect.hpp>
#include <genex/views/zip.hpp>


spp::asts::AssignmentStatementAst::AssignmentStatementAst(
    decltype(lhs) &&lhs,
    decltype(tok_assign) &&tok_assign,
    decltype(rhs) &&rhs) :
    lhs(std::move(lhs)),
    tok_assign(std::move(tok_assign)),
    rhs(std::move(rhs)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::AssignmentStatementAst::~AssignmentStatementAst() = default;


auto spp::asts::AssignmentStatementAst::pos_start() const
    -> std::size_t {
    return lhs.front()->pos_start();
}


auto spp::asts::AssignmentStatementAst::pos_end() const
    -> std::size_t {
    return rhs.back()->pos_end();
}


auto spp::asts::AssignmentStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<AssignmentStatementAst>(
        ast_clone_vec(lhs),
        ast_clone(tok_assign),
        ast_clone_vec(rhs));
}


spp::asts::AssignmentStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(lhs);
    raw_string.append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_EXTEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::AssignmentStatementAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(lhs);
    formatted_string.append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_EXTEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::AssignmentStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Ensure the LHS is semantically valid.
    auto is_attr = [](Ast const &x) -> bool { return not ast_cast<IdentifierAst>(&x); };
    for (auto &&lhs_expr : lhs) {
        lhs_expr->stage_7_analyse_semantics(sm, meta);
    }

    // Ensure the RHS is semantically valid.
    for (auto [i, rhs_expr] : rhs | genex::views::transform(&std::unique_ptr<ExpressionAst>::get) | genex::views::enumerate) {
        meta->save();

        // Handle return type overloading matching for the lhs target types.
        if (const auto pf = ast_cast<PostfixExpressionAst>(rhs_expr); pf) {
            if (const auto fc = &ast_cast<PostfixExpressionOperatorFunctionCallAst>(*pf->op); fc) {
                meta->return_type_overload_resolver_type = lhs[i]->infer_type(sm, meta);
            }
        }

        // Analyse the RHS expression.
        meta->assignment_target = ast_clone(ast_cast<IdentifierAst>(lhs[i].get()));
        rhs_expr->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }

    // For each assignment, get the outermost symbol of the expression.
    auto lhs_syms = lhs
        | genex::views::indirect
        | genex::views::transform([sm](auto &&x) { return sm->current_scope->get_var_symbol_outermost(x); })
        | genex::views::to<std::vector>();

    // Create quick access derefs for the looping.
    for (auto &&[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(lhs | genex::views::ptr, rhs | genex::views::ptr, lhs_syms) | genex::views::to<std::vector>()) {
        auto &&[lhs_sym, _] = lhs_sym_and_scope;

        // Full assignment (ie "x" = "y") requires the "x" symbol to be marked as "mut" or never initialized.
        if (not is_attr(*lhs_expr) and not(lhs_sym->is_mutable or lhs_sym->memory_info->initialization_counter == 0)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidMutationError>().with_args(
                *lhs_sym->name, *tok_assign, *lhs_sym->memory_info->ast_initialization).with_scopes({sm->current_scope}).raise();
        }

        // Attribute assignment (ie "x.y = z"), for a non-borrowed symbol, requires an outermost "mut" symbol.
        if (is_attr(*lhs_expr) and not(lhs_sym->memory_info->ast_borrowed or lhs_sym->is_mutable)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidMutationError>().with_args(
                *lhs_sym->name, *tok_assign, *lhs_sym->memory_info->ast_initialization).with_scopes({sm->current_scope}).raise();
        }

        // Attribute assignment (ie "x.y = z"), for a borrowed symbol, cannot contain an immutable borrow.
        if (is_attr(*lhs_expr) and lhs_sym->memory_info->is_borrow_ref) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidMutationError>().with_args(
                *lhs_sym->name, *tok_assign, *lhs_sym->memory_info->ast_borrowed).with_scopes({sm->current_scope}).raise();
        }

        // Ensure the lhs and rhs have the same type.
        auto lhs_type = lhs_expr->infer_type(sm, meta);
        auto rhs_type = rhs_expr->infer_type(sm, meta);
        if (not analyse::utils::type_utils::symbolic_eq(*lhs_type, *rhs_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *lhs_expr, *lhs_type, *rhs_expr, *rhs_type).with_scopes({sm->current_scope}).raise();
        }
    }
}


auto spp::asts::AssignmentStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // For each assignment, check the memory status and resolve any (partial-)moves.
    auto is_attr = [](Ast const &x) -> bool { return not ast_cast<IdentifierAst>(&x); };
    auto lhs_syms = lhs
        | genex::views::indirect
        | genex::views::transform([sm](auto &&x) { return sm->current_scope->get_var_symbol_outermost(x); })
        | genex::views::to<std::vector>();

    for (auto &&[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(lhs | genex::views::ptr, rhs | genex::views::ptr, lhs_syms)) {
        auto &&[lhs_sym, _] = lhs_sym_and_scope;

        // Partially validate the memory of the right-hand-side expression, if it is an attribute being set. Don't mark
        // the move, but do some checks before calling the internal memory checker on the postfix expression.
        analyse::utils::mem_utils::validate_symbol_memory(*rhs_expr, *tok_assign, *sm, is_attr(*lhs_expr), false, true, true, true, false, meta);

        meta->save();
        meta->assignment_target = ast_clone(ast_cast<IdentifierAst>(lhs_expr));
        rhs_expr->stage_8_check_memory(sm, meta);
        meta->restore();

        // Fully validate the memory of the right-hand-side expression, marking the move.
        analyse::utils::mem_utils::validate_symbol_memory(*rhs_expr, *tok_assign, *sm, true, true, true, true, true, true, meta);

        if (is_attr(*lhs_expr)) {
            const auto pf = ast_cast<PostfixExpressionAst>(lhs_expr);
            analyse::utils::mem_utils::validate_symbol_memory(*lhs_expr, *tok_assign, *sm, true, is_attr(*pf->lhs), false, true, true, false, meta);
        }

        // Resolve moved identifiers to the "initialised" state, otherwise resolve a partial move.
        is_attr(*lhs_expr)
            ? lhs_sym->memory_info->initialized_by(*this)
            : lhs_sym->memory_info->remove_partial_move(*lhs_expr);
    }
}
