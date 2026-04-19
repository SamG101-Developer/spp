module spp.analyse.scopes;
import spp.analyse.errors;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.scope_utils;
import spp.asts;
import spp.asts.utils;
import spp.compiler;
import spp.utils.error_formatter;
import spp.utils.functions;
import ankerl;
import genex;


spp::analyse::scopes::ScopeBlockName::ScopeBlockName(
    std::string &&name) :
    name(std::move(name)) {
}


auto spp::analyse::scopes::ScopeBlockName::from_parts(
    std::string &&header,
    std::vector<AbstractAst*> const &parts,
    const std::size_t pos)
    -> ScopeBlockName {
    // Build the name string.
    auto builder = std::string();
    builder.append("<").append(header);
    for (auto const &part : parts) {
        builder.append("#").append(part->to_string());
    }
    builder.append("#").append(std::to_string(pos));
    builder.append(">");
    return ScopeBlockName(std::move(builder));
}


spp::analyse::scopes::Scope::Scope(
    ScopeName name,
    Scope *parent,
    AbstractAst *ast,
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
    // Copy the children recursively.
    for (auto const &child_scope : other.children) {
        auto child_copy = std::make_unique<Scope>(*child_scope);
        child_copy->parent = this;
        children.emplace_back(std::move(child_copy));
    }
}


spp::analyse::scopes::Scope::~Scope() = default;


auto spp::analyse::scopes::Scope::new_global(
    compiler::Module const &mod)
    -> std::unique_ptr<Scope> {
    // Create a new global scope (no parent or ast for the global scope).
    auto scope_name = ScopeBlockName::from_parts(
        "global", {}, 0);
    auto glob_scope = std::make_unique<Scope>(
        std::move(scope_name), nullptr, nullptr, mod.error_formatter.get());

    // Inject the "_global" namespace symbol into this scope (makes lookups orthogonal).
    auto glob_ns_sym_name = std::make_unique<asts::IdentifierAst>(0, "_global");
    auto glob_ns_sym = std::make_unique<NamespaceSymbol>(
        std::move(glob_ns_sym_name), glob_scope.get());
    glob_scope->ns_sym = std::move(glob_ns_sym);

    // Return the global scope.
    return glob_scope;
}


auto spp::analyse::scopes::Scope::get_error_formatter() const
    -> spp::utils::errors::ErrorFormatter* {
    // Return this scope's error formatter, or the parent's if it doesn't exist.
    const auto this_formatter = m_error_formatter;
    return this_formatter != nullptr ? this_formatter : parent->get_error_formatter();
}


