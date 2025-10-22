#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>

#include <genex/to_container.hpp>
#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/join.hpp>
#include <genex/views/ptr.hpp>
#include <opex/cast.hpp>


spp::analyse::scopes::ScopeManager::ScopeManager(
    std::shared_ptr<Scope> const &global_scope,
    Scope *current_scope) :
    global_scope(global_scope),
    current_scope(current_scope ? current_scope : global_scope.get()) {
}


auto spp::analyse::scopes::ScopeManager::iter() const -> ScopeRange {
    return ScopeRange{current_scope};
}


auto spp::analyse::scopes::ScopeManager::reset(
    Scope *scope,
    std::optional<ScopeIterator> iterator)
    -> void {
    // Set the current scope to the provided scope or global scope.
    current_scope = scope ? scope : global_scope.get();
    m_it = iterator.has_value() ? *iterator : ScopeIterator{current_scope};
}


auto spp::analyse::scopes::ScopeManager::create_and_move_into_new_scope(
    ScopeName const &name,
    asts::Ast *ast,
    spp::utils::errors::ErrorFormatter *error_formatter)
    -> Scope* {
    // Create a new scope, using the current scope as the parent scope.
    auto scope = std::make_unique<Scope>(name, current_scope, ast, error_formatter);
    current_scope->children.emplace_back(std::move(scope));
    ++m_it;

    // Set the new scope as the current scope, and advance the iterator to match.
    current_scope = current_scope->children.back().get();
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::move_out_of_current_scope()
    -> Scope* {
    // Exit the current scope into the parent scope.
    current_scope = current_scope->parent;
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::move_to_next_scope()
    -> Scope* {
    // For debugging mode only, check if the iterator has reached the end of the generator.
    // Move to the next scope by advancing the iterator.
    current_scope = *++m_it;
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::attach_all_super_scopes(
    asts::mixins::CompilerMetaData *meta)
    -> void {
    // Ensure the scope manager is at the global scope.
    reset();
    for (auto *scope : iter()) {
        if (scope->ty_sym != nullptr) {
            attach_specific_super_scopes(*scope, meta);
        }
    }
    reset();
}


auto spp::analyse::scopes::ScopeManager::attach_specific_super_scopes(
    Scope &scope,
    asts::mixins::CompilerMetaData *meta) const
    -> void {
    // Handle alias symbols.
    if (const auto alias_sym = dynamic_cast<AliasSymbol*>(scope.ty_sym.get()); alias_sym != nullptr) {
        if (alias_sym->old_sym->scope != nullptr) {
            const auto old_scope = alias_sym->old_sym->scope;
            auto scopes = genex::views::concat(normal_sup_blocks[old_scope->ty_sym.get()], generic_sup_blocks) | genex::to<std::vector>();
            attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
        }
    }

    // Handle type symbols.
    else if (scope.ty_sym != nullptr) {
        const auto non_generic_sym = scope.get_type_symbol(scope.ty_sym->name->without_generics());
        auto scopes = genex::views::concat(normal_sup_blocks[non_generic_sym.get()], generic_sup_blocks) | genex::to<std::vector>();
        attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
    }
}


auto spp::analyse::scopes::ScopeManager::attach_specific_super_scopes_impl(
    Scope &scope,
    std::vector<Scope*> &&sup_scopes,
    asts::mixins::CompilerMetaData *meta) const
    -> void {
    // Skip "$" identifiers (functions don't have substitutable members and take up lots of time).
    const auto scope_name = std::get<std::shared_ptr<asts::TypeIdentifierAst>>(scope.name);
    if (scope_name->type_parts().back()->name[0] == '$') {
        return;
    }
    if (utils::type_utils::is_type_function(*scope_name, scope)) {
        return;
    }

    // Clear the sup scopes list.
    scope.m_direct_sup_scopes.clear();

    // Iterate through all the super scopes and check if the name matches.
    for (auto *sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types (allows for specializations to match bases).
        auto scope_generics_map = utils::type_utils::GenericInferenceMap();
        auto fq_type = scope.ty_sym->fq_name();
        if (not utils::type_utils::relaxed_symbolic_eq(*fq_type, *asts::ast_name(sup_scope->ast), scope.ty_sym->scope_defined_in, sup_scope, scope_generics_map)) {
            continue;
        }
        auto scope_generics = asts::GenericArgumentGroupAst::from_map(std::move(scope_generics_map));

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<Scope*>(nullptr);
        auto new_cls_scope = static_cast<Scope*>(nullptr);
        auto sup_sym = static_cast<TypeSymbol*>(nullptr);

        if (not scope_generics->args.empty() and not genex::algorithms::contains(generic_sup_blocks, sup_scope)) {
            const auto external_generics = scope.ty_sym->scope_defined_in->get_extended_generic_symbols(scope_generics->args | genex::views::ptr | genex::to<std::vector>());
            std::tie(new_sup_scope, new_cls_scope) = utils::type_utils::create_generic_sup_scope(*sup_scope, scope, *scope_generics, external_generics, this, meta);
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        else {
            const auto sup_proto = asts::ast_cast<asts::SupPrototypeExtensionAst>(sup_scope->ast);
            new_sup_scope = sup_scope;
            new_cls_scope = sup_proto ? scope.get_type_symbol(sup_proto->super_class)->scope : nullptr;
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        auto cls_sym = scope.ty_sym;

        // Prevent double inheritance, cyclic inheritance and self extension.
        if (const auto ext_ast = asts::ast_cast<asts::SupPrototypeExtensionAst>(sup_scope->ast); ext_ast != nullptr) {
            ext_ast->m_check_cyclic_extension(*sup_sym, *sup_scope);
            ext_ast->m_check_double_extension(*cls_sym, *sup_scope);
            ext_ast->m_check_self_extension(*sup_scope);
        }

        // Register the super scope against the current scope.
        scope.m_direct_sup_scopes.emplace_back(new_sup_scope);

        // Register the super scope's class scope against the current scope, if it is different. This "difference" check
        // ensures that "sup [T] T ext A" doesn't create a "sup A ext A" link.
        if (new_cls_scope and scope.ty_sym != new_cls_scope->ty_sym) {
            scope.m_direct_sup_scopes.emplace_back(new_cls_scope);
        }

        // Check for conflicting "cmp" or "type" statements in the super scopes.
        if (asts::ast_cast<asts::SupPrototypeExtensionAst>(sup_scope->ast) or asts::ast_cast<asts::SupPrototypeFunctionsAst>(sup_scope->ast)) {
            check_conflicting_type_or_cmp_statements(*cls_sym, *sup_scope);
        }
    }
}


auto spp::analyse::scopes::ScopeManager::check_conflicting_type_or_cmp_statements(
    TypeSymbol const &cls_sym,
    Scope const &sup_scope)
    -> void {
    // Get the scopes to check for conflicts in.
    auto dummy = utils::type_utils::GenericInferenceMap();
    const auto existing_scopes = cls_sym.scope->m_direct_sup_scopes
        | genex::views::filter([&](auto *scope) { return asts::ast_cast<asts::SupPrototypeExtensionAst>(scope->ast) or asts::ast_cast<asts::SupPrototypeFunctionsAst>(scope->ast); })
        | genex::views::filter([&](auto *scope) { return utils::type_utils::relaxed_symbolic_eq(*ast_name(sup_scope.ast), *ast_name(scope->ast), &sup_scope, scope->ast->m_scope, dummy); })
        | genex::to<std::vector>();

    // Check for conflicting "type" statements.
    std::vector<std::shared_ptr<asts::TypeIdentifierAst>> new_types;
    for (auto scope: existing_scopes) {
        auto body = asts::ast_body(scope->ast);
        for (const auto member: body) {
            if (const auto type_stmt = asts::ast_cast<asts::TypeStatementAst>(member); type_stmt != nullptr) {
                for (const auto &new_type: new_types) {
                    if (*new_type == *type_stmt->new_type) {
                        errors::SemanticErrorBuilder<errors::SppIdentifierDuplicateError>().with_args(
                            *new_type, *type_stmt->new_type, "associated type").with_scopes({scope, &sup_scope}).raise();
                    }
                }
                new_types.emplace_back(type_stmt->new_type);
            }
        }
    }

    // Check for conflicting "cmp" statements.
    std::vector<std::shared_ptr<asts::IdentifierAst>> new_cmps;
    for (auto scope: existing_scopes) {
        auto body = asts::ast_body(scope->ast);
        for (const auto member: body) {
            if (const auto cmp_stmt = asts::ast_cast<asts::CmpStatementAst>(member); cmp_stmt != nullptr and cmp_stmt->type->type_parts().back()->name[0] != '$') {
                for (const auto &new_cmp: new_cmps) {
                    if (*new_cmp == *cmp_stmt->name) {
                        errors::SemanticErrorBuilder<errors::SppIdentifierDuplicateError>().with_args(
                            *new_cmp, *cmp_stmt->name, "comptime constant").with_scopes({scope, &sup_scope}).raise();
                    }
                }
                new_cmps.emplace_back(cmp_stmt->name);
            }
        }
    }
}


auto spp::analyse::scopes::ScopeManager::cleanup() -> void {
    normal_sup_blocks.clear();
    generic_sup_blocks.clear();
}
