module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.scope_utils;
import spp.analyse.errors;
import spp.asts;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import genex;


auto spp::analyse::utils::scope_utils::add_var_symbol(
    scopes::Scope &scope,
    std::shared_ptr<scopes::VariableSymbol> const &sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    scope.table.var_tbl.add(sym->name->val, sym);
}


auto spp::analyse::utils::scope_utils::add_var_symbol_check_conflict(
    scopes::Scope &scope,
    std::shared_ptr<scopes::VariableSymbol> const &sym)
    -> void {
    // Cannot allow for duplicate comptime definitions.
    const auto existing_sym = get_var_symbol(scope, sym->name, false);
    if (existing_sym != nullptr) {
        // const auto is_generic = sym->is_generic;
        // const auto is_comptime = sym->memory_info->ast_comptime != nullptr;
        const auto is_functional = existing_sym->type && existing_sym->type->to_string()[0] == '$';
        raise_if<errors::SppIdentifierDuplicateError>(
            not is_functional, {&scope, &scope},
            ERR_ARGS(*existing_sym->name, *sym->name, "comptime variable identifier"));
    }

    // Add a type symbol to the corresponding symbol table.
    scope.table.var_tbl.add(sym->name->val, sym);
}


auto spp::analyse::utils::scope_utils::add_type_symbol(
    scopes::Scope &scope,
    std::shared_ptr<scopes::TypeSymbol> const &sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    scope.table.type_tbl.add(sym->name->to_string(), sym);
}



auto spp::analyse::utils::scope_utils::add_type_symbol_check_conflict(
    scopes::Scope &scope,
    std::shared_ptr<scopes::TypeSymbol> const &sym)
    -> void {
    // Cannot allow for duplicate definitions.
    const auto existing_sym = get_type_symbol(scope, sym->name, false);
    if (existing_sym != nullptr) {
        const auto is_functional = sym->name->name[0] == '$';
        raise_if<errors::SppIdentifierDuplicateError>(
            not is_functional,
            {existing_sym->scope_defined_in, sym->scope_defined_in},
            ERR_ARGS(*existing_sym->name, *sym->name, "type identifier"));
    }

    // Add a type symbol to the corresponding symbol table.
    scope.table.type_tbl.add(sym->name->to_string(), sym);
}


auto spp::analyse::utils::scope_utils::add_ns_symbol(
    scopes::Scope &scope,
    std::shared_ptr<scopes::NamespaceSymbol> const &sym)
    -> void {
    // Add a namespace symbol to the corresponding symbol table.
    scope.table.ns_tbl.add(sym->name->val, sym);
}


auto spp::analyse::utils::scope_utils::rem_var_symbol(
    scopes::Scope &scope,
    std::shared_ptr<asts::IdentifierAst> const &sym_name)
    -> void {
    // Remove a variable symbol from the corresponding symbol table.
    scope.table.var_tbl.rem(sym_name->val);
}


auto spp::analyse::utils::scope_utils::rem_type_symbol(
    scopes::Scope &scope,
    std::shared_ptr<asts::TypeIdentifierAst> const &sym_name)
    -> void {
    // Remove a type symbol from the corresponding symbol table.
    scope.table.type_tbl.rem(sym_name->to_string());
}


auto spp::analyse::utils::scope_utils::rem_ns_symbol(
    scopes::Scope &scope,
    std::shared_ptr<asts::IdentifierAst> const &sym_name)
    -> void {
    // Remove a namespace symbol from the corresponding symbol table.
    scope.table.ns_tbl.rem(sym_name->to_string());
}


auto spp::analyse::utils::scope_utils::all_var_symbols(
    scopes::Scope const &scope,
    const bool exclusive,
    const bool sup_scope_search)
    -> std::vector<std::shared_ptr<scopes::VariableSymbol>> {
    // Yield all symbols from the var symbol table.
    auto syms = scope.table.var_tbl.all();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and scope.parent != nullptr) {
        syms.append_range(all_var_symbols(scope.parent, exclusive, sup_scope_search));
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : scope.direct_sup_scopes) {
            syms.append_range(sup_scope->all_var_symbols(true, false));
        }
    }

    return syms;
}


