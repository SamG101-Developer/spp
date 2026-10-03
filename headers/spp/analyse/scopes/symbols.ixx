module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.analyse.scopes.instance_key;
import spp.analyse.utils.memory_state;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.utils.visibility;
import spp.codegen.llvm_sym_info;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct AliasInfo);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct Symbol);
use(spp::analyse::scopes, struct NamespaceSymbol);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::scopes, enum class VariableKind);
use(spp::analyse::scopes, enum class TypeKind);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypeStatementAst);

/// What a type resolves to: the symbol of the type, and how
/// a value of it is held. The convention is not part of the
/// symbol ("&mut Str" and "Str" are one type symbol, borrowed
/// differently), and "!" is marked rather than resolved: it
/// names the "Never" class, but fits anywhere that class would
/// not.
SPP_EXP_CLS struct spp::analyse::scopes::TypeRef {
  /// No type: what an expression with none, or a name that did not
  /// resolve, answers with.
  TypeRef() = default;

  /// The type's symbol, as the lookup found it; null when the type
  /// resolved to none. Identity is "Id", not this: two symbols can be
  /// one type (a binding and what it is bound to).
  TypeSymbol *Symbol = nullptr;

  /// How the value is held. "MOV" is no convention, as
  /// "ConventionAst" compares it.
  ConventionTag Conv = ConventionTag::MOV;

  /// Whether the type is "!".
  bool IsNever = false;

  /// The type's identity without its convention: two references are
  /// one type exactly when these are equal and so are their
  /// conventions. Present whenever "Symbol" is.
  TypeId Id = nullptr;

  /// A written or inferred type, resolved where "scope" reads
  /// it.
  static auto Of(TypeAst const &type, Scope const &scope) -> TypeRef;

  /// A symbol already resolved - a class, a super class, a
  /// template, a parameter, a binding - as a reference: its identity
  /// read through "scope"'s bindings, held under "conv", else as the
  /// symbol is held ("TypeSymbol::HeldConvention": a binding's own).
  static auto Of(
    TypeSymbol &sym, Scope const &scope, std::optional<ConventionTag> conv = std::nullopt, bool make = true) -> TypeRef;

  /// "sym" as a kind check reads it ("IsA", "Template", "type_predicates::IsType*"): by value (a binding to "&Vec" is
  /// a "Vec"), and read without making anything, which a template check never needs and must not do while the "sup"
  /// scopes are being attached.
  static auto OfKind(TypeSymbol const &sym, Scope const &scope) -> TypeRef;

  /// "OfKind" for a type's own scope: its type ("Scope::LinkedTypeSymbol", which it must have), read there.
  static auto OfKind(Scope const &scope) -> TypeRef;

  /// What an identity names where "scope" reads it: the symbol filed under it, made there first if it is not yet.
  /// Held under "conv", else the convention the identity carries; "!" when "never", else when it names the "Never"
  /// class (an argument's identity, "ArgsOf", is either).
  static auto Of(
    TypeId id, Scope const &scope, std::optional<ConventionTag> conv = std::nullopt,
    std::optional<bool> never = std::nullopt) -> TypeRef;

  /// This type read where "scope" reads it ("Scope::ReadIn"): a parameter as that scope binds it, an open
  /// instantiation under its bindings; its convention and "!" kept. A symbol's own scope is not read through (it binds
  /// the symbol's parameters to its arguments, which would read one level further each time). Made there first, when
  /// "make" and it is not yet.
  SPP_ATTR_NODISCARD auto ReadIn(Scope const &scope, bool make = false) const -> TypeRef;

  /// "ReadIn" with "subst" applied to this type's identity first ("SubstituteTypeId"): this type, written in the terms
  /// of the parameters "subst" binds, read where "scope" reads it.
  SPP_ATTR_NODISCARD auto Substitute(GenericSubst const &subst, Scope const &scope, bool make = false) const -> TypeRef;

  /// The template this type instantiates, off its identity ("HeadOf"): "Vec" for "Vec[Str]", the variant template for
  /// a variant, what a binding, parameter or "Self" stands for, and a type that instantiates nothing itself (or a
  /// symbol whose identity does not resolve). Null for no type.
  SPP_ATTR_NODISCARD auto Template() const -> TypeSymbol*;

  /// Whether this type instantiates the precompiled template "tmpl" ("std::tuple::Tup", ...), as "Template" reads it.
  /// The template's own symbol is found once from "scope", and cached for the compile ("PrecompiledTemplate"), so
  /// "tmpl" must be one of "common_types_precompiled"'s.
  SPP_ATTR_NODISCARD auto IsA(TypeAst const &tmpl, Scope const &scope) const -> bool;

  /// Whether "that" is the same type held the same way: the same identity and convention. No type is the same as
  /// nothing.
  SPP_ATTR_NODISCARD auto SameAs(TypeRef const &that) const -> bool;

  /// This type as a type ast of its own, under its convention: its symbol's qualified name, else what its identity
  /// names where "scope" reads it ("Scope::TypeAstOf"). Null for no type.
  SPP_ATTR_NODISCARD auto AstIn(Scope const &scope) const -> Shared<TypeAst>;

  SPP_ATTR_NODISCARD auto IsBorrowed() const -> bool {
    return Conv != ConventionTag::MOV;
  }

  /// The same type held by value ("Str" for "&Str"), without the
  /// clone a type's own "WithoutConvention" makes.
  SPP_ATTR_NODISCARD auto WithoutConvention() const -> TypeRef {
    auto out = *this;
    out.Conv = ConventionTag::MOV;
    return out;
  }

  /// The same type held under "conv".
  SPP_ATTR_NODISCARD auto WithConvention(const ConventionTag conv) const -> TypeRef {
    auto out = *this;
    out.Conv = conv;
    return out;
  }

  /// The symbol a kind check ("type_predicates") reads: none for
  /// a borrow or "!", which no template matches.
  SPP_ATTR_NODISCARD auto KindSymbol() const -> TypeSymbol* {
    return IsBorrowed() or IsNever ? nullptr : Symbol;
  }

private:
  /// The one way to a reference with a type: "resolved" is what the
  /// lookup found, "id" its identity where it was found.
  TypeRef(TypeSymbol *resolved, TypeId id, ConventionTag conv, bool never);

  /// A reference to what "id" names where "scope" reads it: the
  /// symbol filed under it, made there first (when "make") if it is
  /// not yet and "open" (the open instantiation a lookup reached)
  /// can be; "open" itself where the identity names no one symbol.
  static auto Named(
    Scope const &scope, TypeId id, TypeSymbol *open, ConventionTag conv, bool never, bool make = true) -> TypeRef;
};

