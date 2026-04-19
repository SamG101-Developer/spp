module;
#include <spp/macros.hpp>

export module spp.analyse.scopes:scope;
import :scope_block_name;
import :symbol_table;
import spp.abstract;
import spp.compiler.module_tree;
import spp.utils.error_formatter;
import std;
import sys;


namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS using ScopeName = std::variant<
        ScopeBlockName,
        ScopeIdentifierName,
        ScopeTypeIdentifierName>;
}


SPP_EXP_CLS class spp::analyse::scopes::Scope {
public:
    /**
     * The name of the scope. This will be either an @c std::shared_ptr<IdentifierAst> (functions, modules), an
     * @c std::shared_ptr<TypeIdentifierAst> (classes), or a @c ScopeBlockName (blocks: @c case, @c loop, etc). It is
     * stored in a @c std::variant to allow for easy type-safe access to the underlying type.
     */
    ScopeName name;

    /**
     * The parent scope. It is a raw pointer as the parents "own" their children, and the children do not own their
     * parents. The parent will be @c nullptr for the global scope.
     */
    Scope *parent;

    /**
     * The child scopes. These are owned by the parent scope, and are stored as @c std::unique_ptr to ensure proper
     * memory management. This allows for an easy traversal of the scope hierarchy.
     */
    std::vector<std::unique_ptr<Scope>> children;
    std::vector<std::unique_ptr<Scope>> temp;

    /**
     * Top level scopes register their AST with the scope. This is useful for error reporting, as it allows for easy
     * access to the AST node that the scope represents. This will be @c nullptr for non-top level scopes. Typically,
     * the AST will need to be cast back to its original type.
     */
    AbstractAst *ast;

    /**
     * The (potential) type symbol that represents this scope. This will be @c nullptr for non-type scopes (eg
     * functions, modules, blocks).
     */
    std::shared_ptr<AbstractSymbol> ty_sym;

    /**
     * The (potential) namespace symbol that represents this scope. This will be @c nullptr for non-namespace scopes
     * (eg functions, classes, blocks). Note that a namespace is a module.
     */
    std::shared_ptr<AbstractSymbol> ns_sym;

    /**
     * The collection of symbol tables (type table, variable table, namespace table) that belong to this scope.
     * Individual accessors are created for each table, but the tables are consolidated into a wrapper object for ease
     * of management.
     */
    SymbolTable table;

    /**
     * The scope representing the non-generic version of this scope. If this scope isn't a generic substitution, then
     * the non-generic scope is the scope itself. For @c Vec[Str], the non-generic scope is @c Vec.
     */
    Scope *non_generic_scope;

    std::vector<Scope*> direct_sup_scopes;

private:
    utils::errors::ErrorFormatter *m_error_formatter;

public:
    Scope(ScopeName name, Scope *parent, AbstractAst *ast = nullptr, utils::errors::ErrorFormatter *error_formatter = nullptr);

    Scope(Scope const &other);
    ~Scope();

    /**
     * Create a new global scope for the scope manager. This is called once at the start of
     * compilation. A global scope has no parent or corresponding ast; it is the root node of all
     * namespaces. A corresponding namespace symbol is created for the global scope, for uniform
     * handling of namespaces.
     * @param mod The "main" module being compiled.
     * @return
     */
    static auto new_global(compiler::Module const &mod) -> std::unique_ptr<Scope>;

    /**
     * Get the error formatter associated with this scope. Lots of scopes don't have error
     * formatters, so if the formatter doesn't exist, the scope's parent's formatter is used,
     * recursively searching upwards until a module scope is found, which will have an errror
     * foratter.
     * @return The error formatter associated with this scope.
     */
    SPP_ATTR_NODISCARD auto get_error_formatter() const -> utils::errors::ErrorFormatter*;

    auto depth_difference(const Scope *scope) const -> sys::ssize_t;

    SPP_ATTR_NODISCARD auto final_child_scope() const -> Scope const*;

    SPP_ATTR_NODISCARD auto ancestors() const -> std::vector<Scope const*>;

    SPP_ATTR_NODISCARD auto parent_module() const -> Scope*;

    SPP_ATTR_NODISCARD auto sup_scopes() const -> std::vector<Scope*>;

    SPP_ATTR_NODISCARD auto sup_types() const -> std::vector<std::shared_ptr<AbstractAst>>;

    SPP_ATTR_NODISCARD auto direct_sup_types() const -> std::vector<std::shared_ptr<AbstractAst>>;

    SPP_ATTR_NODISCARD auto convert_postfix_to_nested_scope(AbstractAst const *postfix_ast) const -> Scope const*;

    SPP_ATTR_NODISCARD auto print_scope_tree() const -> std::string;

    SPP_ATTR_NODISCARD auto name_as_string() const -> std::string;

    auto fix_children_parent_pointers() -> void;
};
