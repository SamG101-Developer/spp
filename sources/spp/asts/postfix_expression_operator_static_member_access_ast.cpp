#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/utils/strings.hpp>

#include <genex/algorithms/min_element.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/to.hpp>
#include <genex/views/transform.hpp>


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(tok_dbl_colon) &&tok_dbl_colon,
    decltype(name) &&name) :
    tok_dbl_colon(std::move(tok_dbl_colon)),
    name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_dbl_colon, lex::SppTokenType::TK_DOUBLE_COLON, "::", name ? name->pos_start() : 0);
}


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_start() const -> std::size_t {
    return tok_dbl_colon->pos_start();
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorStaticMemberAccessAst>(
        ast_clone(tok_dbl_colon),
        ast_clone(name));
}


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dbl_colon);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dbl_colon);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Handle types on the left-hand-side of a static member access.
    if (const auto lhs_as_type = ast_cast<TypeIdentifierAst>(meta->postfix_expression_lhs)) {
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_as_type->shared_from_this());

        // Check the left-hand-side isn't a generic type. Todo: in the future, allow by constraints / intersection types?
        if (lhs_type_sym->is_generic) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>().with_args(
                *lhs_as_type, *lhs_as_type, "member access").with_scopes({sm->current_scope}).raise();
        }

        // Check the target field exists on the type.
        if (not lhs_type_sym->scope->has_var_symbol(name, true)) {
            const auto alternatives = sm->current_scope->all_type_symbols(true, true)
                | genex::views::transform([](auto &&x) { return x->name->name; })
                | genex::views::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(lhs_as_type->name, alternatives);
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>().with_args(
                *this, "type member", closest_match).with_scopes({sm->current_scope}).raise();
        }

        // Check there is only 1 target field on the type at the highest level.
        if (lhs_type_sym->scope->get_var_symbol(name)->type->type_parts().back()->name[0] == '$') {
            return;
        }

        auto scopes_and_syms = std::vector{lhs_type_sym->scope}
            | genex::views::concat(lhs_type_sym->scope->sup_scopes())
            | genex::views::transform([name=name.get()](auto &&x) { return std::make_pair(x, x->table.var_tbl.get(ast_clone(name))); })
            | genex::views::filter([](auto &&x) { return x.second != nullptr; })
            | genex::views::transform([lhs_type_sym](auto &&x) { return std::make_tuple(lhs_type_sym->scope->depth_difference(x.first), x.first, x.second); })
            | genex::views::to<std::vector>();

        auto min_depth = genex::algorithms::min_element(scopes_and_syms
            | genex::views::transform([](auto &&x) { return std::get<0>(x); })
            | genex::views::to<std::vector>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto &&x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto &&x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
            | genex::views::to<std::vector>();

        if (closest.size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppAmbiguousMemberAccessError>().with_args(
                *closest[0].second->name, *closest[1].second->name, *name).with_scopes({closest[0].first, closest[1].first, sm->current_scope}).raise();
        }
    }

    else if (const auto lhs_as_ident_raw = ast_cast<IdentifierAst>(meta->postfix_expression_lhs)) {
        const auto lhs_as_ident = std::make_shared<IdentifierAst>(lhs_as_ident_raw->pos_start(), lhs_as_ident_raw->val);
        const auto lhs_ns_sym = sm->current_scope->get_ns_symbol(lhs_as_ident);
        const auto lhs_var_sym = sm->current_scope->get_var_symbol(lhs_as_ident);

        // Check the lhs is a namespace and not a variable.
        if (lhs_ns_sym == nullptr and lhs_var_sym != nullptr) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessRuntimeOperatorExpectedError>().with_args(
                *meta->postfix_expression_lhs, *tok_dbl_colon).with_scopes({sm->current_scope}).raise();
        }

        // Check the constant exists inside the namespace.
        if (not lhs_ns_sym->scope->has_var_symbol(name, true)) {
            const auto alternatives = sm->current_scope->all_var_symbols(false, true)
                | genex::views::transform([](auto &&x) { return x->name->val; })
                | genex::views::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(lhs_as_ident->val, alternatives);
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>().with_args(
                *this, "namespace member", closest_match).with_scopes({sm->current_scope}).raise();
        }
    }
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the left-hand-side type's member's type.
    // Todo: have to use an "ast_clone" rather than "enable_from_shared" atm, unsure why.
    if (const auto lhs_as_type = ast_cast<TypeAst>(meta->postfix_expression_lhs)) {
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(ast_clone(lhs_as_type));
        return lhs_type_sym->scope->get_var_symbol(name, true)->type;
    }

    // Get the left-hand-side namespace's member's type.
    if (const auto lhs_as_ident = ast_cast<IdentifierAst>(meta->postfix_expression_lhs)) {
        const auto lhs_ns_sym = sm->current_scope->get_ns_symbol(ast_clone(lhs_as_ident));
        return lhs_ns_sym->scope->get_var_symbol(name, true)->type;
    }

    // Should never happen due to earlier checks.
    std::unreachable();
}
