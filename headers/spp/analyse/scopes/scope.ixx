module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbol_table;
import spp.utils.types;
import std;
import sys;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::compiler {
    SPP_EXP_CLS struct Module;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct Symbol;
    SPP_EXP_CLS struct TypeSymbol;
    SPP_EXP_CLS struct NamespaceSymbol;
    SPP_EXP_CLS struct VariableSymbol;
    SPP_EXP_CLS using ScopeName = std::variant<
        ScopeIdentifierName,
        ScopeTypeIdentifierName,
        ScopeBlockName>;
}

namespace spp::utils::errors {
    SPP_EXP_CLS class ErrorFormatter;
}

SPP_EXP_CLS class spp::analyse::scopes::Scope {
public:
    /**
     * The name of the scope. This will be either an @c Shared<IdentifierAst> (functions, modules), an
     * @c Shared<TypeIdentifierAst> (classes), or a @c ScopeBlockName (blocks: @c case, @c loop, etc). It is
     * stored in a @c std::variant to allow for easy type-safe access to the underlying type.
     */
    ScopeName Name;

    /**
     * The parent scope. It is a raw pointer as the parents "own" their children, and the children do not own their
     * parents. The parent will be @c nullptr for the global scope.
     */
    Scope *Parent;

    /**
     * The child scopes. These are owned by the parent scope, and are stored as @c Unique to ensure proper
     * memory management. This allows for an easy traversal of the scope hierarchy.
     */
    Vec<std::unique_ptr<Scope>> Children;

    /**
     * Top level scopes register their AST with the scope. This is useful for error reporting, as it allows for easy
     * access to the AST node that the scope represents. This will be @c nullptr for non-top level scopes. Typically,
     * the AST will need to be cast back to its original type.
     */
    asts::Ast *AstNode;

    /**
     * The (potential) type symbol that represents this scope. This will be @c nullptr for non-type scopes (eg
     * functions, modules, blocks).
     */
    std::shared_ptr<TypeSymbol> TySym;

    /**
     * The (potential) namespace symbol that represents this scope. This will be @c nullptr for non-namespace scopes
     * (eg functions, classes, blocks). Note that a namespace is a module.
     */
    std::shared_ptr<NamespaceSymbol> NsSym;

    /**
     * The scope representing the non-generic version of this scope. If this scope isn't a generic substitution, then
     * the non-generic scope is the scope itself. For @c Vec[Str], the non-generic scope is @c Vec.
     */
    Scope *NonGenericScope;

    Vec<Scope*> DirectSupScopes;

    /**
     * The collection of symbol tables (type table, variable table, namespace table) that belong to this scope.
     * Individual accessors are created for each table, but the tables are consolidated into a wrapper object for ease
     * of management.
     */
    SymbolTable InternalTable;

    Scope(
        ScopeName name,
        Scope *parent,
        asts::Ast *ast = nullptr,
        utils::errors::ErrorFormatter *error_formatter = nullptr);

    Scope(
        Scope const &other);

    ~Scope();

    /**
     * Create a new global scope for the scope manager. This is called once at the start of
     * compilation. A global scope has no parent or corresponding ast; it is the root node of all
     * namespaces. A corresponding namespace symbol is created for the global scope, for uniform
     * handling of namespaces.
     * @param mod The "main" module being compiled.
     * @return
     */
    static auto NewGlobal(compiler::Module const &mod) -> Shared<Scope>;

    /**
     * Search all "sup" scopes of an existing scope (this will be a type scope), for a variable
     * symbol with a name that matches "name". This is used when looking for the a constant defined
     * with "cmp" within a sup-block of a type.
     * @param scope The starting scope to search from.
     * @param name The name of the variable symbol to search for.
     * @return The found variable symbol, or nullptr if not found.
     */
    static auto SearchSupScopesForVar(Scope const &scope, Shared<asts::IdentifierAst> const &name) -> Shared<VariableSymbol>;

    /**
     * Search all "sup" scopes of an existing scope (this will be a type scope), for a type
     * symbol with a name that matches "name". This is used when looking for a type defined with a
     * "type" statement within a sup-block of a type.
     * @param scope The starting scope to search from.
     * @param name The name of the type symbol to search for.
     * @return The found type symbol, or nullptr if not found.
     */
    static auto SearchSupScopesForType(Scope const &scope, Shared<const asts::TypeAst> const &name) -> Shared<TypeSymbol>;

    /**
     * Given a scope and a fully qualified type, this function moves through the namespace parts of
     * the type, moving into the next namespace scopes. Finally, it will arrive at the scope for the
     * innermost (rightmost) namespace part, and the compeltely unqualified type name. For example,
     * @c scope:random_scope and @c string::Str becomes @c scope:string and @c Str.
     * @param scope The starting scope to search from.
     * @param fq_type The fully qualified type to shift the scope for.
     * @return A pair of the shifted scope and the unqualified type identifier.
     */
    static auto ShiftForNamespacedType(Scope const &scope, asts::TypeAst const &fq_type) -> Pair<const Scope*, Shared<const asts::TypeIdentifierAst>>;

    /**
     * Get the error formatter associated with this scope. Lots of scopes don't have error
     * formatters, so if the formatter doesn't exist, the scope's parent's formatter is used,
     * recursively searching upwards until a module scope is found, which will have an errror
     * foratter.
     * @return The error formatter associated with this scope.
     */
    SPP_ATTR_NODISCARD auto GetErrorFormatter() const -> utils::errors::ErrorFormatter*;

    SPP_ATTR_NODISCARD auto GetGenerics() const -> UniqueVec<asts::GenericArgumentAst>;

    SPP_ATTR_NODISCARD auto GetExtendedGenericSymbols(Vec<asts::GenericArgumentAst*> const &generics, Shared<asts::TypeAst> const &ignore = nullptr) const -> SharedVec<Symbol>;

    /**
     * Register a new variable symbol into the symbol table held inside this scope.
     * @param sym The new variable symbol.
     */
    auto AddVarSymbol(Shared<VariableSymbol> const &sym) -> void;

    /**
     * Register a new variable symbol into the symbol table held inside this scope, checking for conflicts with existing
     * symbols. If a conflict is found, an error is raised.
     * @param sym The new variable symbol.
     */
    auto AddVarSymbolCheckConflict(Shared<VariableSymbol> const &sym) -> void;

    /**
     * Register a new type symbol into the symbol table held inside this scope.
     * @param sym The new type symbol.
     */
    auto AddTypeSymbol(Shared<TypeSymbol> const &sym) -> void;

    /**
     * Register a new type symbol into the symbol table held inside this scope, checking for conflicts with existing
     * symbols. If a conflict is found, an error is raised.
     * @param sym The new type symbol.
     */
    auto AddTypeSymbolCheckConflict(Shared<TypeSymbol> const &sym) -> void;

    /**
     * Register a new namespace symbol into the symbol table held inside this scope.
     * @param sym The new namespace symbol.
     */
    auto AddNsSymbol(Shared<NamespaceSymbol> const &sym) -> void;

    /**
     * Remove a variable symbol from the symbol table held inside this scope.
     * @param sym_name The name of the variable symbol to remove.
     */
    auto RemVarSymbol(Shared<asts::IdentifierAst> const &sym_name) -> Shared<VariableSymbol>;

    /**
     * Remove a type symbol from the symbol table held inside this scope.
     * @param sym_name The name of the type symbol to remove.
     */
    auto RemTypeSymbol(Shared<asts::TypeIdentifierAst> const &sym_name) -> Shared<TypeSymbol>;

    /**
     * Remove a namespace symbol from the symbol table held inside this scope.
     * @param sym_name The name of the namespace symbol to remove.
     */
    auto RemNsSymbol(Shared<asts::IdentifierAst> const &sym_name) -> Shared<NamespaceSymbol>;

    SPP_ATTR_NODISCARD auto AllVarSymbols(bool exclusive = false, bool sup_scope_search = false) const -> SharedVec<VariableSymbol>;

    SPP_ATTR_NODISCARD auto AllTypeSymbols(bool exclusive = false, bool sup_scope_search = false) const -> SharedVec<TypeSymbol>;

    SPP_ATTR_NODISCARD auto AllNsSymbols(bool exclusive = false, bool = false) const -> SharedVec<NamespaceSymbol>;

    SPP_ATTR_NODISCARD auto HasVarSymbol(Shared<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;

    SPP_ATTR_NODISCARD auto HasTypeSymbol(Shared<asts::TypeAst> const &sym_name, bool exclusive = false) const -> bool;

    SPP_ATTR_NODISCARD auto HasNsSymbol(Shared<asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> bool;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetVarSymbol(Shared<asts::IdentifierAst> const &sym_name, bool exclusive = false, bool sup_scope_search = true) const -> Shared<VariableSymbol>;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetTypeSymbol(Shared<const asts::TypeAst> const &sym_name, bool exclusive = false, bool sup_scope_search = true) const -> Shared<TypeSymbol>;

    SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetNsSymbol(Shared<const asts::IdentifierAst> const &sym_name, bool exclusive = false) const -> Shared<NamespaceSymbol>;

    SPP_ATTR_NODISCARD auto GetVarSymbolOutermost(asts::Ast const &expr) const -> Pair<Shared<VariableSymbol>, Scope const*>;

    auto DepthDiff(const Scope *scope) const -> sys::ssize_t;

    SPP_ATTR_NODISCARD auto FinalChildScope() const -> Scope const*;

    SPP_ATTR_NODISCARD auto Ancestors() const -> Vec<Scope const*>;

    SPP_ATTR_NODISCARD auto ParentModule() const -> Scope*;

    SPP_ATTR_NODISCARD auto TopLevelParentModule() const -> Scope*;

    SPP_ATTR_NODISCARD auto GetEnclosingTypeScope(asts::meta::CompilerMetaData const &meta) const -> Scope*;

    SPP_ATTR_NODISCARD auto SupScopes() const -> Vec<Scope*>;

    SPP_ATTR_NODISCARD auto SupTypes() const -> SharedVec<asts::TypeAst>;

    SPP_ATTR_NODISCARD auto DirectSupTypes() const -> SharedVec<asts::TypeAst>;

    auto ConvertPostfixToNestedScope(asts::ExpressionAst const *postfix_ast) const -> Scope const*;

    SPP_ATTR_NODISCARD auto PrintScopeTree() const -> Str;

    SPP_ATTR_NODISCARD auto NameAsString() const -> Str;

    auto FixChildrenToParentPointer() -> void;

private:
    utils::errors::ErrorFormatter *_ErrorFormatter;
};
