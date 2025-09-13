#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/symbol_table.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/compiler/module_tree.hpp>
#include <spp/utils/variants.hpp>

#include <genex/actions/concat.hpp>
#include <genex/actions/push_back.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/drop_last.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/take_until.hpp>
#include <genex/views/to.hpp>
#include <genex/views/transform.hpp>


spp::analyse::scopes::ScopeBlockName::ScopeBlockName(std::string &&name) : name(std::move(name)) {
}


spp::analyse::scopes::Scope::Scope(
    ScopeName name,
    Scope *parent,
    asts::Ast *ast,
    spp::utils::errors::ErrorFormatter *error_formatter) :
    name(std::move(name)),
    parent(parent),
    ast(ast),
    ty_sym(nullptr),
    ns_sym(nullptr),
    non_generic_scope(this),
    m_error_formatter(error_formatter) {
}


spp::analyse::scopes::Scope::Scope(Scope const &other) :
    name(other.name),
    parent(other.parent),
    ast(other.ast),
    ty_sym(other.ty_sym),
    ns_sym(other.ns_sym),
    table(other.table),
    non_generic_scope(other.non_generic_scope),
    m_error_formatter(nullptr) {
}


auto spp::analyse::scopes::Scope::new_global(
    compiler::Module const &module)
    -> std::unique_ptr<Scope> {
    // Create a new global scope.
    auto glob_scope_name = ScopeBlockName("<global>");
    auto glob_scope = std::make_unique<Scope>(std::move(glob_scope_name), nullptr);

    // Inject the "_global namespace symbol into this scope (makes lookups orthogonal).
    auto glob_ns_sym_name = std::make_unique<asts::IdentifierAst>(0, "_global");
    auto glob_ns_sym = std::make_unique<NamespaceSymbol>(std::move(glob_ns_sym_name), glob_scope.get());
    glob_scope->ns_sym = std::move(glob_ns_sym);
    glob_scope->ast = module.module_ast.get();

    // Return the global scope.
    return glob_scope;
}


