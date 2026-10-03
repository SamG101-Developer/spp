module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope;
import spp.analyse.scopes.comp_key;
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
  std::shared_ptr<TypeSymbol> LinkedTypeSymbol;

  /// If this is a scope for a module, then the symbol for that
  /// module (namespace) will be linked into this scope here.
  /// This is nullptr for all other scopes (types, functions,
  /// blocks, etc)
  std::shared_ptr<NamespaceSymbol> LinkedNamespaceSymbol;

  /// The scope representing the non-generic version of this
  /// scope. If this scope isn't a generic substitution, then it
  /// will point to itself. "Vec[Str]" -> "Vec", etc.
  Scope *NonGnScope;

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
  /// recursively find the super scopes, use the "GetSupScopes()"
  /// method.
  Vec<Scope*> DirectSupScopes;

  /// The symbol table holder for the 3 internal tables (type,
  /// variable, namespace), that belong to this scope.
  SymbolTable InternalTable;

  /// Whether this scope's super scopes have been attached: they
  /// are attached once. Not carried by a copy, as a clone is a
  /// scope of its own.
  bool SupsAttached = false;

  /// The last "GetSupScopes()" answer and the "TypeStructureGeneration" it was computed at. The walk is the transitive
  /// super-scope graph - 29 nodes per call, measured, 4.9M node visits over a std build - and the graph only changes
  /// when a sup is attached, which bumps that generation. Left out of the copy constructor's list, so a clone starts
  /// with none: it has a graph of its own.
  mutable Vec<Scope*> _SupScopesCache;
  mutable std::uint64_t _SupScopesGen = 0;

  /// Called with a scope before its super scopes are read
  /// ("GetDirectSupScopes", the one door the reads go through) while
  /// "ScopeManager::AttachAllSuperScopes" runs, which attaches them
  /// first: a constraint checked mid-sweep never reads a half-built
  /// graph. It can run from the middle of any analysis (keying a type,
  /// a lookup inside a qualified name), so it starts from a clean
  /// context of its own. Unset outside the sweep, where every scope's
  /// super scopes are attached already.
  inline static std::function<void(Scope const &)> OnSupScopesRead;

  /// "DirectSupScopes", attached first while the sweep runs
  /// ("OnSupScopesRead"): what every read of the super-scope graph
  /// reaches them through.
  SPP_ATTR_NODISCARD auto GetDirectSupScopes() const -> Vec<Scope*> const& {
    if (OnSupScopesRead) { OnSupScopesRead(*this); }
    return DirectSupScopes;
  }

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
  SPP_ATTR_NODISCARD auto GetGns() const -> Vec<Unique<GenericArgumentAst>>;

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

  /// Add a namespace symbol into the scope, and additionally check
  /// for a conflict. Namespaces cannot be aliased yet, so nothing
  /// calls this; it completes the family for when they can be.
  auto AddNsSymbolCheckConflict(Shared<NamespaceSymbol> const &sym) -> void;

  /// Remove a variable symbol into the scope, deleting it from
  /// the internal variable table within the main symbol table.
  auto RemVarSymbol(IdentifierAst const *sym_name) -> Shared<VariableSymbol>;

  /// Remove a type symbol into the scope, deleting it from
  /// the internal type table within the main symbol table.
  auto RemTypeSymbol(TypeIdentifierAst const *sym_name) -> Shared<TypeSymbol>;

  /// Remove a namespace symbol from the scope, deleting it from
  /// the internal namespace table within the main symbol table.
  auto RemNsSymbol(IdentifierAst const *sym_name) -> Shared<NamespaceSymbol>;

  /// Get all the variable symbols from the internal variable
  /// table unrolled into a vector.
  SPP_ATTR_NODISCARD auto GetAllVarSymbols(
    bool exclusive = false, bool sup_scope_search = false) const -> Vec<VariableSymbol*>;

  /// Get all the type symbols from the internal type table
  /// unrolled into a vector.
  SPP_ATTR_NODISCARD auto GetAllTypeSymbols(
    bool exclusive = false, bool sup_scope_search = false) const -> Vec<TypeSymbol*>;

  /// Get all the namespace symbols from the internal namespace
  /// table unrolled into a vector. A namespace has no super scopes
  /// to search.
  SPP_ATTR_NODISCARD auto GetAllNsSymbols(bool exclusive = false) const -> Vec<NamespaceSymbol*>;

  /// Check if a variable symbol with a given name is present
  /// in the internal variable symbol table.
  SPP_ATTR_NODISCARD auto HasVarSymbol(IdentifierAst const *sym_name, bool exclusive = false) const -> bool;

  /// Check if a type symbol with a given name is present in the
  /// internal type symbol table.
  SPP_ATTR_NODISCARD auto HasTypeSymbol(TypeAst const *sym_name, bool exclusive = false) const -> bool;

  /// Check if a namespace symbol with a given name is present
  /// in the internal namespace symbol table.
  SPP_ATTR_NODISCARD auto HasNsSymbol(IdentifierAst const *sym_name, bool exclusive = false) const -> bool;

  /// Query the internal variable symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto FindVarSymbol(
    IdentifierAst const *sym_name, bool exclusive = false, bool sup_scope_search = true) const -> VariableSymbol*;

  /// Query the internal type symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto FindTypeSymbol(
    TypeAst const *sym_name, bool exclusive = false, bool sup_scope_search = true) const -> TypeSymbol*;

  /// The "Self" symbol this scope sees ("FindTypeSymbol" of "Self"): the class, "sup" block or alias it stands for is
  /// behind it ("TypeSymbol::AsBound").
  SPP_ATTR_NODISCARD auto FindSelfSymbol(bool exclusive = false) const -> TypeSymbol*;

  /// "FindTypeSymbol" for a type's head, its generic arguments stripped ("Vec" for "Vec[Str]" or "&Vec[Str]"): the
  /// template a name instantiates, or what a name without arguments names.
  SPP_ATTR_NODISCARD auto FindHeadSymbol(TypeAst const &type, bool exclusive = false) const -> TypeSymbol*;

  /// The identity of a list of generic arguments, read from this
  /// scope: each argument's name and what it resolves to - a
  /// parameter's "ParamId", a symbol, a bound or literal value -
  /// rather than how it is spelled. A function's instantiations
  /// are filed under it; a type's under "InstanceIdOf".
  SPP_ATTR_NODISCARD auto ArgsIdOf(
    Vec<GenericArgumentAst*> const &args, GenericParameterGroupAst const *params = nullptr) const -> TypeId;

  /// The identity ("TypeId") of the instantiation of "tmpl" that
  /// "args" ask for, read from this scope: its template and the
  /// identity of its arguments, or for a variant the set of its
  /// members. Instantiations are filed under it in their template's
  /// "TypeSymbol::Instances", so one identity is one symbol however
  /// it is spelled. Keyed even with a part that does not resolve.
  SPP_ATTR_NODISCARD auto InstanceIdOf(
    TypeSymbol const &tmpl, Vec<GenericArgumentAst*> const &args,
    GenericParameterGroupAst const *params = nullptr) const -> TypeId;

  /// What a type resolves to here, interned ("TypeKey"): equal for
  /// two types exactly when they are one type, or null when any part
  /// of the type does not resolve. The comp twin is "CompIdOf".
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto TypeIdOf(TypeAst const &type) const -> TypeId;

  /// "TypeKey", interned whatever it holds: a part that does not
  /// resolve stays one part of it ("HasUnresolved"), rather than
  /// making it no identity as "TypeIdOf" does. What a question about
  /// what a type names ("IsConcreteTypeId") is asked of, where a
  /// name that does not resolve here is no answer either way.
  SPP_ATTR_NODISCARD auto PartialTypeIdOf(TypeAst const &type) const -> TypeId;

  /// The "TypeId" of a type already resolved to "sym", held under
  /// the convention tag "conv" (0 for none) - what a "TypeRef" is
  /// one type with.
  SPP_ATTR_NODISCARD auto TypeIdOfSymbol(TypeSymbol const &sym, std::uint64_t conv) const -> TypeId;

  /// The symbol a "TypeId" names, its convention aside: a closed
  /// class, a parameter, an instantiation or variant already made
  /// (found in its template's "Instances" by that identity), or
  /// "Self" as this scope reads it. Null for anything not made yet.
  /// A lookup: it makes nothing.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto TypeSymbolOf(TypeId id) const -> TypeSymbol*;

  /// "TypeSymbolOf" for a comp parameter, by its "ParamId": the parameter itself, never a binding of it.
  SPP_ATTR_NODISCARD auto VarSymbolOf(std::uint64_t comp_param) const -> VariableSymbol*;


  /// An identity as written, read here: each type parameter it
  /// names replaced by what this scope binds it to (found by the
  /// parameter's identity, "CanonType"), each comp parameter by its
  /// bound value's identity. What this scope leaves unbound stays.
  SPP_ATTR_NODISCARD auto ReadIn(TypeId written) const -> TypeId;


  /// What a written identity ("WrittenTypeIdOf") names read here: the
  /// symbol it names ("TypeSymbolOf"), as this scope means it
  /// ("CanonType"), as "FindWrittenVarSymbol" reads a comp parameter. Null
  /// for an instantiation not made yet (a lookup makes nothing), and
  /// for "Self", which is looked up by name.
  SPP_ATTR_NODISCARD auto FindWrittenTypeSymbol(TypeId written) const -> TypeSymbol*;

  /// "FindWrittenTypeSymbol" for a comp parameter named somewhere else ("IdentifierAst::WrittenCompParamId"): what it
  /// means here ("CanonVar"). Null for an identity no parameter is registered under; "FindVarSymbol" then looks the
  /// name up as it does any other variable's.
  SPP_ATTR_NODISCARD auto FindWrittenVarSymbol(std::uint64_t comp_param) const -> VariableSymbol*;

  /// The type an identity names, as a written type: the qualified
  /// name of the symbol filed under it where one is made, else one
  /// built from the identity - its template and arguments, a
  /// variant's members, a parameter, "Self" - so an instantiation
  /// nothing has made yet can be named, and made by reading it.
  /// Null when a part cannot be named (a comp value whose identity
  /// was never recorded).
  SPP_ATTR_NODISCARD auto TypeAstOf(TypeId id) const -> Shared<TypeAst>;

  /// "TypeAstOf" for a comp identity ("CompNodeOf"), built from the identity itself - a value as its literal, a
  /// parameter as its name (recording it, so it is that parameter wherever it is read), a pack as a tuple, an
  /// operation as one, a constant named through a type as that access (the type named here). An opaque part, which no
  /// identity names, is the value recorded for it as it was keyed ("comp_generics::OpaqueCompValue"). Null when a part
  /// cannot be named.
  SPP_ATTR_NODISCARD auto CompAstOf(CompId id) const -> Shared<ExpressionAst>;

  /// "CompAstOf" for an identity already parsed.
  SPP_ATTR_NODISCARD auto CompAstOf(CompNode const &node) const -> Unique<ExpressionAst>;

  /// "TypeIdOf" for a comp value written here: its identity ("comp_generics::CompKey"), folded where closed.
  SPP_ATTR_NODISCARD auto CompIdOf(ExpressionAst const &value) const -> CompId;

  /// "TypeIdOfSymbol" for a comp generic: the identity of the value it is bound to, else of the parameter itself.
  SPP_ATTR_NODISCARD auto CompIdOfSymbol(VariableSymbol const &sym) const -> CompId;

  /// Query the internal namespace symbol table to get a symbol
  /// with a matching name, checking ancestor scopes and super
  /// scopes if configured too.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto FindNsSymbol(
    IdentifierAst const *sym_name, bool exclusive = false) const -> NamespaceSymbol*;

  /// Split the expression into its parts by postfix member
  /// accessing, and move leftwards towards the outermost
  /// part. For "a.b.c", it would be "a". Then get the symbol
  /// for the outermost part by querying the variable table.
  SPP_ATTR_NODISCARD auto FindVarSymbolOutermost(
    Ast const &expr) const -> Pair<VariableSymbol*, Scope const*>;

  /// The difference in depth between 2 scopes, based on how
  /// close they are in the sup-scope chain. This is not a
  /// function to determine based on namespace/module depth.
  auto DepthDiff(const Scope *scope) const -> sys::ssize_t;

  /// Get the final child scope recursively from a scope. This
  /// is the scope that when iterated once more, will move to
  /// the next sibling of this scope.
  SPP_ATTR_NODISCARD auto GetFinalChildScope() const -> Scope const*;

  /// A list of ancestor scopes of this scope, by checking the
  /// parent scope until it's nullptr, ie we are in the global
  /// scope.
  SPP_ATTR_NODISCARD auto GetAncestors() const -> Vec<Scope const*>;

  /// The parent module is the scope that is the closest module
  /// scope to the current scope, in the ancestor chain.
  SPP_ATTR_NODISCARD auto GetParentModule() const -> Scope*;

  /// The top level parent module is the scope that is a direct
  /// child of the global scope, furthest away from this scope
  /// in the ancestor chain.
  SPP_ATTR_NODISCARD auto GetTopLevelParentModule() const -> Scope*;

  /// The enclosing type scope is the scope that is the closest
  /// type scope to the current scope, in the ancestor chain.
  SPP_ATTR_NODISCARD auto FindEnclosingTypeScope(CompilerMetaData const &meta) const -> Scope*;

  /// The enclosing self type is the type that "Self" represents
  /// when used in this scope.
  SPP_ATTR_NODISCARD auto FindEnclosingSelfType(CompilerMetaData const &meta) const -> Shared<TypeAst>;

  /// A recursively searched list of sup scopes that forms the
  /// entire tree of externally applied inheritance. This includes
  /// the "cls" type scopes, and the "sup" superimposition scopes,
  /// of all superimpositions.
  /// The answer is the scope's own cached vector, so a caller iterating it copies nothing. It stays valid until this
  /// scope walks its super scopes again, which no caller does while holding the reference.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto GetSupScopes() const -> Vec<Scope*> const&;

  /// A list of all the sup types that this scope has, by taking
  /// the sup scopes, filtering them to the "cls" scopes, and
  /// grabbing their associated (fully qualified) names.
  SPP_ATTR_NODISCARD auto GetSupTypes() const -> Vec<Shared<TypeAst>>;

  /// The namespace scope "parts" names from here ("a::b::c" as "a", "b", "c"), each part looked up in the scope the one
  /// before it names. Null when a part names no namespace. The one namespace walk: a written type's namespace
  /// ("FindTypeSymbol"), a postfix chain ("ConvertPostfixToNestedScope"), and a raising lookup
  /// ("member_lookup::FindNsSymbolOrError") all go through it.
  SPP_ATTR_NODISCARD auto FindNsScope(Vec<IdentifierAst const*> const &parts) const -> Scope const*;

  /// "FindNsScope" for a postfix chain of static member accesses ("a::b::c"), read outermost first.
  SPP_ATTR_NODISCARD auto ConvertPostfixToNestedScope(ExpressionAst const *postfix_ast) const -> Scope const*;

  /// Convert the name of the scope into a string, using the visitor
  /// pattern on the possible variant member scope names.
  SPP_ATTR_NODISCARD auto NameAsString() const -> Str;

  /// Iterate the child scopes and set their parent scope to this
  /// scope. Recursively apply to their child scopes and so on too,
  /// fixing the entire subtree under this scope.
  auto FixChildrenToParentPointer() -> void;