auto spp::analyse::utils::scope_utils::all_type_symbols(
    scopes::Scope const &scope,
    const bool exclusive,
    const bool sup_scope_search)
    -> std::vector<std::shared_ptr<scopes::TypeSymbol>> {
    // Yield all symbols from the type symbol table.
    auto syms = scope.table.type_tbl.all();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and scope.parent != nullptr) {
        syms.append_range(all_type_symbols(scope.parent, exclusive, sup_scope_search));
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : scope.direct_sup_scopes) {
            syms.append_range(sup_scope->all_type_symbols(true, false));
        }
    }

    return syms;
}


auto spp::analyse::utils::scope_utils::all_ns_symbols(
    scopes::Scope const &scope,
    const bool exclusive,
    bool)
    -> std::vector<std::shared_ptr<scopes::NamespaceSymbol>> {
    auto syms = scope.table.ns_tbl.all();

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and scope.parent != nullptr) {
        syms.append_range(all_ns_symbols(scope.parent, exclusive));
    }
    return syms;
}



auto spp::analyse::utils::scope_utils::has_var_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<asts::IdentifierAst> const &sym_name,
    const bool exclusive)
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_var_symbol(scope, sym_name, exclusive) != nullptr;
}


auto spp::analyse::utils::scope_utils::has_type_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<asts::TypeAst> const &sym_name,
    const bool exclusive)
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_type_symbol(scope, sym_name, exclusive) != nullptr;
}


auto spp::analyse::utils::scope_utils::has_ns_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<asts::IdentifierAst> const &sym_name,
    const bool exclusive)
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_ns_symbol(scope, sym_name, exclusive) != nullptr;
}


auto spp::analyse::utils::scope_utils::get_var_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<asts::IdentifierAst> const &sym_name,
    const bool exclusive,
    const bool sup_scope_search)
    -> std::shared_ptr<VariableSymbol> {
    // Get the symbol from the symbol table if it exists.
    if (sym_name == nullptr) { return nullptr; }
    auto sym = table.var_tbl.get(sym_name);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_var_symbol(sym_name, exclusive);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr and sup_scope_search) {
        sym = search_sup_scopes_for_var(*scope, sym_name);
    }

    // Check for a linked aliased variable symbol.
    if (sym != nullptr and sym->alias_sym != nullptr) {
        sym = sym->alias_sym;
    }

    // Return the found symbol, or nullptr.
    return sym;
}


auto spp::analyse::utils::scope_utils::get_type_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<const asts::TypeAst> const &sym_name,
    const bool exclusive,
    const bool sup_scope_search)
    -> std::shared_ptr<TypeSymbol> {
    // Adjust the scope for the namespace of the type identifier if there is one.
    if (sym_name == nullptr) { return nullptr; }

    // Check cache.
    if (sym_name->cached_type_symbols.contains(this)) {
        return sym_name->cached_type_symbols.get(this);
    }

    std::shared_ptr<const asts::TypeIdentifierAst> sym_name_extracted;
    if (const auto sym_name_as_identifier = std::dynamic_pointer_cast<const asts::TypeIdentifierAst>(sym_name)) {
        sym_name_extracted = std::const_pointer_cast<asts::TypeIdentifierAst>(sym_name_as_identifier);
    }
    else {
        auto [scope_, sym_name_extracted_] = shift_scope_for_namespaced_type(*this, *sym_name);
        scope = scope_;
        sym_name_extracted = sym_name_extracted_;
    }

    // Get the symbol from the symbol table if it exists.
    auto sym = scope->table.type_tbl.get(
        std::const_pointer_cast<asts::TypeIdentifierAst>(sym_name_extracted));

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_type_symbol(sym_name_extracted, exclusive);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr and sup_scope_search) {
        sym = search_sup_scopes_for_type(*scope, sym_name_extracted);
    }

    // Update cache and return the found symbol, or nullptr.
    if (sym != nullptr) {
        sym_name->cached_type_symbols.set(this, sym);
    }
    return sym;
}