auto spp::analyse::scopes::Scope::search_sup_scopes_for_var(
    Scope const &scope,
    asts::IdentifierAst const &name)
    -> std::shared_ptr<VariableSymbol> {
    // Recursively search the super scopes for a variable symbol.
    for (const auto sup_scope : scope.m_direct_sup_scopes) {
        if (auto const sym = sup_scope->get_var_symbol(name, true); sym != nullptr) {
            return sym;
        }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::scopes::Scope::search_sup_scopes_for_type(
    Scope const &scope,
    asts::TypeAst const &name,
    const bool ignore_alias)
    -> std::shared_ptr<TypeSymbol> {
    // Recursively search the super scopes for a type symbol.
    for (const auto sup_scope : scope.m_direct_sup_scopes) {
        if (auto const sym = sup_scope->get_type_symbol(name, true, ignore_alias); sym != nullptr) {
            return sym;
        }
    }

    // No symbol was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::scopes::Scope::shift_scope_for_namespaced_type(
    Scope const &scope,
    asts::TypeAst const &fq_type)
    -> std::pair<const Scope*, std::shared_ptr<const asts::TypeIdentifierAst>> {
    // Get the namespace and type parts, to get the scopes.
    const auto ns_parts = fq_type.ns_parts();
    const auto type_parts = fq_type.type_parts();
    auto shifted_scope = &scope;

    // Iterate to move through the namespace parts first.
    for (auto &&ns_part : ns_parts) {
        const auto sym = shifted_scope->get_ns_symbol(*ns_part);
        if (sym == nullptr) { break; }
        shifted_scope = sym->scope;
    }

    // Iterate through the type parts (except the final one) next.
    for (auto &&type_part : type_parts | genex::views::drop_last(1)) {
        const auto sym = shifted_scope->get_type_symbol(*type_part);
        if (sym == nullptr or sym->scope == nullptr) { break; }
        shifted_scope = sym->scope;
    }

    // Return the type scope, and the final type part.
    return std::make_pair(shifted_scope, type_parts.back());
}


auto spp::analyse::scopes::Scope::get_error_formatter() const
    -> spp::utils::errors::ErrorFormatter* {
    // Return this scope's error formatter, or the parent's if it doesn't exist.
    const auto this_formatter = m_error_formatter;
    return this_formatter != nullptr ? this_formatter : parent->get_error_formatter();
}


auto spp::analyse::scopes::Scope::get_generics() const
    -> std::vector<std::unique_ptr<asts::GenericArgumentAst>> {
    // Create the symbols list.
    auto syms = std::vector<std::unique_ptr<asts::GenericArgumentAst>>();

    auto scopes = ancestors()
        | genex::views::take_until([](auto &&scope) { return std::holds_alternative<asts::IdentifierAst*>(scope->name); });

    // Check each ancestor scope, accumulating generic symbols.
    for (auto &&scope : scopes) {
        // Type symbols must be generic and have a "scope" ie the generic points to a concrete type.
        scope->all_type_symbols(true)
            | genex::views::filter([](auto &&sym) { return sym->is_generic and sym->scope != nullptr; })
            | genex::views::for_each([&syms](auto const&sym) { syms.emplace_back(asts::GenericArgumentTypeKeywordAst::from_symbol(*sym)); });

        // Comp symbols must be generic.
        scope->all_var_symbols(true)
            | genex::views::filter([](auto &&sym) { return sym->is_generic; })
            | genex::views::for_each([&syms](auto const&sym) { syms.emplace_back(asts::GenericArgumentCompKeywordAst::from_symbol(*sym)); });
    }

    // Return the list of generic symbols.
    return syms;
}


auto spp::analyse::scopes::Scope::get_extended_generic_symbols(
    std::vector<asts::GenericArgumentAst*> generics)
    -> std::vector<std::shared_ptr<Symbol>> {
    // Convert the provided generic arguments into symbols.
    const auto type_syms = generics
        | genex::views::cast_dynamic<asts::GenericArgumentTypeAst*>()
        | genex::views::transform([this](auto &&gen_arg) { return get_type_symbol(*gen_arg->val); })
        | genex::views::cast_smart_ptr<Symbol>()
        | genex::views::to<std::vector>();

    const auto comp_syms = generics
        | genex::views::cast_dynamic<asts::GenericArgumentCompAst*>()
        | genex::views::transform([this](auto &&gen_arg) { return get_var_symbol(ast_cast<asts::IdentifierAst>(*gen_arg->val)); })
        | genex::views::cast_smart_ptr<Symbol>()
        | genex::views::to<std::vector>();

    auto syms = genex::views::concat(type_syms, comp_syms)
        | genex::views::to<std::vector>();

    // Re-use above logic to collect generic symbols from the ancestor scopes.
    auto scopes = ancestors()
        | genex::views::take_until([](auto *scope) { return std::holds_alternative<asts::IdentifierAst*>(scope->name); })
        | genex::views::to<std::vector>();

    for (auto *scope : scopes) {
        scope->all_type_symbols(true)
            | genex::views::filter([](auto const &sym) { return sym->is_generic and sym->scope != nullptr; })
            | genex::views::for_each([&syms](auto const &sym) { syms.emplace_back(sym); });

        scope->all_var_symbols(true)
            | genex::views::filter([](auto const &sym) { return sym->is_generic; })
            | genex::views::for_each([&syms](auto const &sym) { syms.emplace_back(sym); });
    }

    // Return the full list of generic symbols.
    return syms;
}


auto spp::analyse::scopes::Scope::add_var_symbol(
    std::shared_ptr<VariableSymbol> const &sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    table.var_tbl.add(sym->name.get(), std::move(sym));
}


auto spp::analyse::scopes::Scope::add_type_symbol(
    std::shared_ptr<TypeSymbol> const &sym)
    -> void {
    // Add a type symbol to the corresponding symbol table.
    table.type_tbl.add(sym->name.get(), sym);
}


auto spp::analyse::scopes::Scope::add_ns_symbol(
    std::shared_ptr<NamespaceSymbol> const &sym)
    -> void {
    // Add a namespace symbol to the corresponding symbol table.
    table.ns_tbl.add(sym->name.get(), sym);
}


auto spp::analyse::scopes::Scope::rem_var_symbol(
    asts::IdentifierAst const &sym_name)
    -> void {
    // Remove a variable symbol from the corresponding symbol table.
    table.var_tbl.rem(&sym_name);
}


auto spp::analyse::scopes::Scope::rem_type_symbol(
    asts::TypeIdentifierAst const &sym_name)
    -> void {
    // Remove a type symbol from the corresponding symbol table.
    table.type_tbl.rem(&sym_name);
}


auto spp::analyse::scopes::Scope::rem_ns_symbol(
    asts::IdentifierAst const &sym_name)
    -> void {
    // Remove a namespace symbol from the corresponding symbol table.
    table.ns_tbl.rem(&sym_name);
}


auto spp::analyse::scopes::Scope::all_var_symbols(
    const bool exclusive,
    const bool sup_scope_search) const
    -> std::generator<std::shared_ptr<VariableSymbol>> {
    // Yield all symbols from the var symbol table.
    for (auto sym : table.var_tbl.all()) {
        co_yield sym;
    }

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and parent != nullptr) {
        for (auto sym : parent->all_var_symbols(exclusive, sup_scope_search)) {
            co_yield sym;
        }
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : m_direct_sup_scopes) {
            for (auto sym : sup_scope->all_var_symbols(true, false)) {
                co_yield sym;
            }
        }
    }
}


