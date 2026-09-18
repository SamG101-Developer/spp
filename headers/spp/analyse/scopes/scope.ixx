module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbol_table;
import spp.utils.types;
import std;
import sys;

use(spp::asts, struct Ast);
use(spp::asts, struct DeferStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct Symbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::compiler, struct Module);
use(spp::utils::errors, class ErrorFormatter);

namespace spp::analyse::scopes {
  SPP_EXP_CLS using ScopeName = std::variant<
    ScopeIdentifierName,
    ScopeTypeIdentifierName,
    ScopeBlockName>;
}

namespace spp::analyse::scopes {
  /// A counter that increments whenever a scope's place in
  /// the scope tree changes. This is because we can't reuse
  /// the cached fully qualified type, as this is derived from
  /// the scope chain.
  SPP_EXP_FUN SPP_ATTR_HOT auto ScopeLinkageGeneration() -> std::uint64_t;

  /// Record that a scope's place in the scope tree has
  /// changed, retiring everything cached against the old one.
  /// For example, whenever we re-parent a scope, we have to
  /// bump the linkage generation
  SPP_EXP_FUN auto BumpScopeLinkageGeneration() -> void;

  /// A counter that increments whenever something that provides
  /// the type's method set, changes. This is because the abstract
  /// method checking is hot, and not massively fast, so this allows
  /// caching results.
  SPP_EXP_FUN SPP_ATTR_HOT auto TypeStructureGeneration() -> std::uint64_t;

  /// Record either the position in the scope tree changing
  /// (like the scope linkage generation), or if the sup scopes
  /// change (attached during monomorphization)
  SPP_EXP_FUN auto BumpTypeStructureGeneration() -> void;

  /// A counter that changes whenever a type lookup could resolve
  /// differently; a symbol added or removed, a scope re-parented,
  /// or a super scope attached. Bumped by the other two bump
  /// functions.
  SPP_EXP_FUN SPP_ATTR_HOT auto TypeLookupGeneration() -> std::uint64_t;
}