namespace spp::analyse::scopes {
  /// The template a precompiled template type ("std::tuple::Tup") names, found from "scope" on first query and cached
  /// for the compile ("common_types_precompiled::TEMPLATE_SYMBOLS"). What "TypeRef::IsA" compares against; also usable
  /// while an identity is being built, as it only looks a name up.
  SPP_EXP_CLS auto PrecompiledTemplate(asts::TypeAst const &tmpl, Scope const &scope) -> TypeSymbol*;
}

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
  /// symbol will be the "LinkedNamespaceSymbol" on the linked scope.
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
  Capture, // A closure's capture, owned by the closure's environment. Not "Local": the linear and "is" checks skip it.
  FlowNarrowing, // A narrowed view of another symbol's value, from a case pattern. Not "Local", as "Capture".
  Attribute, // A class attribute, reached with ".".
  Constant, // A "cmp" constant, reached with "::".
  FnMock, // A function's "$" mock constant, reached with "." or "::".
  GnCompParam, // A comp generic parameter, not yet bound.
  GnCompArg, // A comp generic bound to an argument.
};

/// What a type symbol names, the counterpart of "VariableKind".
/// Whether a generic is bound, and to what, is still read off
/// its "LinkedScope" and "BoundTypeVal" - a binding to another
/// generic behaves like an unbound one, which no kind captures.
SPP_EXP_CLS enum class spp::analyse::scopes::TypeKind {
  Cls, // A class, or an instantiation of one.
  Alias, // A "type" statement or a "use" of a type; see "AliasInfo".
  Self, // The "Self" of a class, "sup" block or alias.
  GnTypeParam, // A generic type parameter, not yet bound.
  GnTypeArg, // A generic type parameter bound to an argument.
  FnMock, // A function's "$" mock class, which its overloads are superimposed over.
  ClosureMock, // A closure's "$closure" mock class, with its function type attached directly.
};

