module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.assignment_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.obj_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


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
    SPP_STRING_EXTEND(lhs, ", ").append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_EXTEND(rhs, ", ");
    SPP_STRING_END;
}


auto spp::asts::AssignmentStatementAst::is_identifier(
    Ast const *x) -> bool {
    // Determine if the AST node is an identifier.
    return x->to<IdentifierAst>() != nullptr;
}


auto spp::asts::AssignmentStatementAst::is_attr(
    Ast const *x,
    analyse::scopes::ScopeManager const *sm) -> bool {
    // Determine if the AST node is an attribute (ie not an identifier).
    return not x->to<IdentifierAst>() and sm->current_scope->get_var_symbol_outermost(*x).first != nullptr;
}


auto spp::asts::AssignmentStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure the LHS is semantically valid.
    for (auto &&lhs_expr : lhs) {
        SPP_DEREF_ALLOW_MOVE_HELPER(lhs_expr) {
            meta->save();
            meta->allow_move_deref = true;
            lhs_expr->stage_7_analyse_semantics(sm, meta);
            meta->restore();
        }
        else {
            lhs_expr->stage_7_analyse_semantics(sm, meta);
        }
    }

    // Ensure the RHS is semantically valid.
    for (auto [i, rhs_expr] : rhs | genex::views::ptr | genex::views::enumerate) {
        meta->save();

        // Handle return type overloading matching for the lhs target types.
        if (const auto pf = rhs_expr->to<PostfixExpressionAst>(); pf != nullptr) {
            if (const auto fc = pf->op->to<PostfixExpressionOperatorFunctionCallAst>(); fc != nullptr) {
                meta->return_type_overload_resolver_type = lhs[i]->infer_type(sm, meta);
            }
        }

        // Analyse the RHS expression.
        meta->assignment_target = ast_clone(lhs[i]->to<IdentifierAst>());
        meta->assignment_target_type = lhs[i]->infer_type(sm, meta);
        rhs_expr->stage_7_analyse_semantics(sm, meta);
        meta->restore();
    }

    // For each assignment, get the outermost symbol of the expression.
    auto lhs_syms = lhs
        | genex::views::transform([sm](auto const &x) { return sm->current_scope->get_var_symbol_outermost(*x); });

    // Create quick access derefs for the looping.
    for (auto &&[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(lhs | genex::views::ptr, rhs | genex::views::ptr, lhs_syms)) {
        auto &&[lhs_sym, _] = lhs_sym_and_scope;

        // Full assignment (ie "x" = "y") requires the "x" symbol to be marked as "mut" or never initialized.
        raise_if<analyse::errors::SppInvalidMutationError>(
            is_identifier(lhs_expr) and not(lhs_sym->is_mutable or lhs_sym->memory_info->initialization_counter == 0),
            {sm->current_scope}, ERR_ARGS(*lhs_sym->name, *tok_assign, *std::get<0>(lhs_sym->memory_info->ast_initialization)));

        // Attribute assignment (ie "x.y = z"), for a non-borrowed symbol, requires an outermost "mut" symbol.
        raise_if<analyse::errors::SppInvalidMutationError>(
            is_attr(lhs_expr, sm) and not(std::get<0>(lhs_sym->memory_info->ast_borrowed) or lhs_sym->is_mutable),
            {sm->current_scope}, ERR_ARGS(*lhs_sym->name, *tok_assign, *std::get<0>(lhs_sym->memory_info->ast_initialization)));

        // Attribute assignment (ie "x.y = z"), for a borrowed symbol, cannot be immutably borrowed.
        raise_if<analyse::errors::SppInvalidMutationError>(
            is_attr(lhs_expr, sm) and lhs_sym->type->get_convention() and *lhs_sym->type->get_convention() == ConventionTag::REF,
            {sm->current_scope}, ERR_ARGS(*lhs_sym->name, *tok_assign, *std::get<0>(lhs_sym->memory_info->ast_borrowed)));

        // Prevent double initializations to immutable uninitialized let statements.
        if (is_identifier(lhs_expr)) {
            lhs_sym->memory_info->initialized_by(*this, sm->current_scope);
        }

        // Ensure the lhs and rhs have the same type.
        auto lhs_type = lhs_expr->infer_type(sm, meta);
        auto rhs_type = rhs_expr->infer_type(sm, meta);
        raise_if<analyse::errors::SppTypeMismatchError>(
            not analyse::utils::type_utils::symbolic_eq(*lhs_type, *rhs_type, *sm->current_scope, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*lhs_expr, *lhs_type, *rhs_expr, *rhs_type));
    }
}


