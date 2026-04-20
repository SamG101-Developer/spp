module spp.asts.utils.monomorphization;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;


auto spp::asts::utils::monomorphization::attach_all_super_scopes(
    analyse::scopes::ScopeManager &sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Ensure the scope manager is at the global scope.
    sm.reset();
    for (auto *scope : sm.iter()) {
        if (scope->ty_sym != nullptr) {
            attach_specific_super_scopes(*scope, meta);
        }
    }
    sm.reset();
}


auto spp::asts::utils::monomorphization::attach_specific_super_scopes(
    analyse::scopes::Scope &scope,
    meta::CompilerMetaData *meta)
    -> void {
    // Handle type symbols.
    if (scope.ty_sym != nullptr) {
        const auto type_sym = analyse::utils::scope_utils::associated_type_symbol(scope);
        const auto non_generic_sym = analyse::utils::scope_utils::get_type_symbol(scope, type_sym->fq_name()->without_generics());
        auto scopes = analyse::utils::scope_utils::normal_sup_blocks[non_generic_sym.get()];
        scopes.append_range(analyse::utils::scope_utils::generic_sup_blocks);
        attach_specific_super_scopes_impl(scope, std::move(scopes), meta);
    }
}


auto spp::asts::utils::monomorphization::attach_specific_super_scopes_impl(
    analyse::scopes::Scope &scope,
    std::vector<analyse::scopes::Scope*> &&sup_scopes,
    meta::CompilerMetaData *meta)
    -> void {
    // Skip "$" identifiers (functions don't have substitutable members and take up lots of time).
    const auto cls_sym = analyse::utils::scope_utils::associated_type_symbol(scope);
    const auto scope_name = cls_sym->fq_name();
    if (scope_name->type_parts().back()->name[0] == '$') {
        return;
    }
    if (analyse::utils::type_utils::is_type_function(*scope_name, scope)) {
        return;
    }
    if (sup_scopes.empty()) {
        return;
    }

    // Clear the sup scopes list.
    scope.direct_sup_scopes.clear();
    const auto fq_type = cls_sym->fq_name();

    // Iterate through all the super scopes and check if the name matches.
    for (auto *sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types (allows for specializations to match bases).
        auto scope_generics_map = analyse::utils::type_utils::GenericInferenceMap();
        if (not analyse::utils::type_utils::relaxed_symbolic_eq(*fq_type, *ast_name(sup_scope->ast), *cls_sym->scope_defined_in, *sup_scope, scope_generics_map)) {
            // Todo: Is this eliminating too many cases?
            // Todo: For example, superimposing a type will be omitted here because the type won't be the same.
            continue;
        }
        auto scope_generics = GenericArgumentGroupAst::from_map(std::move(scope_generics_map));

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<analyse::scopes::Scope*>(nullptr);
        auto new_cls_scope = static_cast<analyse::scopes::Scope*>(nullptr);
        auto sup_sym = static_cast<analyse::scopes::TypeSymbol*>(nullptr);

        // Todo: Is this "if-else" quite correct? 2 conditions in the "if", then no "else if" block.
        if (not scope_generics->args.empty() and not genex::contains(generic_sup_blocks, sup_scope)) {
            const auto external_generics = scope.ty_sym->scope_defined_in->get_extended_generic_symbols(scope_generics->args | genex::views::ptr | genex::to<std::vector>());
            std::tie(new_sup_scope, new_cls_scope) = analyse::utils::type_utils::create_generic_sup_scope(*sup_scope, scope, *scope_generics, external_generics, this, meta);
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }
        else {
            const auto sup_proto = sup_scope->ast->to<SupPrototypeExtensionAst>();
            new_sup_scope = sup_scope;
            new_cls_scope = sup_proto ? scope.get_type_symbol(sup_proto->super_class)->scope : nullptr;
            sup_sym = new_cls_scope ? new_cls_scope->ty_sym.get() : nullptr;
        }

        // Prevent double inheritance, cyclic inheritance and self extension.
        if (const auto ext_ast = sup_scope->ast->to<SupPrototypeExtensionAst>(); ext_ast != nullptr) {
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
        if (sup_scope->ast->to<SupPrototypeExtensionAst>() or sup_scope->ast->to<SupPrototypeFunctionsAst>()) {
            check_conflicting_type_or_cmp_statements(*cls_sym, *sup_scope);
        }
    }
}