#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>

#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/to.hpp>
#include <opex/cast.hpp>


spp::analyse::scopes::ScopeManager::ScopeManager(
    std::shared_ptr<Scope> const &global_scope,
    Scope *current_scope) :
    global_scope(global_scope),
    current_scope(current_scope ? current_scope : global_scope.get()),
    m_generator(iter()),
    m_it(m_generator.begin()),
    m_end(m_generator.end()) {
}


auto spp::analyse::scopes::ScopeManager::iter() const -> std::generator<Scope*> {
    // Define the generation algorithm for iterating scopes.
    auto iterator = [](this auto &&self, const Scope *scope) -> std::generator<Scope*> {
        for (auto &&child : scope->children) {
            co_yield child.get();
            for (auto *descendant : self(child.get())) {
                co_yield descendant;
            }
        }
    };

    // Generate from the lambda.
    for (auto *scope : iterator(current_scope)) {
        co_yield scope;
    }
}


auto spp::analyse::scopes::ScopeManager::reset(
    Scope *scope,
    std::optional<std::generator<Scope*>> gen)
    -> void {
    // Set the current scope to the provided scope or global scope.
    current_scope = scope ? scope : global_scope.get();

    // Reset the iterator from the new generator.
    m_generator = gen.has_value() ? std::move(gen.value()) : iter();
    m_it = m_generator.begin();
    m_end = m_generator.end();
}


