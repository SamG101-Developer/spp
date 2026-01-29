module;
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.error_formatter;
import genex;


spp::analyse::scopes::ScopeManager::ScopeManager(
    std::shared_ptr<Scope> const &global_scope,
    Scope *current_scope) :
    global_scope(global_scope),
    current_scope(current_scope ? current_scope : global_scope.get()) {
}


auto spp::analyse::scopes::ScopeManager::iter() const
    -> ScopeRange {
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


auto spp::analyse::scopes::ScopeManager::move_to_next_scope(
    const bool ignore_alias_class_scopes)
    -> Scope* {
    // For debugging mode only, check if the iterator has reached the end of the generator.
    // Move to the next scope by advancing the iterator.
    current_scope = *++m_it;
    while (ignore_alias_class_scopes and current_scope->ty_sym != nullptr and current_scope->ty_sym->alias_stmt != nullptr) {
        current_scope = *++m_it;
    }
    return current_scope;
}


auto spp::analyse::scopes::ScopeManager::exhaust_scope()
    -> void {
    // Manual scope skipping.
    const auto final_scope = current_scope->final_child_scope();
    while (current_scope != final_scope) {
        move_to_next_scope(false);
    }
}


auto spp::analyse::scopes::ScopeManager::attach_llvm_type_info(
    asts::ModulePrototypeAst const &mod,
    codegen::LLvmCtx *ctx) const
    -> void {
    // Iterate the members of the module, filter to class prototypes, and call the register function.

    auto cls_members = std::vector<asts::ClassPrototypeAst*>{};
    for (auto const &member : mod.impl->members) {
        if (const auto cls_member = member->to<asts::ClassPrototypeAst>(); cls_member != nullptr) {
            cls_members.emplace_back(cls_member);
        }

        if (const auto sup_member = member->to<asts::SupPrototypeFunctionsAst>(); sup_member != nullptr) {
            for (auto const &sup_member_inner : sup_member->impl->members) {
                if (const auto cls_sup_member = sup_member_inner->to<asts::ClassPrototypeAst>(); cls_sup_member != nullptr) {
                    cls_members.emplace_back(cls_sup_member);
                }
            }
        }

        if (const auto ext_member = member->to<asts::SupPrototypeExtensionAst>(); ext_member != nullptr) {
            for (auto const &ext_member_inner : ext_member->impl->members) {
                if (const auto cls_ext_member = ext_member_inner->to<asts::ClassPrototypeAst>(); cls_ext_member != nullptr) {
                    cls_members.emplace_back(cls_ext_member);
                }
            }
        }
    }

    for (auto const &cls_proto : cls_members) {
        // If this is not a base generic (std::vector::Vec)
        if (cls_proto->registered_generic_substitutions().empty()) {
            codegen::register_llvm_type_info(cls_proto, ctx);

            // All aliases need llvm type info propagated from their aliased types.
            const auto llvm_type = codegen::llvm_type(*cls_proto->get_ast_scope()->ty_sym, ctx);
            for (auto const &alias_sym : cls_proto->get_ast_scope()->ty_sym->aliased_by_symbols) {
                alias_sym->llvm_info->llvm_type = llvm_type;
            }
        }

        // All concrete generic implementations (not std::vector::Vec[T]).
        // Todo: don't generate when one of the generics is "comp->identifier" or "type->generic"
        for (auto const &generic_sub : cls_proto->registered_generic_substitutions()) {
            codegen::register_llvm_type_info(generic_sub.second, ctx);

            // All generic aliases need llvm type info propagated from their aliased types.
            const auto llvm_type = codegen::llvm_type(*generic_sub.second->get_ast_scope()->ty_sym, ctx);
            for (auto const &alias_sym : generic_sub.second->get_ast_scope()->ty_sym->aliased_by_symbols) {
                alias_sym->llvm_info->llvm_type = llvm_type;
            }
        }
    }
}


auto spp::analyse::scopes::ScopeManager::attach_all_super_scopes(
    asts::meta::CompilerMetaData *meta)
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
    asts::meta::CompilerMetaData *meta) const
    -> void {
    // Handle type symbols.
    if (scope.ty_sym != nullptr) {
        const auto non_generic_sym = scope.get_type_symbol(scope.ty_sym->fq_name()->without_generics());
        auto scopes = normal_sup_blocks[non_generic_sym.get()];
        scopes.append_range(generic_sup_blocks);
        attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
    }
}


