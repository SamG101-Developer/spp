module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.utils.visibility;
import spp.codegen.llvm_sym_info;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct AliasInfo);
use(spp::analyse::scopes, struct Symbol);
use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::scopes, enum class VariableKind);
use(spp::analyse::scopes, enum class TypeKind);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypeStatementAst);

/// The base symbol type for all symbol variations to inherit
/// from. This provides a common interface for them, and some
/// abstract method that must be implemented by all derived
/// classes.
SPP_EXP_CLS struct spp::analyse::scopes::Symbol : EnableLocalSharedFromThis<Symbol> {
  SPP_GCC_VTABLE_FIX_BASE;

  virtual ~Symbol();

  /// Simple function determining whether the symbol actually
  /// needs to be deeply copied, or we can do a shallow copy and
  /// it will act the same. Good optimization for cloning.
  SPP_ATTR_NODISCARD virtual auto NeedsDeepCopy() const -> bool = 0;

  /// Add a customized "shared_from_this()" implementation for
  /// derived classes. This allows a simple way to get the shared
  /// version of this struct, downcast to any of the specialised
  /// symbols.
  template <typename Derived = Symbol>
  SPP_ATTR_NODISCARD auto SharedFromThis() -> Shared<Derived> {
    if constexpr (std::is_same_v<Derived, Symbol>) { return shared_from_this(); }
    else { return spp::static_shared_cast<Derived>(shared_from_this()); }
  }
};

/// The namespace symbol is the simplest symbol, and allows the
/// discovery of a nested module namespace from a given module
/// scope.
SPP_EXP_CLS struct spp::analyse::scopes::NamespaceSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX;

  /// The name of the namespace; this will be the same as the
  /// namespace scope whom this symbol represents.
  Shared<IdentifierAst> Name;

  /// The namespace scope that this symbol represents. This
  /// symbol will be the "NsSym" on the linked scope.
  Scope *LinkedScope;

  NamespaceSymbol(Shared<IdentifierAst> name, Scope *scope);
  NamespaceSymbol(NamespaceSymbol const &that);
  ~NamespaceSymbol() override;

  /// A namespace symbol never needs to be deep copied - nothing
  /// is ever mutated and shared.
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  /// Equality is done by pointer comparison. We only want to
  /// know if two pointers are the same effectively.
  auto operator==(NamespaceSymbol const &that) const -> bool;
};

/// What a variable symbol names. Several different things share
/// the variable table, and this is how they are told apart,
/// rather than by which other fields happen to be set.
SPP_EXP_CLS enum class spp::analyse::scopes::VariableKind {
  Local, // A "let" binding or a function parameter.
  Temporary, // A "$" desugaring temporary, never written by the programmer.
  Import, // A "use" import whose target is not found yet; takes the target's kind in stage 3.
  Capture, // A closure's capture, owned by the closure's environment.
  FlowNarrowing, // A narrowed view of another symbol's value, from a case pattern.
  Attribute, // A class attribute, reached with ".".
  Constant, // A "cmp" constant, reached with "::".
  Function, // A function's "$" mock constant, reached with "." or "::".
  GenericCompParam, // A comp generic parameter, not yet bound.
  GenericCompArg, // A comp generic bound to an argument.
};

/// What a type symbol names, the counterpart of "VariableKind".
/// Whether a generic is bound, and to what, is still read off
/// its "LinkedScope" and "GenericVal" - a binding to another
/// generic behaves like an unbound one, which no kind captures.
SPP_EXP_CLS enum class spp::analyse::scopes::TypeKind {
  Class, // A class, or an instantiation of one.
  Alias, // A "type" statement or a "use" of a type; see "AliasInfo".
  Self, // The "Self" of a class, "sup" block or alias.
  GenericParam, // A generic type parameter, not yet bound.
  GenericArg, // A generic type parameter bound to an argument.
  FunctionMock, // A function's "$" mock class, which its overloads are superimposed over.
  ClosureMock, // A closure's "$closure" mock class, with its function type attached directly.
};