private:
  /// What a type resolves to here, as a key: equal for two types
  /// exactly when they are one type. An instantiation is its
  /// template and the key of its arguments, read here - also one
  /// that is not made yet, which is keyed as it would be without
  /// making it. A variant is the set of its members. A key may
  /// hold an unresolved part, which "TypeIdOf" turns away and
  /// "ArgsIdOf" keeps as one part of an instantiation's key. The
  /// comp twin is "comp_generics::CompKey".
  SPP_ATTR_NODISCARD auto TypeKey(TypeAst const &type) const -> InstanceKey;

  /// What a type symbol, resolved somewhere else, means from this
  /// scope ("FindWrittenTypeSymbol" reads every written identity
  /// through it). A generic parameter is this scope's binding of that
  /// very parameter - matched by "ParamId", not by name - or the
  /// parameter itself when nothing here binds it; an open
  /// instantiation is the one its arguments name from here (none when
  /// that is not made yet); a generic block's alias is this scope's
  /// copy of it. Anything else (a closed class, a template, a mock)
  /// is itself.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto CanonType(TypeSymbol &sym) const -> TypeSymbol*;

  /// "CanonType" for a comp generic ("FindWrittenVarSymbol"): this
  /// scope's binding of that parameter, or the parameter itself when
  /// nothing here binds it. Anything that is no comp generic (a local,
  /// an attribute, a constant) has no answer: it has no identity to
  /// read, and is found by its name.
  SPP_ATTR_NODISCARD auto CanonVar(VariableSymbol &sym) const -> VariableSymbol*;

  /// The nullable error formatter for this scope. Uses the parent
  /// module's error formatter if this is nullptr - same token set.
  ErrorFormatter *_ErrorFormatter;
};
