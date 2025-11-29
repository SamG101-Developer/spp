module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.asts.meta.compiler_meta_data;
import spp.utils.strings;
import genex;


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(tok_dot) &&tok_dot,
    decltype(name) name) :
    tok_dot(std::move(tok_dot)),
    name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_dot, lex::SppTokenType::TK_DOT, ".");
}


spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::~PostfixExpressionOperatorRuntimeMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_start() const
    -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {

    // Prevent types on the left-hand-side of a runtime member access.
    if (ast_cast<TypeAst>(meta->postfix_expression_lhs)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessStaticOperatorExpectedError>().with_args(
            *meta->postfix_expression_lhs, *tok_dot).with_scopes({sm->current_scope}).raise();
    }

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);

        // Check the left-hand-side isn't a generic type. Todo: until constraints.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>().with_args(
                *meta->postfix_expression_lhs, *lhs_type, "member access").with_scopes({sm->current_scope}).raise();
        }

        // Check the lhs is a tuple/array (the only indexable types).
        if (not analyse::utils::type_utils::is_type_comptime_indexable(*lhs_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>().with_args(
                *meta->postfix_expression_lhs, *lhs_type, *tok_dot).with_scopes({sm->current_scope}).raise();
        }

        // Check the index is within the bounds of the tuple/array.
        if (not analyse::utils::type_utils::is_index_within_type_bound(std::stoul(name->val), *lhs_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessOutOfBoundsError>().with_args(
                *meta->postfix_expression_lhs, *lhs_type, *tok_dot).with_scopes({sm->current_scope}).raise();
        }
    }

    // Accessing a regular attribute/method on an instance.
    else {
        const auto lhs_as_ident_raw = ast_cast<IdentifierAst>(meta->postfix_expression_lhs);
        const auto lhs_as_ident = lhs_as_ident_raw ? std::make_shared<IdentifierAst>(lhs_as_ident_raw->pos_start(), lhs_as_ident_raw->val) : nullptr;
        const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

        const auto lhs_ns_sym = sm->current_scope->get_ns_symbol(lhs_as_ident);
        const auto lhs_var_sym = sm->current_scope->get_var_symbol(lhs_as_ident);
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(lhs_type);

        // Check the lhs is a variable and not a namespace.
        if (lhs_var_sym == nullptr and lhs_ns_sym != nullptr) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessStaticOperatorExpectedError>().with_args(
                *meta->postfix_expression_lhs, *tok_dot).with_scopes({sm->current_scope}).raise();
        }

        // Check the left-hand-side isn't a generic type.
        if (lhs_type_sym->is_generic) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppGenericTypeInvalidUsageError>().with_args(
                *meta->postfix_expression_lhs, *lhs_type, "member access").with_scopes({sm->current_scope}).raise();
        }

        // Check the target field exists on the type.
        if (not lhs_type_sym->scope->has_var_symbol(name, true)) {
            const auto alternatives = sm->current_scope->all_var_symbols(true, true)
                | genex::views::transform([](auto const &x) { return x->name->val; })
                | genex::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(name->val, alternatives);
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierUnknownError>().with_args(
                *this, "instance member", closest_match).with_scopes({sm->current_scope}).raise();
        }

        // Check there is only 1 target field on the type at the highest level.
        if (lhs_type_sym->scope->get_var_symbol(name)->type->type_parts().back()->name[0] == '$') {
            return;
        }

        auto scopes_and_syms = std::vector{lhs_type_sym->scope}
            | genex::views::concat(lhs_type_sym->scope->sup_scopes())
            | genex::views::transform([name=name.get()](auto const &x) { return std::make_pair(x, x->table.var_tbl.get(ast_clone(name))); })
            | genex::views::filter([](auto const &x) { return x.second != nullptr; })
            | genex::views::transform([lhs_type_sym](auto const &x) { return std::make_tuple(lhs_type_sym->scope->depth_difference(x.first), x.first, x.second); })
            | genex::to<std::vector>();

        auto min_depth = genex::min_element(scopes_and_syms
            | genex::views::transform([](auto const &x) { return std::get<0>(x); })
            | genex::to<std::vector>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto const &x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto const &x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
            | genex::to<std::vector>();

        if (closest.size() > 1) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppAmbiguousMemberAccessError>().with_args(
                *closest[0].second->name, *closest[1].second->name, *name).with_scopes({closest[0].first, closest[1].first, sm->current_scope}).raise();
        }
    }
}


auto spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the type of the left-hand-side expression.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);

    // Numeric index access (for tuples).
    if (std::isdigit(name->val[0])) {
        const auto elem_type = analyse::utils::type_utils::get_nth_type_of_indexable_type(
            std::stoul(name->val), *lhs_type, *sm->current_scope);
        return elem_type;
    }

    // Get the field symbol and return its type.
    const auto lhs_sym = sm->current_scope->get_type_symbol(lhs_type);
    const auto field_type = lhs_sym->scope->get_var_symbol(name)->type;
    return lhs_sym->scope->get_type_symbol(field_type)->fq_name();
}