/// A variable symbol is used extremely often, for class fields,
/// constants, parameters, variables, captures, etc etc. It has
/// a host of flags for fine-tuning usage.
SPP_EXP_CLS struct spp::analyse::scopes::VariableSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX;

  /// The name of the symbol as the identifier ast, used for
  /// matching on a "get symbol" operation.
  Shared<IdentifierAst> Name;

  /// The type that this variable is. This is always provided
  /// on declaration of a variable, so should never be nullptr
  /// or even deferred.
  Shared<TypeAst> Type;

  /// The scope that this symbol was defined in (this will happen
  /// to be the scope that the symbol resides in, which is not
  /// always the case for type symbols).
  Scope *ScopeDefinedIn;

  /// The kind of variable symbol, such as a comptime constant,
  /// a capture, a flow types narrower, etc.
  VariableKind Kind;

  /// Whether the symbol is mutable, ie can the variable it
  /// represents be re-assigned a new value.
  bool IsMutable = false;

  /// For a "use" import, the symbol it names (its target). This
  /// is nullptr for anything not imported, and for an import
  /// before stage 3 - whose kind is "Import" until then.
  Shared<VariableSymbol> AliasSym;

  /// For flow typing, store the variable that this symbol has
  /// narrowed down from. If this is a variable of "Some[S32]",
  /// the "NarrowsSym" might be the "Opt[S32]" type variable.
  Shared<VariableSymbol> NarrowsSym;

  /// There is a unique cases with functional types; if we constrain
  /// a generic F to FunMov, and then supply a FunRef, the "f: F"
  /// would get called with the convention based on the actual type.
  /// To enforce consistency of memory rules, we call it with the
  /// constraint type, so whether FunMov/Mut/Ref is passed in, it
  /// always uses the FunMov technique and memory analysis.
  Shared<const TypeAst> CallableAsType;

  /// The visibility that this variable has. This is only
  /// applicable in contexts such as module/sup-level constants,
  /// and is only queried when needed (nullptr is fine).
  asts::utils::Visibility Visibility;

  /// Reflecting the above "Visibility", the ast representing
  /// the visibility is also used, for error reporting diagnostics.
  /// This is needed to ensure there are no visibility conflicts
  /// for all overloads of the same function.
  AnnotationAst *VisibilityAnnotation = nullptr;

  /// The memory info for this variable, unique per symbol and
  /// used extensively in stage 8 memory analysis. Holds all move,
  /// initialization-stage, branch-inconsistencies, etc.
  Unique<utils::mem_info_utils::MemoryInfo> MemInfo;

  /// The LLVM symbol information used during stage 10 and 11 of
  /// the compilation pipeline. Currently, tracks the "alloca".
  Shared<codegen::LlvmVarSymInfo> LlvmInfo;

  /// The compile-time value: a constant's (folded) value, a bound
  /// comp generic's argument, or a local's value while a comp-time
  /// call runs. Null when there is none, as for an unbound comp
  /// generic.
  Unique<Ast> CompTimeValue;

  VariableSymbol(
    Shared<IdentifierAst> name,
    Shared<TypeAst> type,
    Scope *ScopeDefinedIn,
    VariableKind kind,
    bool is_mutable = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPrivate);

  VariableSymbol(VariableSymbol const &that);
  ~VariableSymbol() override;

  /// A variable symbol must be deep copied, as it contains
  /// lots of mutable state that we don't want to be shared,
  /// especially as the analysis is what mutates it (stages
  /// 7, 8, 11 in particular).
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  /// Equality is done by pointer comparison. We only want to
  /// know if two pointers are the same effectively.
  auto operator==(VariableSymbol const &that) const -> bool;

  /// Get the fully qualified name of a variable by moving
  /// through ancestors - applicable to constants etc.
  SPP_ATTR_NODISCARD auto FqName() const -> Shared<ExpressionAst>;

  /// The value a comp generic was bound to: its compile-time
  /// value, for a bound one only. Null for anything else,
  /// including a comp generic that is still unbound.
  SPP_ATTR_NODISCARD auto BoundCompValue() const -> ExpressionAst*;

  /// Whether this is a comp generic, bound or not.
  SPP_ATTR_NODISCARD auto IsCompGeneric() const -> bool;

  /// Whether this has no runtime storage of its own: a "cmp"
  /// constant, a function's mock constant, or a comp generic.
  SPP_ATTR_NODISCARD auto IsCompTime() const -> bool;

  /// Whether this names something brought in by a "use" rather
  /// than declared here: an import still waiting for its target,
  /// or one that has found it. A declaration may shadow an import,
  /// but importing a name twice is a redefinition.
  SPP_ATTR_NODISCARD auto IsImport() const -> bool;
};