auto spp::analyse::scopes::ScopeManager::create_and_move_into_new_scope(
    ScopeName name,
    asts::Ast *ast,
    std::unique_ptr<spp::utils::errors::ErrorFormatter> &&error_formatter)
    -> Scope* {
    // Create a new scope, using the current scope as the parent scope.
    auto scope = std::make_unique<Scope>(name, current_scope, ast, std::move(error_formatter));
    current_scope->children.emplace_back(std::move(scope));

    // Set the new scope as the current scope, and advance the iterator to match.
    current_scope = current_scope->children.back().get();
    ++m_it;
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
    // Move ti the next scope by advancing the iterator.
    current_scope = *++m_it;
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::get_namespaced_scope(
    std::vector<asts::IdentifierAst*> const &names) const
    -> Scope* {
    // Find the first scope that matches the first part of the namespace.
    auto ns_sym = current_scope->get_ns_symbol(*names[0]);
    if (ns_sym == nullptr) { return nullptr; }

    // Work inwards, getting the nested scopes for the namespace parts.
    auto ns_scope = ns_sym->scope;
    for (auto &&ns_part : names | genex::views::drop(1)) {
        ns_sym = ns_scope->get_ns_symbol(*ns_part);
        if (ns_sym == nullptr) { return nullptr; }
        ns_scope = ns_sym->scope;
    }
    return ns_scope;
}


auto spp::analyse::scopes::ScopeManager::attach_all_super_scopes(
    asts::mixins::CompilerMetaData *meta)
    -> void {
    // Ensure the scope manager is at the global scope.
    reset();
    iter()
        | genex::views::filter([](auto *scope) { return scope->ty_sym != nullptr; })
        | genex::views::for_each([meta, this](auto *scope) { attach_specific_super_scopes(*scope, meta); });
    reset();
}


auto spp::analyse::scopes::ScopeManager::attach_specific_super_scopes(
    Scope &scope,
    asts::mixins::CompilerMetaData *meta) const
    -> void {
    // Handle alias symbols.
    if (const auto alias_sym = dynamic_cast<AliasSymbol*>(scope.ty_sym.get()); alias_sym != nullptr and alias_sym->old_sym->scope != nullptr) {
        const auto old_scope = alias_sym->old_sym->scope;
        auto scopes = genex::views::concat(normal_sup_blocks[old_scope->ty_sym.get()], generic_sup_blocks) | genex::views::to<std::vector>();
        attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
    }

    // Handle type symbols.
    else if (const auto type_sym = scope.ty_sym.get(); type_sym != nullptr) {
        auto scopes = genex::views::concat(normal_sup_blocks[scope.m_non_generic_scope->ty_sym.get()], generic_sup_blocks) | genex::views::to<std::vector>();
        attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
    }
}


auto spp::analyse::scopes::ScopeManager::attach_specific_super_scopes_impl(
    Scope &scope,
    std::vector<Scope*> &&sup_scopes,
    asts::mixins::CompilerMetaData *meta) const
    -> void {
    // Skip "$" identifiers (functions don't have substitutable members and take up lots of time).
    auto scope_name = std::get<asts::TypeIdentifierAst*>(scope.name);
    if (scope_name->type_parts().back()->name[0] == '$') {
        return;
    }
    if (utils::type_utils::is_type_function(*scope_name, scope)) {
        return;
    }

    // Clear the sup scopes list.
    scope.m_direct_sup_scopes.clear();

    // Iterator through all the super scopes and check if the name matches.
    for (auto *sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types (allows for specializations to match bases).
        auto scope_generics_map = std::map<asts::TypeAst*, asts::ExpressionAst*>();
        if (not utils::type_utils::relaxed_symbolic_eq(*scope.ty_sym->fq_name(), *asts::ast_name(sup_scope->ast), *scope.ty_sym->scope_defined_in, *sup_scope, scope_generics_map)) {
            continue;
        }
        auto scope_generics = asts::GenericArgumentGroupAst::from_map(std::move(scope_generics_map));

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<Scope*>(nullptr);
        auto new_cls_scope = static_cast<Scope*>(nullptr);
        auto sup_sym = static_cast<TypeSymbol*>(nullptr);

        if (not scope_generics->args.empty() and not genex::algorithms::contains(generic_sup_blocks, sup_scope)) {
            const auto external_generics = scope.ty_sym->scope_defined_in->get_extended_generic_symbols(scope_generics->args | genex::views::ptr | genex::views::to<std::vector>());
            std::tie(new_sup_scope, new_cls_scope) = utils::type_utils::create_generic_sup_scope(*sup_scope, scope, *scope_generics, external_generics, *this, meta);
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        else {
            const auto sup_proto = asts::ast_cast<asts::SupPrototypeExtensionAst>(sup_scope->ast);
            new_sup_scope = sup_scope;
            new_cls_scope = sup_proto ? scope.get_type_symbol(*sup_proto->super_class)->scope : nullptr;
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        auto cls_sym = scope.ty_sym;

        // Prevent double inheritance, cyclic inheritance and self extension.
        if (const auto ext_ast = asts::ast_cast<asts::SupPrototypeExtensionAst>(scope.ast); ext_ast != nullptr) {
            ext_ast->m_check_cyclic_extension(*sup_sym, *sup_scope);
            ext_ast->m_check_double_extension(*cls_sym, *sup_scope);
            ext_ast->m_check_self_extension(*sup_scope);
        }

        // Register the super scope against the current scope.
        scope.m_direct_sup_scopes.emplace_back(new_sup_scope);

        // Register the super scope's class scope against the current scope, if it is different. Ths "difference" check
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
    auto dummy = std::map<asts::TypeAst*, asts::ExpressionAst*>();
    auto existing_scopes = cls_sym.scope->m_direct_sup_scopes
        | genex::views::filter([&](auto *scope) { return asts::ast_cast<asts::SupPrototypeExtensionAst>(scope->ast) or asts::ast_cast<asts::SupPrototypeFunctionsAst>(scope->ast); })
        | genex::views::filter([&](auto *scope) { return utils::type_utils::relaxed_symbolic_eq(*ast_name(sup_scope.ast), *ast_name(scope->ast), sup_scope, *scope->ast->m_scope, dummy); })
        | genex::views::to<std::vector>();

    // Check for conflicting "type" statements.
    auto existing_types = existing_scopes
        | genex::views::transform([](auto *scope) { return asts::ast_body(scope->ast); })
        | genex::views::flatten
        | genex::views::filter([](auto *member) { return asts::ast_cast<asts::TypeStatementAst>(member); })
        | genex::views::to<std::vector>();

    auto existing_type_names = existing_types
        | genex::views::transform([](auto *type_stmt) { return asts::ast_cast<asts::TypeStatementAst>(type_stmt)->new_type->name; })
        | genex::views::to<std::vector>();

    auto duplicate_type_names = existing_type_names
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not duplicate_type_names.empty()) {
        const auto i1 = genex::algorithms::position(existing_type_names, [&](auto &&x) { return x == duplicate_type_names[0]; });
        const auto i2 = genex::algorithms::position(existing_type_names, [&](auto &&x) { return x == duplicate_type_names[1]; }, {}, -1, i1 + 1);
        const auto d1 = existing_types[i1 as USize];
        const auto d2 = existing_types[i2 as USize];
        errors::SemanticErrorBuilder<errors::SppIdentifierDuplicateError>().with_args(
            *d1, *d2, "associated type").with_scopes({d1->m_scope, d2->m_scope}).raise();
    }

    // Check for conflicting "cmp" statements.
    auto existing_cmps = existing_scopes
        | genex::views::transform([](auto *scope) { return asts::ast_body(scope->ast); })
        | genex::views::flatten
        | genex::views::filter([](auto *member) { return asts::ast_cast<asts::CmpStatementAst>(member); })
        | genex::views::to<std::vector>();

    auto existing_cmp_names = existing_cmps
        | genex::views::transform([](auto *cmp_stmt) { return asts::ast_cast<asts::CmpStatementAst>(cmp_stmt)->name; })
        | genex::views::to<std::vector>();

    auto duplicate_cmp_names = existing_cmp_names
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not duplicate_cmp_names.empty()) {
        const auto i1 = genex::algorithms::position(existing_cmp_names, [&](auto &&x) { return x == duplicate_cmp_names[0]; });
        const auto i2 = genex::algorithms::position(existing_cmp_names, [&](auto &&x) { return x == duplicate_cmp_names[1]; }, {}, -1, i1 + 1);
        const auto d1 = existing_cmps[i1 as USize];
        const auto d2 = existing_cmps[i2 as USize];
        errors::SemanticErrorBuilder<errors::SppIdentifierDuplicateError>().with_args(
            *d1, *d2, "comparison operator").with_scopes({d1->m_scope, d2->m_scope}).raise();
    }
}