auto spp::analyse::scopes::Scope::all_type_symbols(
    const bool exclusive,
    const bool sup_scope_search) const
    -> std::generator<std::shared_ptr<TypeSymbol>> {
    // Yield all symbols from the type symbol table.
    for (auto sym : table.type_tbl.all()) {
        co_yield sym;
    }

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and parent != nullptr) {
        for (auto sym : parent->all_type_symbols(exclusive, sup_scope_search)) {
            co_yield sym;
        }
    }

    // For super scope searches, yield from all direct super scopes.
    if (sup_scope_search) {
        for (auto const *sup_scope : m_direct_sup_scopes) {
            for (auto sym : sup_scope->all_type_symbols(true, false)) {
                co_yield sym;
            }
        }
    }
}


auto spp::analyse::scopes::Scope::all_ns_symbols(
    const bool exclusive, bool) const
    -> std::generator<std::shared_ptr<NamespaceSymbol>> {
    // Yield all symbols from the var symbol table.
    for (auto sym : table.ns_tbl.all()) {
        co_yield sym;
    }

    // For non-exclusive searches where a parent is present, yield from the parent scope.
    if (not exclusive and parent != nullptr) {
        for (auto sym : parent->all_ns_symbols(exclusive)) {
            co_yield sym;
        }
    }
}


auto spp::analyse::scopes::Scope::has_var_symbol(
    asts::IdentifierAst const &sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_var_symbol(sym_name, exclusive) != nullptr;
}


auto spp::analyse::scopes::Scope::has_type_symbol(
    asts::TypeAst const &sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_type_symbol(sym_name, exclusive) != nullptr;
}


auto spp::analyse::scopes::Scope::has_ns_symbol(
    asts::IdentifierAst const &sym_name,
    const bool exclusive) const
    -> bool {
    // Check if getting the symbol returns nullptr or not.
    return get_ns_symbol(sym_name, exclusive) != nullptr;
}


auto spp::analyse::scopes::Scope::get_var_symbol(
    asts::IdentifierAst const &sym_name,
    const bool exclusive) const
    -> std::shared_ptr<VariableSymbol> {
    // Get the symbol from the symbol table if it exists.
    const auto scope = this;
    auto sym = table.var_tbl.get(&sym_name);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_var_symbol(sym_name, exclusive);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr) {
        sym = search_sup_scopes_for_var(*scope, sym_name);
    }

    // Return the found symbol, or nullptr.
    return sym;
}