auto spp::analyse::utils::scope_utils::get_ns_symbol(
    scopes::Scope const &scope,
    std::shared_ptr<const asts::IdentifierAst> const &sym_name,
    const bool exclusive)
    -> std::shared_ptr<NamespaceSymbol> {
    // Get the symbol from the symbol table if it exists.
    if (sym_name == nullptr) { return nullptr; }
    auto sym = table.ns_tbl.get(
        std::const_pointer_cast<asts::IdentifierAst>(sym_name));

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_ns_symbol(sym_name, exclusive);
    }

    // Return the found symbol, or nullptr.
    return sym;
}


auto spp::analyse::utils::scope_utils::get_var_symbol_outermost(
    asts::Ast const &expr) const
    -> std::pair<std::shared_ptr<scopes::VariableSymbol>, scopes::Scope const*> {
    // Define helper methods to check expression types.
    auto is_valid_postfix_expression = []<typename OpType>(auto *ast) -> bool {
        auto postfix_expr = ast->template to<asts::PostfixExpressionAst>();
        if (postfix_expr == nullptr) {
            return false;
        }

        auto postfix_op = postfix_expr->op->template to<OpType>();
        return postfix_op != nullptr;
    };

    auto is_valid_postfix_expression_runtime = [is_valid_postfix_expression](auto *ast) -> bool {
        return is_valid_postfix_expression.operator()<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(ast);
    };

    auto is_valid_postfix_expression_static = [is_valid_postfix_expression](auto *ast) -> bool {
        return is_valid_postfix_expression.operator()<asts::PostfixExpressionOperatorStaticMemberAccessAst>(ast);
    };

    auto adjusted_name = &expr;
    if (is_valid_postfix_expression_runtime(&expr)) {
        // Keep moving into the left-hand-side until there is no left-hand-side: "a.b.c" becomes "a".
        while (is_valid_postfix_expression_runtime(adjusted_name)) {
            adjusted_name = adjusted_name->to<asts::PostfixExpressionAst>()->lhs.get();
        }

        // Get the symbol (will be in this scope), and return it with the scope.
        auto sym = get_var_symbol(asts::ast_clone(adjusted_name->to<asts::IdentifierAst>()));
        return std::make_pair(sym, this);
    }

    if (is_valid_postfix_expression_static(&expr)) {
        // This is possible with a left-hand-side type or namespace.
        const auto postfix_expr = expr.to<asts::PostfixExpressionAst>();
        const auto postfix_op = postfix_expr->op->to<asts::PostfixExpressionOperatorStaticMemberAccessAst>();

        // Type based left-hand-side, such as "some_namespace::Type::static_member()"
        if (const auto type_lhs = postfix_expr->lhs->to<asts::TypeAst>()) {
            const auto type_sym = get_type_symbol(asts::ast_clone(type_lhs));
            const auto var_sym = type_sym->scope->get_var_symbol(postfix_op->name);
            return std::make_pair(var_sym, type_sym->scope);
        }

        // Namespace based left-hand-side, such as "a::b::c::my_function()"
        auto namespace_scope = this;
        if (is_valid_postfix_expression_static(adjusted_name)) {
            adjusted_name = adjusted_name->to<asts::PostfixExpressionAst>()->lhs.get();
            namespace_scope = namespace_scope->convert_postfix_to_nested_scope(adjusted_name->to<asts::ExpressionAst>());
        }
        auto sym = namespace_scope ? namespace_scope->get_var_symbol(postfix_op->name) : nullptr;
        return std::make_pair(sym, namespace_scope);
    }

    // Identifiers or non-symbolic expressions can use the normal lookup.
    auto sym = get_var_symbol(asts::ast_clone(adjusted_name->to<asts::IdentifierAst>()));
    return std::make_pair(sym, this);
}



