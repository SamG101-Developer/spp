module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.strings;
import genex;


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(tok_dbl_colon) &&tok_dbl_colon,
    decltype(name) &&name) :
    tok_dbl_colon(std::move(tok_dbl_colon)),
    name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_dbl_colon, lex::SppTokenType::TK_DOUBLE_COLON, "::", name ? name->pos_start() : 0);
}


spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::~PostfixExpressionOperatorStaticMemberAccessAst() = default;


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_start() const
    -> std::size_t {
    return tok_dbl_colon->pos_start();
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::clone() const
    -> std::unique_ptr<Ast> {
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


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Handle types on the left-hand-side of a static member access.
    if (const auto lhs_as_type = meta->postfix_expression_lhs->to<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(ast_clone(lhs_as_type));

        // Check the left-hand-side isn't a generic type. Todo: until constraints.
        raise_if<analyse::errors::SppGenericTypeInvalidUsageError>(
            lhs_type_sym->is_generic, {sm->current_scope},
            ERR_ARGS(*lhs_as_type, *lhs_as_type, "member access"));

        // Check the target field exists on the type.
        if (not lhs_type_sym->scope->has_var_symbol(name, true)) {
            const auto alternatives = sm->current_scope->all_type_symbols(true, true)
                | genex::views::transform([](auto &&x) { return x->name->name; })
                | genex::to<std::vector>();

            const auto closest_match = spp::utils::strings::closest_match(name->val, alternatives);
            raise<analyse::errors::SppIdentifierUnknownError>(
                {sm->current_scope}, ERR_ARGS(*this, "type member", closest_match));
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
            | genex::to<std::vector>();

        auto min_depth = genex::min_element(scopes_and_syms
            | genex::views::transform([](auto &&x) { return std::get<0>(x); })
            | genex::to<std::vector>());

        auto closest = scopes_and_syms
            | genex::views::filter([min_depth](auto &&x) { return std::get<0>(x) == min_depth; })
            | genex::views::transform([](auto &&x) { return std::make_pair(std::get<1>(x), std::get<2>(x)); })
            | genex::to<std::vector>();

        if (closest.size() <= 1) { return; }
        raise<analyse::errors::SppAmbiguousMemberAccessError>(
            {closest[0].first, closest[1].first, sm->current_scope},
            ERR_ARGS(*closest[0].second->name, *closest[1].second->name, *name));
    }

    else {
        const auto lhs_as_ident = meta->postfix_expression_lhs->to<IdentifierAst>();
        const auto lhs_var_sym = sm->current_scope->get_var_symbol(ast_clone(lhs_as_ident));

        // Check the lhs is a namespace and not a variable.
        raise_if<analyse::errors::SppMemberAccessRuntimeOperatorExpectedError>(
            lhs_var_sym != nullptr, {sm->current_scope},
            ERR_ARGS(*meta->postfix_expression_lhs, *tok_dbl_colon));

        // Check the constant exists inside the namespace.
        const auto lhs_ns_sym = sm->current_scope->convert_postfix_to_nested_scope(meta->postfix_expression_lhs)->ns_sym;
        if (not lhs_ns_sym->scope->has_var_symbol(name, true) and not lhs_ns_sym->scope->has_ns_symbol(name, true)) {
            const auto alternatives = sm->current_scope->all_var_symbols(false, true)
                | genex::views::transform([](auto &&x) { return x->name->val; })
                | genex::to<std::vector>();

            // Todo: get the last part of postfix otherwise identifier value for string.
            const auto closest_match = spp::utils::strings::closest_match(
                static_cast<std::string>(*meta->postfix_expression_lhs), alternatives);

            raise<analyse::errors::SppIdentifierUnknownError>(
                {sm->current_scope}, ERR_ARGS(*this, "namespace member", closest_match));
        }
    }
}


auto spp::asts::PostfixExpressionOperatorStaticMemberAccessAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the left-hand-side type's member's type.
    if (const auto lhs_as_type = meta->postfix_expression_lhs->to<TypeAst>(); lhs_as_type != nullptr) {
        const auto lhs_type_sym = sm->current_scope->get_type_symbol(ast_clone(lhs_as_type));
        const auto sym = lhs_type_sym->scope->get_var_symbol(name, true);
        if (sym != nullptr) { return sym->type; }

        // This is where we need to handle the FwdRef/FwdMut logic.
        auto lhs_as_type_clone = std::shared_ptr(ast_clone(lhs_as_type));
        auto [fwd_ref_type, _] = analyse::utils::type_utils::get_fwd_types(*lhs_as_type_clone, *sm);
        const auto inner_type = fwd_ref_type->type_parts().back()->generic_arg_group->type_at("T")->val;
        const auto inner_type_sym = sm->current_scope->get_type_symbol(inner_type);
        const auto fwd_sym = inner_type_sym->scope->get_var_symbol(name, true);
        return fwd_sym->type;
    }

    // Get the left-hand-side namespace's member's type.
    const auto lhs_ns_scope = sm->current_scope->convert_postfix_to_nested_scope(meta->postfix_expression_lhs);
    const auto type = lhs_ns_scope->get_var_symbol(name, true)->type;
    return lhs_ns_scope->get_type_symbol(type)->fq_name();
}