namespace spp::analyse::scopes {
  /// A fresh identity for a generic parameter's symbol ("ParamId"),
  /// never zero. Declared "extern C++" like the rest of this module,
  /// whose definitions sit in the global module ("SPP_MOD_BEGIN").
  SPP_EXP_CLS auto NextGnParamId() -> std::uint64_t;

  /// File a generic type parameter under its "ParamId", so the
  /// "TypeId" of a type naming it can answer with it
  /// ("Scope::TypeSymbolOf"). Ids are never reused within a process.
  SPP_EXP_CLS auto RegisterGnTypeParam(TypeSymbol &param) -> void;

  /// The generic type parameter filed under "id", if any.
  SPP_EXP_CLS SPP_ATTR_HOT auto GnTypeParamOf(std::uint64_t id) -> TypeSymbol*;

  /// File an instance under its identity in the template its identity is headed by ("TypeSymbol::Instances"), so a
  /// lookup by that identity ("Scope::TypeSymbolOf") finds it however it is spelled.
  SPP_EXP_CLS auto FileInstance(TypeId id, TypeSymbol &instance) -> void;

  /// The parameters a parameter group declares, by identity, as "ParamsOf" lists a type's.
  SPP_EXP_CLS auto ParamsOfGroup(asts::GenericParameterGroupAst const &params) -> TypeIdParams;

  /// Bind the parameters "params" lists to the arguments of the same names in "args" (an instantiation's arguments'
  /// key, as "Scope::ArgsIdOf" makes one), as an instantiation's arguments name the parameters of what it
  /// instantiates. With "all", nothing unless every one is bound; otherwise a parameter with no argument of its name
  /// is left out.
  SPP_EXP_CLS auto BindByName(TypeIdParams const &params, TypeId args, bool all) -> std::optional<GenericSubst>;

  /// Forget every registered parameter, of both kinds: they belong to one compilation's scopes, which are freed
  /// after it ("ScopeManager::Cleanup"). Ids are not reused, so nothing later asks for one.
  SPP_EXP_CLS auto ClearGnParams() -> void;

  /// As "RegisterGnTypeParam", for a comp parameter.
  SPP_EXP_CLS auto RegisterGnCompParam(VariableSymbol &param) -> void;

  /// The comp parameter filed under "id", if any.
  SPP_EXP_CLS auto GnCompParamOf(std::uint64_t id) -> VariableSymbol*;

  /// The identity a type symbol stands for wherever it is named, as a type written naming it means it: a parameter
  /// (or a binding of one) is that parameter, read through the reader's bindings ("Scope::ReadIn"); "Self" is "Self";
  /// an instantiation is the identity it is filed under; anything else (a class, a template, an alias) is itself.
  SPP_EXP_CLS auto WrittenTypeIdOf(TypeSymbol const &sym) -> TypeId;