auto spp::analyse::utils::scope_utils::get_scope_generics(
    scopes::Scope const &scope) const
    -> std::vector<std::unique_ptr<asts::GenericArgumentAst>> {
    // Create the symbols list.
    auto syms = std::vector<std::unique_ptr<asts::GenericArgumentAst>>();
    const auto scopes = ancestors();
    auto type_names = std::vector<std::shared_ptr<asts::TypeIdentifierAst>>();
    auto comp_names = std::vector<std::shared_ptr<asts::IdentifierAst>>();

    // Check each ancestor scope, accumulating generic symbols.
    for (auto const *scope : scopes) {
        auto all_type_syms = scope->all_type_symbols(true)
            | genex::views::filter([](auto const &sym) { return sym->is_generic; })
            | genex::to<std::vector>();

        for (auto &&t : all_type_syms) {
            if (genex::contains(type_names, *t->name, genex::meta::deref)) { continue; }
            syms.emplace_back(asts::GenericArgumentTypeKeywordAst::from_symbol(*t));
            type_names.emplace_back(t->name);
        }

        auto all_var_syms = scope->all_var_symbols(true)
            | genex::views::filter([](auto const &sym) { return sym->is_generic; })
            | genex::to<std::vector>();

        for (auto &&v : all_var_syms) {
            if (genex::contains(comp_names, *v->name, genex::meta::deref)) { continue; }
            syms.emplace_back(asts::GenericArgumentCompKeywordAst::from_symbol(*v));
            comp_names.emplace_back(v->name);
        }
    }

    // Return the list of generic symbols.
    return syms;
}


auto spp::analyse::utils::scope_utils::get_scope_extended_generic_symbols(
    scopes::Scope const &scope,
    std::vector<asts::GenericArgumentAst*> const &generics,
    std::shared_ptr<asts::TypeAst> const &ignore) const
    -> std::vector<std::shared_ptr<scopes::Symbol>> {
    // Convert the provided generic arguments into symbols. Todo: filter to "is_generic"?
    const auto type_syms = generics
        | genex::views::cast_dynamic<asts::GenericArgumentTypeAst*>()
        | genex::views::transform([this](auto &&gen_arg) { return get_type_symbol(gen_arg->val); })
        | genex::views::filter([](auto &&sym) { return sym != nullptr and sym->is_generic; })
        | genex::views::cast_smart<Symbol>()
        | genex::to<std::vector>();

    const auto comp_syms = generics
        | genex::views::cast_dynamic<asts::GenericArgumentCompAst*>()
        | genex::views::filter([&ignore](auto &&gen_arg) { return ignore == nullptr or *gen_arg->val != *ignore; })
        | genex::views::transform([this](auto &&gen_arg) { return get_var_symbol(asts::ast_clone(gen_arg->val->template to<asts::IdentifierAst>())); })
        | genex::views::filter([](auto &&sym) { return sym != nullptr and sym->is_generic; })
        | genex::views::cast_smart<Symbol>()
        | genex::to<std::vector>();

    auto syms = type_syms;
    syms.append_range(comp_syms);

    // Re-use above logic to collect generic symbols from the ancestor scopes.
    const auto scopes = ancestors()
        | genex::views::take_while([](auto *scope) { return not std::holds_alternative<std::shared_ptr<asts::IdentifierAst>>(scope->name); })
        | genex::to<std::vector>();

    for (auto const *scope : scopes) {
        for (auto const &sym : scope->all_type_symbols(true)
             | genex::views::filter([](auto const &s) { return s->is_generic; })) {
            syms.emplace_back(sym);
        }

        for (auto const &sym : scope->all_var_symbols(true)
             | genex::views::filter([](auto const &s) { return s->is_generic; })
             | genex::views::filter([&ignore](auto const &s) { return ignore == nullptr or *s->name == *ignore; })) {
            syms.emplace_back(sym);
        }
    }

    // Return the full list of generic symbols.
    return syms;
}