/// All required information about aliases - how it was
/// written, how it resolved, generic information scopes, etc.
/// Aliases aren't "new types", they are transparent secondary
/// names for already existing types, so the genuine type is
/// the "Resolved" type
SPP_EXP_CLS struct spp::analyse::scopes::AliasInfo {
  /// The target as it was written, before any alias in the
  /// chain has been followed.
  Shared<TypeAst> Written;

  /// The resolved type that came out of the core alias
  /// analyser.
  Shared<TypeAst> Resolved;

  /// The parameters the alias declares itself: the "T" of
  /// "type MyVec[T] = Vec[T]".
  Shared<GenericParameterGroupAst> Params;

  /// The scope the final target was found in, which is where
  /// an instantiation of this alias is attached.
  Scope *TrackingScope = nullptr;

  /// The scope the alias was written in.
  Scope *DeclScope = nullptr;

  /// Whether a "use" statement produced this alias; generics
  /// propagate differently along such a link.
  bool FromUseStmt = false;

  /// Whether the "Params" was adopted from the target rather
  /// then written on the alias itself (like being carried
  /// through from a "use" statement). This modified how generic
  /// and constraint resolution is performed, scope-wise.
  bool ParamsFromTarget = false;

  /// The statement this describes, for diagnostics.
  TypeStatementAst *Stmt = nullptr;
};

SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX;

  /// The name of the type symbol, provided from the type it
  /// represents.
  Shared<TypeIdentifierAst> Name;

  /// The class prototype that this symbol is for, bound when
  /// the class is analysed.
  ClassPrototypeAst *Type;

  /// The scope representing the type. This scope's "TySym" is
  /// this symbol. Simple 2-way pointer link.
  Scope *LinkedScope;

  /// The scope that this symbol was defined in, almost never the
  /// "LinkedScope". If "Vec[U]" is created, "U" will be in the
  /// "ScopeDefinedIn" scope, and no-where else, so track it.
  Scope *ScopeDefinedIn;

  // Todo: Remove, I think
  Scope *ScopeModule;

  /// The kind of type symbol: a class, an alias, "Self", a
  /// generic parameter or argument, or a function / closure mock.
  TypeKind Kind;

  /// Whether this symbol names a variadic generic parameter
  /// like "..Ts" or not. Required for monomorphisation and
  /// overload management.
  bool IsVariadic = false;

  /// Whether this symbol names a concrete type all the way down;
  /// are all the generic arguments concrete etc.
  bool IsConcrete = true;

  /// For a generic symbol, the constraints that are used on that
  /// generic, so they can be re-pulled for inference validation.
  Vec<Shared<TypeAst>> GenericConstraints;

  /// For generic-to-generic bindings, we need to store the lexical
  /// value of the next generic symbol.
  Shared<TypeAst> GenericVal;

  /// There is sometimes a case where we want to derive information
  /// off of another symbol, such as a generic base might be copyable
  /// so any instantiation also needs to be.
  Shared<TypeSymbol> DerivesFromSym;

  /// The visibility tag on the type symbol, like for the variable
  /// symbol/ Needed for checking this type is accessible outside
  /// it's module or type (sup-defined).
  asts::utils::Visibility Visibility;

  /// The symbol's type convention. Todo: Why is needed again
  /// as opposed to how it gets used with "&" tokens etc.
  Unique<ConventionAst> Convention;

  /// The LLVM metadata for this type, used during stage 10 and 11
  /// of the compilation pipeline. Currently, tracks the LLVM type
  /// and the field index map for S++ layout convention.
  Shared<codegen::LlvmTypeSymInfo> LlvmInfo;

  /// Set when this symbol names an alias rather than a class. It
  /// contains all the alias information required.
  Shared<AliasInfo> Alias;

  /// Track whether this type has been marked as copyable, with
  /// the Copy type superimposition.
  bool IsDirectlyCopyable = false;

  /// Track whether this type has been marked as zero-type, with
  /// the "!zero_type" annotation.
  bool IsDirectlyZeroType = false;

  /// Track whether this type has been marked as a thread hazard
  /// with the "!thread_hazard" annotation.
  bool IsDirectlyThreadHazard = false;

  /// The cached fully qualified name of this symbol. This is an
  /// expensive operation given how often it's used, so we can
  /// cache it until a cache generation bump occurs.
  mutable Shared<TypeAst> _CachedFqName;
  mutable std::uint64_t _CachedFqNameGen = 0;

  TypeSymbol(
    Shared<TypeIdentifierAst> name,
    ClassPrototypeAst *type,
    Scope *scope,
    Scope *scope_defined_in,
    Scope *scope_module,
    TypeKind kind,
    bool is_directly_copyable = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPrivate,
    Unique<ConventionAst> &&convention = nullptr,
    Vec<Shared<TypeAst>> const &generic_constraints = {});

  TypeSymbol(TypeSymbol const &that);

  /// Whether this is a generic type parameter, bound or not.
  SPP_ATTR_NODISCARD auto IsTypeGeneric() const -> bool;

  /// Whether this is a compiler-generated "$" mock class: a
  /// function's or a closure's.
  SPP_ATTR_NODISCARD auto IsMock() const -> bool;

  /// Whether this stands for "Self": a class / "sup" block / alias
  /// "Self", or the generic argument an instantiation registers when
  /// overload resolution pins "Self" to the receiver - which is also
  /// a generic argument, so has that kind, but is named "Self".
  SPP_ATTR_NODISCARD auto IsSelf() const -> bool;

  ~TypeSymbol() override;

  /// Only if this is an alias symbol do we want to do the
  /// deep copy, because of the rewrite per instantiation.
  /// Like with "type T = ,,," becomes "type T = Str" etc.
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  /// Whether this symbol is directly copyable, or it has
  /// inherited copyability from the derives-from symbol.
  SPP_ATTR_NODISCARD auto IsCopyable() const -> bool;

  /// Whether this symbol is directly zero-type, or it has
  /// inherited zero-typedness from the derives-from symbol.
  SPP_ATTR_NODISCARD auto IsZeroType() const -> bool;

  /// This is more complex. If this type is a direct thread
  /// hazard, it inherits a thread-hazardous property, or any
  /// of its fields are a type hazard - then this symbol is a
  /// thread hazard type. Opposite: thread safe.
  SPP_ATTR_NODISCARD auto IsThreadSafe() const -> bool;

  /// Equality is done by pointer comparison. We only want to
  /// know if two pointers are the same effectively.
  auto operator==(TypeSymbol const &that) const -> bool;

  /// Almost always is the "LinkedScope->TySym" this symbol;
  /// but for "Self", there isn't the link, so we pull the
  /// type symbol from the linked scope. This is because "Self"
  /// has a nullptr type, which is required for "Self" to match
  /// when they are on different sup-levels (ie override match).
  SPP_ATTR_NODISCARD auto AsClassSymbol() const -> TypeSymbol*;

  /// When this is a generic, link through the "LinkedScope" to
  /// get the actual scope, then get the "TySym" from there. It
  /// resolves continuously, until we arrive at a non-generic,
  /// resolving though any amount of generic links.
  SPP_ATTR_NODISCARD auto AsBoundSymbol() const -> TypeSymbol*;

  /// Discard this symbol's cached fully qualified name. Needed
  /// when the "LinkedScope" is re-pointed after the symbol has
  /// been built, which changes the chain the name is read off
  /// without moving any scope in the tree.
  auto InvalidateFqNameCache() const -> void;

  /// Qualify this type name with the scope it belongs too,
  /// produces a fully namespaced type name, subject to some
  /// generic, alias, and $Type rules. There is a rare occasion
  /// where we don't want to qualify the $Types, so we have a
  /// flag for it.
  SPP_ATTR_NODISCARD auto FqName(bool ignore_dollar = false) const -> Shared<TypeAst>;

  /**
   * The type this symbol stands for where it is written.
   *
   * @n
   * For anything but a generic parameter that is simply its fully qualified name. A parameter is the interesting case:
   * an instantiation binds it, and a type written in terms of it - the @c "Pass[T]" of @c "is Pass[T](val)" - stands
   * for what it was bound to once that body is being analysed as the instantiation's own. @c FqName cannot answer
   * that, because a parameter's own name is exactly what it has to keep giving back while the template is analysed in
   * its own terms, and both readings come through the same symbol.
   *
   * @n
   * An unbound parameter has nothing to follow and stands for itself, which is what leaves a template's body written
   * the way its author wrote it. A parameter bound to another parameter (@c "T=T", passing an enclosing body's
   * parameter along) is that same case reached the long way round, and gives back the name it was bound to.
   *
   * @return The bound type, or this parameter's own name when it is unbound.
   */
  SPP_ATTR_NODISCARD auto BoundName() const
    -> Shared<asts::TypeAst>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::Symbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::NamespaceSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::VariableSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::TypeSymbol)