auto spp::analyse::scopes::Scope::depth_difference(
    const Scope *scope) const
    -> sys::ssize_t {
    // Create an internal function to call recursively with a counter.
    auto func = [](this auto &&self, const Scope *source, const Scope *target, const sys::ssize_t depth) -> sys::ssize_t {
        if (source == target) { return depth; }
        for (auto const *sup_scope : source->direct_sup_scopes) {
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
    if (children.empty()) { return this; }

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
    -> Scope* {
    // Get the parent module scope, if it exists.
    for (auto *scope = this; scope != nullptr; scope = scope->parent) {
        if (std::holds_alternative<ScopeIdentifierName>(scope->name)) {
            return const_cast<Scope*>(scope); // TODO: REMOVE CONST CAST
        }
    }
    return nullptr;
}


auto spp::analyse::scopes::Scope::sup_scopes() const
    -> std::vector<Scope*> {
    // Get all super scopes, recursively.
    auto scopes = std::vector<Scope*>();
    for (auto *scope : direct_sup_scopes) {
        const auto child_scopes = scope->sup_scopes();
        scopes.push_back(scope);
        scopes.append_range(child_scopes);
    }
    return scopes;
}


auto spp::analyse::scopes::Scope::sup_types() const
    -> std::vector<std::shared_ptr<AbstractAst>> {
    // Get all super types, recursively (filter and map the super scopes).
    return sup_scopes()
        | genex::views::filter([](auto *scope) { return scope->ast->template to<asts::ClassPrototypeAst>(); })
        | genex::views::transform([](auto *scope) { return utils::scope_utils::associated_type_symbol(*scope)->fq_name(); })
        | genex::views::cast_smart<AbstractAst>()
        | genex::to<std::vector>();
}


auto spp::analyse::scopes::Scope::direct_sup_types() const
    -> std::vector<std::shared_ptr<AbstractAst>> {
    // Get all direct super types (filter and map the direct super scopes).
    return direct_sup_scopes
        | genex::views::filter([](auto *scope) { return scope->ast->template to<asts::ClassPrototypeAst>(); })
        | genex::views::transform([](auto *scope) { return utils::scope_utils::associated_type_symbol(*scope)->fq_name(); })
        | genex::views::cast_smart<AbstractAst>()
        | genex::to<std::vector>();
}


auto spp::analyse::scopes::Scope::convert_postfix_to_nested_scope(
    AbstractAst const *postfix_ast) const
    -> Scope const* {
    // Get the left-hand-side namespace's member's type.
    if (const auto lhs_as_ident = postfix_ast->to<asts::IdentifierAst>()) {
        const auto ns_sym = utils::scope_utils::get_ns_symbol(*this, asts::ast_clone(lhs_as_ident));
        return ns_sym ? ns_sym->scope : nullptr;
    }

    // Postfix lhs -> get the ns scopes.
    auto lhs = postfix_ast;
    auto namespaces = std::vector<asts::IdentifierAst*>();
    while (auto const *postfix_lhs = lhs->to<asts::PostfixExpressionAst>()) {
        const auto op = postfix_lhs->op->to<asts::PostfixExpressionOperatorStaticMemberAccessAst>();
        namespaces.emplace_back(op->name->to<asts::IdentifierAst>());
        lhs = postfix_lhs->lhs.get();
    }
    if (auto *lhs_as_ident = lhs->to<asts::IdentifierAst>()) {
        // todo: is the condition requires? just body.
        namespaces.emplace_back(const_cast<asts::IdentifierAst*>(lhs_as_ident));
    }

    auto scope = this;
    for (auto const *ns : namespaces | genex::views::reverse) {
        const auto ns_sym = utils::scope_utils::get_ns_symbol(*scope, asts::ast_clone(ns));
        scope = ns_sym ? ns_sym->scope : nullptr;
        if (scope == nullptr) { break; }
    }
    return scope;
}


auto spp::analyse::scopes::Scope::print_scope_tree() const
    -> std::string {
    // Indent the children, print the scope name.
    auto func = [](this auto &&self, Scope const *scope, std::string const &indent) -> std::string {
        auto result = indent + std::visit(
            spp::utils::functions::overload{
                [](std::shared_ptr<asts::IdentifierAst> const &id) { return id->val; },
                [](std::shared_ptr<asts::TypeIdentifierAst> const &id) { return id->name; },
                [](ScopeBlockName const &block) { return block.name; }
            }, scope->name) + "\n";

        for (auto child : scope->children | genex::views::ptr) {
            result += self(child, indent + "    ");
        }
        return result;
    };

    return func(this, "");
}


auto spp::analyse::scopes::Scope::name_as_string() const
    -> std::string {
    if (auto const name_as_id = std::get_if<std::shared_ptr<asts::IdentifierAst>>(&name)) {
        return name_as_id->to_string();
    }
    if (auto const name_as_type_id = std::get_if<std::shared_ptr<asts::TypeIdentifierAst>>(&name)) {
        return name_as_type_id->to_string();
    }
    if (auto const name_as_block = std::get_if<ScopeBlockName>(&name)) {
        return name_as_block->name;
    }
    std::unreachable();
}


auto spp::analyse::scopes::Scope::fix_children_parent_pointers()
    -> void {
    // Iterate all children, setting their parent pointer to this scope. Recurse into them.
    for (auto const &child : children) {
        child->parent = this;
        child->fix_children_parent_pointers();
    }
}