  /// The identity a comp parameter (or a binding of one) stands for wherever its name is written: that parameter's
  /// comp identity ("C<ParamId>"), as a type parameter's is its "TypeParam" ("WrittenTypeIdOf").
  SPP_EXP_CLS auto WrittenCompIdOf(VariableSymbol const &sym) -> TypeId;
}

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

  /// "Type" resolved where "scope" reads it ("TypeRef::Of");
  /// empty while there is no type.
  SPP_ATTR_NODISCARD auto TypeRefIn(Scope const &scope) const -> TypeRef;

  /// The scope that this symbol was defined in (this will happen
  /// to be the scope that the symbol resides in, which is not
  /// always the case for type symbols).
  Scope *ScopeDefinedIn;

  /// The kind of variable symbol, such as a comptime constant,
  /// a capture, a flow types narrower, etc.
  VariableKind Kind;

  /// On a comp generic parameter, its own identity: non-zero, unique to its declaration and kept by every copy of the
  /// symbol (the name alone is shared by every parameter spelled the same). Zero on anything else.
  std::uint64_t OwnParamId = 0;

  /// On a binding, the identity of the parameter it binds, so a lookup finds the binding of one particular parameter
  /// rather than of whatever shares its name. Zero on anything else.
  std::uint64_t BindsParamId = 0;

  /// The parameter this symbol is or binds: "OwnParamId" on a parameter, "BindsParamId" on a binding, zero for
  /// neither. The one way to ask; the two fields are only set.
  SPP_ATTR_NODISCARD auto ParamId() const -> std::uint64_t {
    return OwnParamId != 0 ? OwnParamId : BindsParamId;
  }

  /// Whether this symbol names a variadic comp generic parameter
  /// like "..n", or binds one. Its type is one element's ("Bool"
  /// for "..n: Bool"), so this is what says it holds a tuple.
  bool IsVariadic = false;

  /// Whether the symbol is mutable, ie can the variable it
  /// represents be re-assigned a new value.
  bool IsMutable = false;

  /// For a "use" import, the symbol it names (its target). This
  /// is nullptr for anything not imported, and for an import
  /// before stage 3 - whose kind is "Import" until then.
  Shared<VariableSymbol> AliasSymbol;

  /// For flow typing, store the variable that this symbol has
  /// narrowed down from. If this is a variable of "Some[S32]",
  /// the "NarrowsSymbol" might be the "Opt[S32]" type variable.
  Shared<VariableSymbol> NarrowsSymbol;

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
  Unique<utils::memory_state::MemoryInfo> MemInfo;

  /// The LLVM symbol information used during stage 10 and 11 of
  /// the compilation pipeline. Currently, tracks the "alloca".
  Shared<codegen::LlvmVarSymbolInfo> LlvmInfo;

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
  SPP_ATTR_NODISCARD auto BoundCompVal() const -> ExpressionAst*;

  /// The comp generic at the end of this one's chain of bindings to other comp generics, as each is named where read
  /// ("scope"): the generic it is bound through to, whose value ("BoundCompVal") is a value or nothing. The comp analog
  /// of "TypeSymbol::AsBound"; itself for anything that is not bound to another generic.
  SPP_ATTR_NODISCARD auto AsBound(Scope const &scope) const -> VariableSymbol const*;

  /// Whether this is a comp generic, bound or not.
  SPP_ATTR_NODISCARD auto IsGn() const -> bool;

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

  /// The scope its target is written in, where every reader reads it ("TypeSymbol::AliasTarget",
  /// "TypeSymbol::AliasTargetId"): the statement's own scope, which declares its parameters, or for a copy made per
  /// instantiation of a generic block ("CreateGnSupScope"), that instantiation's scope.
  Scope *WrittenIn = nullptr;

  /// Whether a "use" statement produced this alias; generics
  /// propagate differently along such a link.
  bool IsFromUseStmt = false;

  /// Whether the "Params" was adopted from the target rather
  /// then written on the alias itself (like being carried
  /// through from a "use" statement). This modified how generic
  /// and constraint resolution is performed, scope-wise.
  bool IsParamsFromTarget = false;

  /// The statement this describes, for diagnostics.
  TypeStatementAst *Stmt = nullptr;

  /// The scope the alias symbol is declared in, which an instantiation of it is attached to: the one enclosing the
  /// statement's own scope, or a per-instantiation copy's own scope ("WrittenIn").
  SPP_ATTR_NODISCARD auto DeclaredIn() const -> Scope*;
};

SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX;

  /// The name of the type symbol, provided from the type it
  /// represents.
  Shared<TypeIdentifierAst> Name;

  /// The class prototype that this symbol is for, bound when
  /// the class is analysed.
  ClassPrototypeAst *Type;

  /// The scope representing the type. This scope's "LinkedTypeSymbol" is
  /// this symbol. Simple 2-way pointer link.
  Scope *LinkedScope;

  /// The scope that this symbol was defined in, almost never the
  /// "LinkedScope". If "Vec[U]" is created, "U" will be in the
  /// "ScopeDefinedIn" scope, and no-where else, so track it.
  Scope *ScopeDefinedIn;

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

  /// On a generic type parameter, its own identity: non-zero, unique to its declaration and kept by every copy of the
  /// symbol (the name alone is shared by every parameter spelled the same). Zero on anything else.
  std::uint64_t OwnParamId = 0;

  /// On a binding, the identity of the parameter it binds, so a lookup finds the binding of one particular parameter
  /// rather than of whatever shares its name. Zero on anything else.
  std::uint64_t BindsParamId = 0;

  /// The parameter this symbol is or binds: "OwnParamId" on a parameter, "BindsParamId" on a binding, zero for
  /// neither. The one way to ask; the two fields are only set.
  SPP_ATTR_NODISCARD auto ParamId() const -> std::uint64_t {
    return OwnParamId != 0 ? OwnParamId : BindsParamId;
  }

  /// For an instantiation, the template it instantiates, and its
  /// identity ("Scope::InstanceIdOf") it is filed under in the
  /// template's "Instances". Spelling does not identify an
  /// instantiation: "Box[T]" over two different "T"s is two types,
  /// and "Vec[S32]" or "S32 or Bool" however it is written is one.
  TypeSymbol *InstanceOf = nullptr;
  TypeId Id = nullptr;

  /// For a template, its instantiations by identity.
  Map<TypeId, TypeSymbol*> Instances;

  /// For a generic symbol, the constraints that are used on that
  /// generic, so they can be re-pulled for inference validation.
  Vec<Shared<TypeAst>> TypeConstraints;

  /// For generic-to-generic bindings, we need to store the lexical
  /// value of the next generic symbol.
  Shared<TypeAst> BoundTypeVal;

  /// On a binding made to an alias whose target was not made yet
  /// (an instantiation during the alias stage, "U8" before its
  /// "SizedInteger[w=8, ..]" exists), the alias: the binding links
  /// what the alias's target reached then - its template - and is
  /// re-linked to the target once it resolves ("Rebind").
  TypeSymbol *BoundAlias = nullptr;

  /// There is sometimes a case where we want to derive information
  /// off of another symbol, such as a generic base might be copyable
  /// so any instantiation also needs to be.
  Shared<TypeSymbol> DerivesFromSymbol;

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
  Shared<codegen::LlvmTypeSymbolInfo> LlvmInfo;

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
    TypeKind kind,
    bool is_directly_copyable = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPrivate,
    Unique<ConventionAst> &&convention = nullptr,
    Vec<Shared<TypeAst>> const &generic_constraints = {});

  TypeSymbol(TypeSymbol const &that);

  /// Whether this is a generic type parameter, bound or not.
  SPP_ATTR_NODISCARD auto IsGn() const -> bool;

  /// Whether this is a compiler-generated "$" mock class: a
  /// function's or a closure's.
  SPP_ATTR_NODISCARD auto IsMock() const -> bool;

  /// Whether this stands for "Self": a class / "sup" block / alias
  /// "Self", or the generic argument an instantiation registers when
  /// overload resolution pins "Self" to the receiver - which is also
  /// a generic argument, so has that kind, but is named "Self".
  SPP_ATTR_NODISCARD auto IsSelf() const -> bool;

  /// Whether this is a generic class template itself ("Vec"), not an
  /// instance of one nor an alias: never a type on its own, only what
  /// an instance is made from. Something that resolves to one before
  /// its instance is made has not resolved yet.
  SPP_ATTR_NODISCARD auto IsBareTemplate() const -> bool;

  /// Whether a generic argument can be bound to this ("BindTo"): a
  /// type made under its own name - not an alias, and concrete or an
  /// instance (open or closed) of a template, not a template still
  /// waiting on its instance.
  SPP_ATTR_NODISCARD auto IsBindTarget() const -> bool {
    return Alias == nullptr and (IsConcrete or InstanceOf != nullptr);
  }

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

  /// The symbol the scope this one links belongs to ("LinkedScope->LinkedTypeSymbol"): a class for itself, the class a
  /// binding or "Self" stands for, an alias's target. Itself where it links none.
  SPP_ATTR_NODISCARD auto LinkedSymbol() const -> TypeSymbol*;

  /// The generic parameters a name of this symbol takes: an alias's own ("AliasInfo::Params"), else its class's. Null
  /// for neither (a generic, a symbol with no prototype).
  SPP_ATTR_NODISCARD auto GnParams() const -> GenericParameterGroupAst*;

  /// What this symbol stands for: a binding, a parameter or "Self" followed through the scopes it links
  /// ("LinkedSymbol") to the symbol it names, a binding still waiting on its alias's target re-linked first
  /// ("Rebind"); anything else is itself. Capped against "Self" stand-ins naming each other.
  SPP_ATTR_NODISCARD auto AsBound() const -> TypeSymbol*;

  /// Re-link a binding made to an alias ("BoundAlias") to what the
  /// alias's target resolves to, once it does; once re-linked, it
  /// stays. Nothing for any other symbol. A lazy link: the one
  /// mutation reading a binding makes ("AsBound", "FqName", keying a
  /// binding), idempotent, and no identity depends on whether it has
  /// run yet.
  auto Rebind() const -> void;

  /// Link an alias to the type it stands for: the same type under
  /// another name, so it shares that type's class, scope and lowered
  /// form, and derives from it.
  auto LinkTo(TypeSymbol const &target) -> void;

  /// Bind a generic argument to the type it is bound to: a parameter
  /// standing for that type, so it reads its class, scope,
  /// copyability, zero size and constraints, and keeps its own name
  /// and lowered form.
  auto BindTo(TypeSymbol const &target) -> void;

  /// What this symbol stands for once aliases are looked through:
  /// the symbol an alias's recorded target resolves to, followed
  /// until it is no alias. The one rule every alias reader uses: the
  /// target is read where it is written ("AliasInfo::WrittenIn"),
  /// else in the scope the alias links, else in "scope". Made where
  /// it is not yet ("TypeRef::Of"), unless "make" is false (a
  /// reader that runs while keying, which makes nothing). An alias
  /// not resolved yet, or anything else, is itself.
  SPP_ATTR_NODISCARD auto AliasTarget(Scope const &scope, bool make = true) const -> TypeSymbol*;

  /// What this alias stands for, by identity: its target as recorded
  /// when its statement was resolved, or before then (an alias
  /// instantiated by an earlier declaration's stage 4) its target
  /// keyed where it is written. In the alias's own parameters' terms,
  /// unless "args" (an instantiation's argument list,
  /// "Scope::ArgsIdOf") binds them: what that instantiation stands
  /// for ("Res[T=S32, E=Str]" is "Pass[S32] or Fail[Str]"). Null for
  /// no alias, or a target that does not resolve.
  SPP_ATTR_NODISCARD auto AliasTargetId(TypeId args = nullptr) const -> TypeId;

  /// What a "use" names, followed through each "use" in turn:
  /// the template (or "type" alias) at the end, whose parameters
  /// the "use" declares again under the same names. Anything
  /// that is not a "use" is itself.
  SPP_ATTR_NODISCARD auto UseTarget() const -> TypeSymbol*;

  /// The convention a type naming this symbol is held under: the one "written" with it, else a binding's own ("T"
  /// bound to "&mut Str" is held as "&mut Str"), else by value.
  SPP_ATTR_NODISCARD auto HeldConvention(asts::TypeAst const *written = nullptr) const -> ConventionTag;

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
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto FqName(bool ignore_dollar = false) const -> Shared<TypeAst>;

  /// "TypeArg" for a comp argument: the value it is, read off the
  /// identity of the instantiation this stands for ("CompArgId") and
  /// named as an ast ("Scope::CompAstOf", a closed value as its
  /// literal); null if there is none.
  SPP_ATTR_NODISCARD auto CompArg(Str const &name) const -> Shared<ExpressionAst>;

  /// "TypeArgRef" for a comp argument: its identity, read off the
  /// instantiation's (no ast built); zero if there is none.
  SPP_ATTR_NODISCARD auto CompArgId(Str const &name) const -> CompId;

  /// The type an instantiation's type argument "name" is (convention, "Self" and all), read off the identity of the
  /// instantiation this stands for (an alias's target, a binding's bound type, "Self"'s class) and pointing where the
  /// argument was written; null if there is none. Its symbol is "TypeRef::Of" it, read in the instantiation's scope.
  SPP_ATTR_NODISCARD auto TypeArg(Str const &name) const -> Shared<TypeAst>;

  /// That type argument as a "TypeRef", read off the identity in the instantiation's scope (no ast built); no type if
  /// there is none.
  SPP_ATTR_NODISCARD auto TypeArgRef(Str const &name) const -> TypeRef;

  /// Every type argument of that instantiation, in order (a tuple's stay positional, with no names to read).
  SPP_ATTR_NODISCARD auto TypeArgs() const -> Vec<Shared<TypeAst>>;

  /// This type as a pattern: a template (named as written,
  /// "Vec") over its own parameters ("Vec[T=T]"), and any other
  /// type as its own name. What "Self" means inside a template,
  /// and what the sup blocks written over it are matched against.
  SPP_ATTR_NODISCARD auto GnSelfName() const -> Shared<TypeAst>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::Symbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::NamespaceSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::VariableSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::TypeSymbol)