auto spp::analyse::scopes::ScopeManager::attach_specific_super_scopes_impl(
    Scope &scope,
    std::vector<Scope*> &&sup_scopes,
    asts::meta::CompilerMetaData *meta) const
    -> void {
    // Skip "$" identifiers (functions don't have substitutable members and take up lots of time).
    const auto scope_name = scope.ty_sym->fq_name();
    if (scope_name->type_parts().back()->name[0] == '$') {
        return;
    }
    if (utils::type_utils::is_type_function(*scope_name, scope)) {
        return;
    }
    if (sup_scopes.empty()) {
        return;
    }

    // Clear the sup scopes list.
    scope.direct_sup_scopes.clear();
    const auto fq_type = scope.ty_sym->fq_name();
    const auto cls_sym = scope.ty_sym;

    // Iterate through all the super scopes and check if the name matches.
    for (auto *sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types (allows for specializations to match bases).
        auto scope_generics_map = utils::type_utils::GenericInferenceMap();
        if (not utils::type_utils::relaxed_symbolic_eq(*fq_type, *asts::ast_name(sup_scope->ast), *scope.ty_sym->scope_defined_in, *sup_scope, scope_generics_map)) {
            // Todo: Is this eliminating too many cases?
            // Todo: For example, superimposing a type will be omitted here because the type won't be the same.
            continue;
        }
        auto scope_generics = asts::GenericArgumentGroupAst::from_map(std::move(scope_generics_map));

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<Scope*>(nullptr);
        auto new_cls_scope = static_cast<Scope*>(nullptr);
        auto sup_sym = static_cast<TypeSymbol*>(nullptr);

        // Todo: Is this "if-else" quite correct? 2 conditions in the "if", then no "else if" block.
        if (not scope_generics->args.empty() and not genex::contains(generic_sup_blocks, sup_scope)) {
            const auto external_generics = scope.ty_sym->scope_defined_in->get_extended_generic_symbols(scope_generics->args | genex::views::ptr | genex::to<std::vector>());
            std::tie(new_sup_scope, new_cls_scope) = utils::type_utils::create_generic_sup_scope(*sup_scope, scope, *scope_generics, external_generics, this, meta);
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        else {
            const auto sup_proto = sup_scope->ast->to<asts::SupPrototypeExtensionAst>();
            new_sup_scope = sup_scope;
            new_cls_scope = sup_proto ? scope.get_type_symbol(sup_proto->super_class)->scope : nullptr;
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }

        // Prevent double inheritance, cyclic inheritance and self extension.
        if (const auto ext_ast = sup_scope->ast->to<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
            ext_ast->check_cyclic_extension(*sup_sym, *sup_scope);
            ext_ast->check_double_extension(*cls_sym, *sup_scope);
            ext_ast->check_self_extension(*sup_scope);
        }

        // Register the super scope against the current scope.
        scope.direct_sup_scopes.emplace_back(new_sup_scope);

        // Register the super scope's class scope against the current scope, if it is different. This "difference" check
        // ensures that "sup [T] T ext A" doesn't create a "sup A ext A" link.
        if (new_cls_scope and scope.ty_sym != new_cls_scope->ty_sym) {
            // Todo: is this definitely the generically substituted "new_cls_scope"?
            scope.direct_sup_scopes.emplace_back(new_cls_scope);
        }

        // Check for conflicting "cmp" or "type" statements in the super scopes.
        if (sup_scope->ast->to<asts::SupPrototypeExtensionAst>() or sup_scope->ast->to<asts::SupPrototypeFunctionsAst>()) {
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
    const auto existing_scopes = cls_sym.scope->direct_sup_scopes
        | genex::views::filter([&](auto *scope) { return scope->ast->template to<asts::SupPrototypeExtensionAst>() or scope->ast->template to<asts::SupPrototypeFunctionsAst>(); })
        | genex::views::filter([&](auto *scope) { return utils::type_utils::relaxed_symbolic_eq(*ast_name(sup_scope.ast), *ast_name(scope->ast), sup_scope, *scope->ast->get_ast_scope(), dummy); })
        | genex::to<std::vector>();

    // Check for conflicting "type" statements.
    std::vector<std::shared_ptr<asts::TypeIdentifierAst>> new_types;
    for (auto const *scope : existing_scopes) {
        auto body = asts::ast_body(scope->ast);
        for (auto const *member : body) {
            if (auto const *type_stmt = member->to<asts::TypeStatementAst>(); type_stmt != nullptr) {
                for (auto const &new_type : new_types) {
                    raise_if<errors::SppIdentifierDuplicateError>(
                        *new_type == *type_stmt->new_type, {scope, &sup_scope},
                        ERR_ARGS(*new_type, *type_stmt->new_type, "associated type"));
                }
                new_types.emplace_back(type_stmt->new_type);
            }
        }
    }

    // Check for conflicting "cmp" statements.
    std::vector<std::shared_ptr<asts::IdentifierAst>> new_cmps;
    for (const auto *scope : existing_scopes) {
        auto body = asts::ast_body(scope->ast);
        for (auto const *member : body) {
            if (auto const *cmp_stmt = member->to<asts::CmpStatementAst>(); cmp_stmt != nullptr and cmp_stmt->type->type_parts().back()->name[0] != '$') {
                for (auto const &new_cmp : new_cmps) {
                    raise_if<errors::SppIdentifierDuplicateError>(
                        *new_cmp == *cmp_stmt->name, {scope, &sup_scope},
                        ERR_ARGS(*new_cmp, *cmp_stmt->name, "comptime constant"));
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