auto spp::analyse::utils::scope_utils::search_sup_scopes_for_var(
    scopes::Scope const &scope,
    std::shared_ptr<asts::IdentifierAst> const &name)
    -> std::shared_ptr<scopes::VariableSymbol> {
    // Recursively search the super scopes for a variable symbol.
    for (auto const *sup_scope : scope.direct_sup_scopes) {
        if (auto sym = get_var_symbol(*sup_scope, name, true); sym != nullptr) { return sym; }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::scope_utils::search_sup_scopes_for_type(
    scopes::Scope const &scope,
    std::shared_ptr<const asts::TypeAst> const &name)
    -> std::shared_ptr<scopes::TypeSymbol> {
    // Recursively search the super scopes for a type symbol.
    for (auto const *sup_scope : scope.direct_sup_scopes) {
        if (auto sym = get_type_symbol(*sup_scope, name, true); sym != nullptr) { return sym; }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::scope_utils::shift_scope_for_namespaced_type(
    scopes::Scope const &scope,
    asts::TypeAst const &fq_type)
    -> std::pair<const scopes::Scope*, std::shared_ptr<const asts::TypeIdentifierAst>> {
    // Get the namespace and type parts, to get the scopes.
    const auto ns_parts = fq_type.ns_parts();
    const auto type_parts = fq_type.type_parts();
    auto shifted_scope = &scope;

    // Iterate to move through the namespace parts first.
    for (auto &&ns_part : ns_parts) {
        const auto sym = get_ns_symbol(*shifted_scope, ns_part);
        if (sym == nullptr) { break; }
        shifted_scope = sym->scope;
    }

    // Iterate through the type parts (except the final one) next.
    for (auto &&type_part : type_parts | genex::views::drop_last(1)) {
        const auto sym = get_type_symbol(*shifted_scope, type_part);
        if (sym == nullptr or sym->is_generic) { break; }
        shifted_scope = sym->scope;
    }

    // Return the type scope, and the final type part.
    auto final = type_parts.back();
    return std::make_pair(shifted_scope, std::move(final));
}




auto spp::analyse::utils::scope_utils::attach_llvm_type_info(
    asts::ModulePrototypeAst const &mod,
    codegen::LlvmCtx *ctx)
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
            const auto llvm_type = codegen::llvm_type(
                *associated_type_symbol(*cls_proto->get_ast_scope()), ctx);
            for (auto const &alias_sym : cls_proto->get_ast_scope()->ty_sym->aliased_by_symbols) {
                alias_sym->llvm_info->llvm_type = llvm_type;
            }
        }

        // All concrete generic implementations (not std::vector::Vec[T]).
        // Todo: don't generate when one of the generics is "comp->identifier" or "type->generic"
        for (auto const &generic_sub : cls_proto->registered_generic_substitutions()) {
            codegen::register_llvm_type_info(generic_sub.second, ctx);

            // All generic aliases need llvm type info propagated from their aliased types.
            const auto llvm_type = codegen::llvm_type(
                *associated_type_symbol(*generic_sub.second->get_ast_scope()), ctx);
            for (auto const &alias_sym : generic_sub.second->get_ast_scope()->ty_sym->aliased_by_symbols) {
                alias_sym->llvm_info->llvm_type = llvm_type;
            }
        }
    }
}



auto spp::analyse::utils::scope_utils::check_conflicting_type_or_cmp_statements(
    scopes::TypeSymbol const &cls_sym,
    scopes::Scope const &sup_scope)
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