SPP_EXP_CLS class spp::analyse::scopes::Scope {
public:
  /// The name of the scope; either a identifier ast (functions,
  /// modules), a type identifier ast (classes) or a block
  /// name (wrapped string like "case", "loop", etc). Stored
  /// in a variant.
  ScopeName Name;

  /// The parent scope. Parents own their children, so a raw
  /// reverse-pointer to view the parent. The global scope's
  /// parent will be nullptr.
  Scope *Parent;

  /// The child scopes. These are owned by the parent scope,
  /// and stored as unique pointers to enforce ownership.
  /// Provides an easy way to traverse a scope.
  Vec<std::unique_ptr<Scope>> Children;

  /// Top level scopes register their associated ast with the
  /// scope, for error reporting. Typically used by functions,
  /// classes and superimpositions. The asts own _Scope will be
  /// this scope.
  Ast *AstNode;

  /// If this is a scope for a type, then the symbol for that
  /// type will be linked into this scope here. This is nullptr
  /// for all other scopes (modules, functions, blocks, etc)
  std::shared_ptr<TypeSymbol> TySym;

  /// If this is a scope for a module, then the symbol for that
  /// module (namespace) will be linked into this scope here.
  /// This is nullptr for all other scopes (types, functions,
  /// blocks, etc)
  std::shared_ptr<NamespaceSymbol> NsSym;

  /// The scope representing the non-generic version of this
  /// scope. If this scope isn't a generic substitution, then it
  /// will point to itself. "Vec[Str]" -> "Vec", etc.
  Scope *NonGenericScope;

  /// The list of deferred statements written directly into
  /// this scope, in the order they were reached. Leaving the
  /// scope runs them in reverse.
  Vec<DeferStatementAst*> Deferred;

  /// The list of deferred statements that the stage 11 run has
  /// actually walked past, in the order they were reached. This
  /// is so that if the LLVM returns part way through a function,
  /// it would run defer statements that were declared below the
  /// actual exit point.
  Vec<DeferStatementAst*> DeferredReached;

  /// The sup-scopes directly attached to this type. Top
  /// recursively find the super scopes, use the "SupScopes()"
  /// method.
  Vec<Scope*> DirectSupScopes;

  /// The symbol table holder for the 3 internal tables (type,
  /// variable, namespace), that belong to this scope.
  SymbolTable InternalTable;

  /// Whether this scope's super scopes have been attached: they
  /// are attached once. Not carried by a copy, as a clone is a
  /// scope of its own.
  bool SupsAttached = false;

  /// The last "SupScopes()" answer and the "TypeStructureGeneration" it was computed at. The walk is the transitive
  /// super-scope graph - 29 nodes per call, measured, 4.9M node visits over a std build - and the graph only changes
  /// when a sup is attached, which bumps that generation. Left out of the copy constructor's list, so a clone starts
  /// with none: it has a graph of its own.
  mutable Vec<Scope*> _SupScopesCache;
  mutable std::uint64_t _SupScopesGen = 0;

  /// Called with a scope before anything reads its super scopes
  /// while "ScopeManager::AttachAllSuperScopes" runs, which
  /// attaches them first: a constraint checked mid-sweep never
  /// reads a half-built graph.
  inline static std::function<void(Scope const &)> OnSupScopesRead;

  /// Called by "Canon" with an open instantiation and the scope
  /// asking, when re-keying it through that scope's bindings
  /// names one not made yet: it makes it, as a name written
  /// there would, or returns null. Set while the analysis stages
  /// run ("CompilerBoot"), as only they can make one; a lookup
  /// cannot make it alone.
  inline static std::function<TypeSymbol*(TypeSymbol &, Scope const &)> OnInstantiationMissing;

  Scope(
    ScopeName name,
    Scope *parent,
    Ast *ast = nullptr,
    ErrorFormatter *error_formatter = nullptr);

  Scope(Scope const &other);
  ~Scope();

  /// Create a new global scope for the scope manager. This is
  /// only called once at the compile start. The global scope has
  /// no parent or corresponding ast. It is the root node of all
  /// modules. There is a corresponding (unused) global namespace
  /// symbol too, for uniform scope setup.
  /// Todo: in the future, we might allow a "::" prefix like c++
  static auto NewGlobal(Module const &mod) -> Shared<Scope>;

  /// Given a scope and a fully qualified type, this function
  /// moves through the namespace parts of the type, moving into
  /// the next namespace scopes. Returns the innermost namespace
  /// scope and the unqualified type (for this scope).
  static auto ShiftForNamespacedType(
    Scope const &scope, TypeAst const &fq_type) -> Pair<const Scope*, TypeIdentifierAst const*>;

  /// Get the error formatter for this scope. This is either the
  /// dedicated formatter attached to this scope, or an ancestral
  /// formatter, which will use the same token set (same module).
  SPP_ATTR_NODISCARD auto GetErrorFormatter() const -> ErrorFormatter*;

  /// Whether a node sits in the prelude appended behind this
  /// scope's file, rather than in what the author wrote.
  SPP_ATTR_NODISCARD auto IsFromPrelude(Ast const &ast) const -> bool;

  /// Get the generics associated with this scope, by searching all
  /// type and variable symbols, filtering them on their generic
  /// flag, and converting them into generic argument ast nodes.
  SPP_ATTR_NODISCARD auto GetGenerics() const -> Vec<Unique<GenericArgumentAst>>;

  /// Add a variable symbol into the scope, inserting it into the
  /// internal variable table within the main symbol table.
  auto AddVarSymbol(Shared<VariableSymbol> const &sym) -> void;

  /// Add a variable symbol into the scope, inserting it into the
  /// internal variable table within the main symbol table, and
  /// additionally check for a conflict.
  auto AddVarSymbolCheckConflict(Shared<VariableSymbol> const &sym) -> void;

  /// Add a type symbol into the scope, inserting it into the
  /// internal type table within the main symbol table.
  auto AddTypeSymbol(Shared<TypeSymbol> const &sym) -> void;

  /// Add a type symbol into the scope, inserting it into the
  /// internal type table within the main symbol table, and
  /// additionally check for a conflict.
  auto AddTypeSymbolCheckConflict(Shared<TypeSymbol> const &sym) -> void;

  /// Add a namespace symbol into the scope, inserting it into
  /// the internal namespace table within the main symbol table.
  auto AddNsSymbol(Shared<NamespaceSymbol> const &sym) -> void;

  /// Remove a variable symbol into the scope, deleting it from
  /// the internal variable table within the main symbol table.
  auto RemVarSymbol(IdentifierAst const *sym_name) -> Shared<VariableSymbol>;

  /// Remove a type symbol into the scope, deleting it from
  /// the internal type table within the main symbol table.
  auto RemTypeSymbol(TypeIdentifierAst const *sym_name) -> Shared<TypeSymbol>;

  /// Get all the variable symbols from the internal variable
  /// table unrolled into a vector.
  SPP_ATTR_NODISCARD auto AllVarSymbols(
    bool exclusive = false, bool sup_scope_search = false) const -> Vec<VariableSymbol*>;

  /// Get all the type symbols from the internal type table
  /// unrolled into a vector.
  SPP_ATTR_NODISCARD auto AllTypeSymbols(
    bool exclusive = false, bool sup_scope_search = false) const -> Vec<TypeSymbol*>;

  /// Get all the namespace symbols from the internal namespace
  /// table unrolled into a vector.
  SPP_ATTR_NODISCARD auto AllNsSymbols(bool exclusive = false, bool = false) const -> Vec<NamespaceSymbol*>;

  /// Check if a variable symbol with a given name is present
  /// in the internal variable symbol table.
  SPP_ATTR_NODISCARD auto HasVarSymbol(IdentifierAst const *sym_name, bool exclusive = false) const -> bool;

  /// Check if a namespace symbol with a given name is present
  /// in the internal namespace symbol table.
  SPP_ATTR_NODISCARD auto HasNsSymbol(IdentifierAst const *sym_name, bool exclusive = false) const -> bool;

  /// Query the internal variable symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetVarSymbol(
    IdentifierAst const *sym_name, bool exclusive = false, bool sup_scope_search = true) const -> VariableSymbol*;

  /// What a comp generic parameter, named somewhere else, means from
  /// this scope: this scope's binding of that very parameter (matched
  /// by "ParamId"), or the parameter itself when nothing here binds
  /// it. Anything else has no answer, and is resolved by name.
  SPP_ATTR_NODISCARD auto CanonVar(VariableSymbol &sym) const -> VariableSymbol*;

  /// Query the internal type symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetTypeSymbol(
    TypeAst const *sym_name, bool exclusive = false, bool sup_scope_search = true) const -> TypeSymbol*;

  /// What a type symbol, resolved somewhere else, means from this
  /// scope. A generic parameter is this scope's binding of that very
  /// parameter - matched by "ParamId", not by name - or the parameter
  /// itself when nothing here binds it; a closed class means the same
  /// everywhere; an open instantiation is the one its arguments name
  /// from here. Anything else (an alias, "Self", a mock), or an
  /// instantiation that does not exist yet, has no answer, and is
  /// resolved by name instead.
  SPP_ATTR_NODISCARD auto Canon(TypeSymbol &sym) const -> TypeSymbol*;

  /// The identity of a list of generic arguments, read from this
  /// scope: each argument's name and what it resolves to - a
  /// parameter's "ParamId", a symbol, a bound or literal value -
  /// rather than how it is spelled. Instantiations are filed under
  /// it in their template's "TypeSymbol::Instances".
  SPP_ATTR_NODISCARD auto InstanceIdentityKey(
    Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params = nullptr) const -> InstanceKey;

  /// Query the internal namespace symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetNsSymbol(
    IdentifierAst const *sym_name, bool exclusive = false) const -> NamespaceSymbol*;

  /// Split the expression into its parts by postfix member
  /// accessing, and move leftwards towards the outermost
  /// part. For "a.b.c", it would be "a". Then get the symbol
  /// for the outermost part by querying the variable table.
  SPP_ATTR_NODISCARD auto GetVarSymbolOutermost(
    Ast const &expr) const -> Pair<VariableSymbol*, Scope const*>;

  /// The difference in depth between 2 scopes, based on how
  /// close they are in the sup-scope chain. This is not a
  /// function to determine based on namespace/module depth.
  auto DepthDiff(const Scope *scope) const -> sys::ssize_t;

  /// Get the final child scope recursively from a scope. This
  /// is the scope that when iterated once more, will move to
  /// the next sibling of this scope.
  SPP_ATTR_NODISCARD auto FinalChildScope() const -> Scope const*;

  /// A list of ancestor scopes of this scope, by checking the
  /// parent scope until it's nullptr, ie we are in the global
  /// scope.
  SPP_ATTR_NODISCARD auto Ancestors() const -> Vec<Scope const*>;

  /// The parent module is the scope that is the closest module
  /// scope to the current scope, in the ancestor chain.
  SPP_ATTR_NODISCARD auto ParentModule() const -> Scope*;

  /// The top level parent module is the scope that is a direct
  /// child of the global scope, furthest away from this scope
  /// in the ancestor chain.
  SPP_ATTR_NODISCARD auto TopLevelParentModule() const -> Scope*;

  /// The enclosing type scope is the scope that is the closest
  /// type scope to the current scope, in the ancestor chain.
  SPP_ATTR_NODISCARD auto GetEnclosingTypeScope(CompilerMetaData const &meta) const -> Scope*;

  /// The enclosing self type is the type that "Self" represents
  /// when used in this scope.
  SPP_ATTR_NODISCARD auto GetEnclosingSelfType(CompilerMetaData const &meta) const -> Shared<TypeAst>;

  /// A recursively searched list of sup scopes that forms the
  /// entire tree of externally applied inheritance. This includes
  /// the "cls" type scopes, and the "sup" superimposition scopes,
  /// of all superimpositions.
  /// The answer is the scope's own cached vector, so a caller iterating it copies nothing. It stays valid until this
  /// scope walks its super scopes again, which no caller does while holding the reference.
  SPP_ATTR_NODISCARD auto SupScopes() const -> Vec<Scope*> const&;

  /// A list of all the sup types that this scope has, by taking
  /// the sup scopes, filtering them to the "cls" scopes, and
  /// grabbing their associated (fully qualified) names.
  SPP_ATTR_NODISCARD auto SupTypes() const -> Vec<Shared<TypeAst>>;

  /// Take a postfix member access ast and map it to a scope search
  /// by finding the outermost part, and moving inwards.
  SPP_ATTR_NODISCARD auto ConvertPostfixToNestedScope(ExpressionAst const *postfix_ast) const -> Scope const*;

  /// Convert the name of the scope into a string, using the visitor
  /// pattern on the possible variant member scope names.
  SPP_ATTR_NODISCARD auto NameAsString() const -> Str;

  /// Iterate the child scopes and set their parent scope to this
  /// scope. Recursively apply to their child scopes and so on too,
  /// fixing the entire subtree under this scope.
  auto FixChildrenToParentPointer() -> void;

private:
  /// The nullable error formatter for this scope. Uses the parent
  /// module's error formatter if this is nullptr - same token set.
  ErrorFormatter *_ErrorFormatter;
};