auto spp::asts::AssignmentStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // For each assignment, check the memory status and resolve any (partial-)moves.
    auto lhs_syms = lhs
        | genex::views::transform([sm](auto &&x) { return sm->current_scope->get_var_symbol_outermost(*x); })
        | genex::to<std::vector>();

    for (auto &&[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(lhs | genex::views::ptr, rhs | genex::views::ptr, lhs_syms)) {
        auto &&[lhs_sym, _] = lhs_sym_and_scope;

        // Partially validate the memory of the right-hand-side expression, if it is an attribute being set. Don't mark
        // the move, but do some checks before calling the internal memory checker on the postfix expression.
        analyse::utils::mem_utils::validate_symbol_memory(
            *rhs_expr, *tok_assign, *sm, is_attr(lhs_expr, sm), false, true, true, true, false, meta);

        meta->save();
        meta->assignment_target = ast_clone(lhs_expr->to<IdentifierAst>());
        meta->assignment_target_type = lhs_expr->infer_type(sm, meta);
        rhs_expr->stage_8_check_memory(sm, meta);
        meta->restore();

        // Fully validate the memory of the right-hand-side expression, marking the move.
        analyse::utils::mem_utils::validate_symbol_memory(
            *rhs_expr, *tok_assign, *sm, true, true, true, true, true, true, meta);

        if (is_attr(lhs_expr, sm)) {
            const auto pf = lhs_expr->to<PostfixExpressionAst>();
            analyse::utils::mem_utils::validate_symbol_memory(
                *lhs_expr, *tok_assign, *sm, true, is_attr(pf->lhs.get(), sm), false, true, true, false, meta);
        }

        // Resolve moved identifiers to the "initialised" state, otherwise resolve a partial move.
        if (is_attr(lhs_expr, sm)) {
            lhs_sym->memory_info->remove_partial_move(*lhs_expr, sm->current_scope);
        }
        else if (is_identifier(lhs_expr)) {
            lhs_sym->memory_info->initialized_by(*this, sm->current_scope);
        }

        // Ensure a borrow is not increasing its lifetime.
        const auto lhs_outermost = sm->current_scope->get_var_symbol_outermost(*lhs_expr).first;
        const auto rhs_outermost = sm->current_scope->get_var_symbol_outermost(*rhs_expr).first;
        const auto is_rhs_borrow = rhs_outermost and std::get<0>(rhs_outermost->memory_info->ast_borrowed) != nullptr;
        if (lhs_outermost != nullptr and rhs_outermost != nullptr and is_rhs_borrow) {
            const auto rhs_borrow_scope = std::get<1>(rhs_outermost->memory_info->ast_borrowed);
            const auto lhs_init_scope = ( {
                auto ret_scope = static_cast<analyse::scopes::Scope const*>(nullptr);
                for (auto const &ancestor : rhs_borrow_scope->ancestors()) {
                    if (not ancestor->has_var_symbol(lhs_outermost->name, true)) { continue; }
                    ret_scope = ancestor;
                    break;
                }
                ret_scope;
            });
            if (lhs_init_scope != nullptr) {
                const auto scope_depth_difference = genex::position(lhs_init_scope->ancestors(), genex::operations::eq_fixed{rhs_borrow_scope});
                raise_if<analyse::errors::SppBorrowLifetimeIncreaseError>(
                    scope_depth_difference < 0, {sm->current_scope},
                    ERR_ARGS(*this, *lhs_outermost->name, *std::get<0>(rhs_outermost->memory_info->ast_borrowed)));
            }
        }
    }
}


auto spp::asts::AssignmentStatementAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Wrap the rhs value and move it into the value of the variable symbol.
    for (auto i = 0uz; i < lhs.size(); ++i) {
        rhs[i]->stage_9_comptime_resolution(sm, meta);
        const auto lhs_sym = sm->current_scope->get_var_symbol_outermost(*lhs[i]).first;

        // Assign to a full identifier.
        if (is_identifier(lhs[i].get())) {
            lhs_sym->comptime_value = std::move(meta->cmp_result);
        }

        // Assign to an attribute.
        else if (is_attr(lhs[i].get(), sm)) {
            analyse::utils::obj_utils::set_attribute_value(
                lhs_sym->comptime_value->to<ObjectInitializerAst>(), lhs[i].get(), std::move(meta->cmp_result), sm);
        }

        // Otherwise, unsupported in the comptime context.
        else {
            raise<analyse::errors::SppInvalidComptimeOperationError>(
                {sm->current_scope}, ERR_ARGS(*lhs[i]));
        }
    }
}


auto spp::asts::AssignmentStatementAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for each assignment in sequence.
    for (auto i = 0uz; i < lhs.size(); ++i) {
        // Set the meta information for generating with values.
        meta->save();
        meta->assignment_target = ast_clone(lhs[i]->to<IdentifierAst>());
        meta->assignment_target_type = lhs[i]->infer_type(sm, meta);

        // Generate the RHS value.
        const auto llvm_rhs = rhs[i]->stage_11_code_gen_2(sm, meta, ctx);
        meta->restore();

        // Generate the LHS location and store the RHS value into it.
        const auto llvm_lhs = lhs[i]->stage_11_code_gen_2(sm, meta, ctx);
        ctx->builder.CreateStore(llvm_rhs, llvm_lhs);
    }

    // Statements are always generated into a builder so no need to return anything.
    return nullptr;
}