auto spp::analyse::scopes::Scope::get_type_symbol(
    asts::TypeAst const &sym_name,
    const bool exclusive,
    const bool ignore_alias) const
    -> std::shared_ptr<TypeSymbol> {
    // Adjust the scope for the namespace of the type identifier if there is one.
    auto [scope, sym_name_extracted] = shift_scope_for_namespaced_type(*this, *sym_name.without_convention());

    // Get the symbol from the symbol table if it exists.
    auto sym = scope->table.type_tbl.get(sym_name_extracted.get());

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_type_symbol(*sym_name_extracted, exclusive, ignore_alias);
    }

    // If the symbol still hasn't been found, check the super scopes for it.
    if (sym == nullptr) {
        sym = search_sup_scopes_for_type(*scope, *sym_name_extracted, ignore_alias);
    }

    // Handle any possible type aliases; sometimes the original type needs retrieving.
    if (const auto alias_sym = std::dynamic_pointer_cast<AliasSymbol>(sym); alias_sym != nullptr and alias_sym->old_sym and not ignore_alias) {
        return alias_sym->old_sym;
    }

    return sym;
}


auto spp::analyse::scopes::Scope::get_ns_symbol(
    asts::IdentifierAst const &sym_name,
    const bool exclusive) const
    -> std::shared_ptr<NamespaceSymbol> {
    // Get the symbol from the symbol table if it exists.
    const auto scope = this;
    auto sym = table.ns_tbl.get(&sym_name);

    // If the symbol doesn't exist, and this is a non-exclusive search, check the parent scope.
    if (sym == nullptr and not exclusive and scope->parent != nullptr) {
        sym = scope->parent->get_ns_symbol(sym_name, exclusive);
    }

    // Return the found symbol, or nullptr.
    return sym;
}


auto spp::analyse::scopes::Scope::get_var_symbol_outermost(
    asts::Ast const &expr) const
    -> std::pair<std::shared_ptr<VariableSymbol>, Scope const*> {
    // Define helper methods to check expression types.
    auto is_valid_postfix_expression = []<typename OpType>(auto *ast) -> bool {
        auto postfix_expr = asts::ast_cast<asts::PostfixExpressionAst>(ast);
        if (postfix_expr == nullptr) {
            return false;
        }

        auto postfix_op = asts::ast_cast<OpType>(ast);
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
            adjusted_name = asts::ast_cast<asts::PostfixExpressionAst>(&expr)->lhs.get();
        }

        // Get the symbol (will be in this scope), and return it with the scope.
        auto sym = get_var_symbol(*asts::ast_cast<asts::IdentifierAst>(adjusted_name));
        return std::make_pair(sym, this);
    }

    if (is_valid_postfix_expression_static(&expr)) {
        // This is possible with a left-hand-side type or namespace.
        const auto postfix_expr = asts::ast_cast<asts::PostfixExpressionAst>(&expr);
        const auto postfix_op = asts::ast_cast<asts::PostfixExpressionOperatorStaticMemberAccessAst>(&expr);

        // Type based left-hand-side, such as "some_namespace::Type::static_member()"
        if (const auto type_lhs = asts::ast_cast<asts::TypeAst>(postfix_expr->lhs.get())) {
            const auto type_sym = get_type_symbol(*type_lhs);
            const auto var_sym = type_sym->scope->get_var_symbol(*postfix_op->name);
            return std::make_pair(var_sym, type_sym->scope);
        }

        // Namespace based left-hand-side, such as "a::b::c::my_function()"
        auto namespace_scope = this;
        while (is_valid_postfix_expression_static(adjusted_name)) {
            adjusted_name = asts::ast_cast<asts::PostfixExpressionAst>(adjusted_name)->lhs.get();
            const auto namespace_sym = namespace_scope->get_ns_symbol(*asts::ast_cast<asts::IdentifierAst>(adjusted_name));
            namespace_scope = namespace_sym->scope;
        }
        return std::make_pair(namespace_scope->get_var_symbol(*postfix_op->name), namespace_scope);
    }

    // Identifiers or non-symbolic expressions can use the normal lookup.
    auto sym = get_var_symbol(*asts::ast_cast<asts::IdentifierAst>(adjusted_name));
    return std::make_pair(sym, this);
}


