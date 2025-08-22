#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/utils/strings.hpp>

#include <genex/algorithms/minmax.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/map.hpp>
#include <genex/views/to.hpp>


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(tok_dot) &&tok_dot,
    decltype(name) &&name) :
    tok_dot(std::move(tok_dot)),
    name(std::move(name)) {
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_start() const -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
        ast_clone(tok_dot),
        ast_clone(name));
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Prevent types on the left-hand-side of a runtime member access.
    if (ast_cast<TypeAst>(meta->postfix_expression_lhs)) {
        analyse::errors::SppMemberAccessStaticOperatorExpectedError(*meta->postfix_expression_lhs, *tok_dot).scopes({sm->current_scope}).raise();
    }

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(*lhs_type);

        // Check the left-hand-side isn't a generic type.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SppGenericTypeInvalidUsageError(*meta->postfix_expression_lhs, *lhs_type, "member access").scopes({sm->current_scope}).raise();
        }

        // Check the lhs is a tuple/array (the only indexable types).
        if (not analyse::utils::type_utils::is_type_indexable(*lhs_type, *sm->current_scope)) {
            analyse::errors::SppMemberAccessNonIndexableError(*meta->postfix_expression_lhs, *lhs_type, *tok_dot).scopes({sm->current_scope}).raise();
        }

        // Check the index is within the bounds of the tuple/array.
        if (not analyse::utils::type_utils::is_index_within_type_bound(std::stoi(name->val), *lhs_type, *sm->current_scope)) {
            analyse::errors::SppMemberAccessOutOfBoundsError(*meta->postfix_expression_lhs, *lhs_type, *tok_dot).scopes({sm->current_scope}).raise();
        }
    }

    // Accessing a regular attribute/method on an instance.
    else {
        const auto lhs_as_ident = ast_cast<IdentifierAst>(meta->postfix_expression_lhs);
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
        const auto lhs_ns_sym = sm->current_scope->get_ns_symbol(*lhs_as_ident);
        const auto lhs_var_sym = sm->current_scope->get_var_symbol(*lhs_as_ident);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(*lhs_type);

        // Check the lhs is a variable and not a namespace.
        if (lhs_var_sym == nullptr and lhs_ns_sym != nullptr) {
            analyse::errors::SppMemberAccessStaticOperatorExpectedError(*meta->postfix_expression_lhs, *tok_dot).scopes({sm->current_scope}).raise();
        }

        // Check the left-hand-side isn't a generic type.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SppGenericTypeInvalidUsageError(*meta->postfix_expression_lhs, *lhs_type, "member access").scopes({sm->current_scope}).raise();
        }

        // Check the target field exists on the type.
        if (not lhs_type_sym->scope->has_var_symbol(*name, true)) {
            const auto alternatives = sm->current_scope->all_var_symbols(true, true)
                | genex::views::map([](auto &&x) { return x->name->val; })
                | genex::views::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(lhs_as_ident->val, alternatives);
            analyse::errors::SppIdentifierUnknownError(*this, "instance member", closest_match).scopes({sm->current_scope}).raise();
        }

        // Check there is only 1 target field on the type at the highest level.
        if (lhs_type_sym->scope->get_var_symbol(*name)->type->type_parts().back()->name[0] == '$') {
            return;
        }

        auto scopes_and_syms = std::vector{lhs_type_sym->scope}
            | genex::views::concat(lhs_type_sym->scope->sup_scopes())
            | genex::views::map([name=name.get()](auto &&x) { return std::make_pair(x, x->m_sym_table.var_tbl.get(*name)); })
            | genex::views::filter([](auto &&x) { return x.second != nullptr; })
            | genex::views::map([lhs_type_sym](auto &&x) { return std::make_tuple(lhs_type_sym->scope->depth_difference(x.first), x.first, x.second); })
            | genex::views::to<std::vector>();

        auto min_depth = scopes_and_syms
            | genex::algorithms::min([](auto &&x) { return std::get<0>(x); });

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto &&x) { return std::get<0>(x) == min_depth; })
            | genex::views::map([](auto &&x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
            | genex::views::to<std::vector>();

        if (closest.size() > 1) {
            analyse::errors::SppAmbiguousMemberAccessError(closest[0].second->name, closest[1].second->name, *name).scopes({closest[0].first, closest[1].first, sm->current_scope}).raise();
        }
    }
}