auto spp::analyse::scopes::Scope::depth_difference(
    const Scope *scope) const
    -> ssize_t {
    // Create an internal function to call recursively with a counter.
    auto func = [](this auto &&self, const Scope *source, const Scope *target, const ssize_t depth) -> ssize_t {
        if (source == target) { return depth; }
        for (const auto sup_scope : source->m_direct_sup_scopes) {
            if (const auto result = self(sup_scope, target, depth + 1z); result >= 0z) {
                return result;
            }
        }
        return -1;
    };

    return func(this, scope, 0z);
}


auto spp::analyse::scopes::Scope::final_child_scope() const
    -> Scope const* {
    // If there are no children, return this scope (base case for the recursion).
    if (children.empty()) {
        return this;
    }

    // Otherwise, return the final child scope (recursively searching).
    return children.back()->final_child_scope();
}


auto spp::analyse::scopes::Scope::ancestors() const
    -> std::vector<Scope const*> {
    // Get all ancestor scopes, including this scope, and the global scope.
    auto scopes = std::vector<Scope const*>();
    for (auto *scope = this; scope != nullptr; scope = scope->parent) {
        scopes.emplace_back(scope);
    }
    return scopes;
}


auto spp::analyse::scopes::Scope::parent_module() const
    -> Scope const* {
    // Get the parent module scope, if it exists.
    for (auto *scope = this; scope != nullptr; scope = scope->parent) {
        if (std::holds_alternative<asts::IdentifierAst*>(scope->name)) {
            return scope;
        }
    }
    return nullptr;
}


auto spp::analyse::scopes::Scope::sup_scopes() const
    -> std::vector<Scope*> {
    // Get all super scopes, recursively.
    auto scopes = std::vector<Scope*>();
    for (auto *scope : m_direct_sup_scopes) {
        const auto child_scopes = scope->sup_scopes();
        scopes |= genex::actions::push_back(scope);
        scopes |= genex::actions::concat(child_scopes);
    }
    return scopes;
}


auto spp::analyse::scopes::Scope::sup_types() const
    -> std::vector<std::shared_ptr<asts::TypeAst>> {
    // Get all super types, recursively (filter and map the super scopes).
    return sup_scopes()
        | genex::views::filter([](auto *scope) { return asts::ast_cast<asts::ClassPrototypeAst>(scope->ast); })
        | genex::views::transform([](auto *scope) { return scope->ty_sym->fq_name(); })
        | genex::views::to<std::vector>();
}


auto spp::analyse::scopes::Scope::direct_sup_scopes() const
    -> std::vector<Scope*> {
    // Return the direct super scopes.
    return m_direct_sup_scopes;
}


auto spp::analyse::scopes::Scope::direct_sup_types() const
    -> std::vector<std::shared_ptr<asts::TypeAst>> {
    // Get all direct super types (filter and map the direct super scopes).
    return m_direct_sup_scopes
        | genex::views::filter([](auto *scope) { return asts::ast_cast<asts::ClassPrototypeAst>(scope->ast); })
        | genex::views::transform([](auto *scope) { return scope->ty_sym->fq_name(); })
        | genex::views::to<std::vector>();
}


auto spp::analyse::scopes::Scope::print_scope_tree() const
    -> std::string {
    // Indent the children, print the scope name.
    auto func = [](this auto &&self, Scope const *scope, std::string const &indent) -> std::string {
        auto result = indent + std::visit(
            spp::utils::variants::overload{
                [](asts::IdentifierAst *id) {
                    return id->val;
                },
                [](asts::TypeIdentifierAst *id) {
                    return id->name;
                },
                [](ScopeBlockName const &block) {
                    return block.name;
                }
            }, scope->name) + "\n";

        for (auto child : scope->children | genex::views::ptr) {
            result += self(child, indent + "    ");
        }
        return result;
    };

    return func(this, "");
}
