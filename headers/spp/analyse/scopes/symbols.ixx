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

namespace spp::asts {
  SPP_EXP_CLS struct AnnotationAst;
  SPP_EXP_CLS struct ClassPrototypeAst;
  SPP_EXP_CLS struct ConventionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct TypeStatementAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct AliasInfo;
  SPP_EXP_CLS struct Symbol;
  SPP_EXP_CLS struct NamespaceSymbol;
  SPP_EXP_CLS struct TypeSymbol;
  SPP_EXP_CLS struct VariableSymbol;
}

/**
 * The base Symbol type for all symb variations to inherit from. This provides a common interface for all symbols, and
 * some abstract methods that must be implemented by all derived classes. The `@c Symbol* type is used, creating the
 * need for a base class.
 */
SPP_EXP_CLS struct spp::analyse::scopes::Symbol : EnableLocalSharedFromThis<Symbol> {
  SPP_GCC_VTABLE_FIX_BASE

  /**
   * Enforce a virtual destructor for the Symbol class. This is to ensure that derived classes can be properly
   * destructed when deleted through a base class pointer. This is important for polymorphism and memory management,
   * as it allows for proper cleanup of resources when a derived class is deleted.
   */
  virtual ~Symbol();

  /**
   * Whether a generic instantiation cloning a scope has to be given its own copy of this symbol, or can share the
   * template's. Only what monomorphization goes on to rewrite, or what carries per-instantiation state, needs its own;
   * everything else is written once and read from every instantiation. See @c IndividualSymbolTable::DeepCopyFrom .
   * @return Whether this symbol must be copied rather than shared.
   */
  SPP_ATTR_NODISCARD virtual auto NeedsDeepCopy() const -> bool = 0;

  /**
   * Obtain a @c Shared owning pointer to this symbol, downcast to the requested derived symbol type. Symbols are
   * always created via @c MakeShared and stored in symbol tables, so the enclosing control block is guaranteed to
   * exist; this mints an owning pointer lazily at the call sites that actually need shared ownership, without the
   * ancestor-walking traversals having to copy @c Shared pointers per element.
   * @tparam Derived The concrete symbol type to downcast to (defaults to @c Symbol for no downcast).
   * @return A @c Shared pointer to this symbol as @c Derived.
   */
  template <typename Derived = Symbol>
  SPP_ATTR_NODISCARD auto SharedFromThis() -> Shared<Derived> {
    if constexpr (std::is_same_v<Derived, Symbol>) {
      return shared_from_this();
    }
    else {
      return spp::static_shared_cast<Derived>(shared_from_this());
    }
  }
};

SPP_EXP_CLS struct spp::analyse::scopes::NamespaceSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX

  Shared<asts::IdentifierAst> Name;

  Scope *LinkedScope;

  NamespaceSymbol(
    Shared<asts::IdentifierAst> name,
    Scope *scope);

  NamespaceSymbol(
    NamespaceSymbol const &that);

  ~NamespaceSymbol() override;

  /**
   * A namespace is the same namespace from inside every instantiation, and nothing about one is rewritten by a
   * substitution, so it is always shared. It is unlikely it is ever in a place that needs copying, but for future
   * namespace aliasing, is included.
   */
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  auto operator==(
    NamespaceSymbol const &that) const
    -> bool;
};

SPP_EXP_CLS struct spp::analyse::scopes::VariableSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX

  Shared<asts::IdentifierAst> Name;

  Shared<asts::TypeAst> Type;

  Scope *ScopeDefinedIn;

  bool IsMutable = false;

  bool IsGeneric = false;

  bool IsFlowNarrowing = false;

  /**
   * Whether this symbol is a closure's capture. A capture is owned by the closure's environment rather than by the
   * body that reads it - the environment is read again on every call - so the body is not obliged to consume it. The
   * closure value carries that obligation instead.
   */
  bool IsCapture = false;

  /**
   * For a flow-narrowing symbol, the symbol it narrows: same name, same storage, wider type. Consuming through the
   * narrowed name discharges the value itself, so a move recorded against this symbol is recorded against that one
   * too - otherwise the original reads as live and is reported as never discharged at whatever exit follows.
   */
  Shared<VariableSymbol> NarrowsSym;

  /**
   * The functional type this symbol is called through, when that is not simply its own type. A parameter declared
   * against a generic ("mut pred: F", with "F: FunMov") is callable through what its constraint promised, whatever
   * the instantiation substituted for it - and a "FunMut" satisfies a "FunMov" constraint while also being callable
   * through a borrow. Deciding from the substituted type instead made the same body consume the value in one
   * instantiation and borrow it in another, which linear ownership cannot account for: the body would have to discard
   * the value in one and must not in the other. Null for everything else, which is called through its own type.
   */
  Shared<const asts::TypeAst> CallableAsType;

  asts::utils::Visibility Visibility;

  asts::AnnotationAst *VisibilityAnnotation = nullptr;

  Unique<utils::mem_info_utils::MemoryInfo> MemInfo;

  Shared<codegen::LlvmVarSymInfo> LlvmInfo;

  Unique<asts::Ast> CompTimeValue;

  Shared<VariableSymbol> AliasSym;

  VariableSymbol(
    Shared<asts::IdentifierAst> name,
    Shared<asts::TypeAst> type,
    Scope *ScopeDefinedIn,
    bool is_mutable = false,
    bool is_generic = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPrivate);

  VariableSymbol(
    VariableSymbol const &that);

  ~VariableSymbol() override;

  /**
   * A variable symbol carries state that belongs to one instantiation and not to the template: the memory state the
   * memory checker writes, and the alloca (or global) code generation gives it. Two instantiations sharing one would
   * be two functions sharing one stack slot, so a variable symbol is always copied.
   */
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  auto operator==(
    VariableSymbol const &that) const
    -> bool;

  SPP_ATTR_NODISCARD auto FqName() const
    -> Shared<asts::ExpressionAst>;

  /**
   * The value this symbol's generic parameter was bound to, for a comp generic that has been given an argument. A
   * bound type generic's binding is reachable from its symbol's @c TypeSymbol::FqName; a comp generic's lives on the
   * comp-time ast @c type_utils::CreateGenericSym left behind, encoded as the argument itself for an instantiation
   * and as the parameter for a template. This is the one place that knows that encoding.
   * @return The bound value, or @c nullptr if this symbol is not a comp generic, or is one that is still unbound.
   */
  SPP_ATTR_NODISCARD auto BoundCompValue() const
    -> asts::ExpressionAst*;
};

/**
 * Everything an alias is: the type it was written as, the type that turns out to be, and the parameters it declares
 * of its own. An alias is transparent - it introduces a second name for a type rather than a type of its own - so
 * @c Resolved is what every consumer of the symbol actually means by it, and is the only field most of them read.
 *
 * @n
 * Held as a value on the symbol rather than as a pointer to the statement that produced it. The statement belongs to
 * the module tree, and an instantiation of a generic alias has no statement of its own to point at - it is the same
 * alias under substituted arguments, which is a new @c AliasInfo and not a new piece of syntax.
 */
SPP_EXP_CLS struct spp::analyse::scopes::AliasInfo {
  /** The target as written ("SizedIntegerUnsigned[8_u32]"), before any alias in the chain has been followed. */
  Shared<asts::TypeAst> Written;

  /**
   * What @c Written names once every alias in the chain has been followed ("SizedInteger[w=8_u32, signed=false]").
   * Seeded with @c Written and refined during resolution, so it is never null: analysing an alias's target reads
   * this off the very symbol still being resolved.
   */
  Shared<asts::TypeAst> Resolved;

  /**
   * The parameters the alias declares itself: the "T" of @code type MyVec[T] = Vec[T]@endcode .
   */
  Shared<asts::GenericParameterGroupAst> Params;

  /**
   * The scope the final target was found in, which is where an instantiation of this alias is attached.
   */
  Scope *TrackingScope = nullptr;

  /**
   * The scope the alias was written in.
   */
  Scope *DeclScope = nullptr;

  /**
   * Whether a @c use statement produced this alias; generics propagate differently along such a link.
   */
  bool FromUseStmt = false;

  /**
   * Whether @c Params was adopted from the target rather than written on the alias itself, which a @c use statement
   * does to carry the target's parameters across. Adopted parameters name their constraint and @c cmp types in the
   * target's file, so they only resolve against @c TrackingScope ; ones the alias declared itself name them in its
   * own file, and resolve against the statement's own scope.
   */
  bool ParamsFromTarget = false;

  /**
   * The statement this describes, for diagnostics. Not owned: the module tree owns it, and an instantiation shares
   * the statement of the alias it was instantiated from.
   */
  asts::TypeStatementAst *Stmt = nullptr;
};

SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX

  Shared<asts::TypeIdentifierAst> Name;

  asts::ClassPrototypeAst *Type;

  Scope *LinkedScope;

  Scope *ScopeDefinedIn;

  Scope *ScopeModule;

  bool IsGeneric = false;

  /**
   * Whether this names a variadic generic parameter (@c "..Ts"), which stands for however many arguments are left
   * rather than for exactly one. Only meaningful alongside @c IsGeneric . Written as the last argument of a variadic
   * type it swallows the remainder, which is what lets @c "Tup[Ts]" name a tuple of any size where @c "Tup[T, U, V]"
   * names a three element one and nothing else.
   */
  bool IsVariadic = false;

  /**
   * Whether this names a real type the whole way down. False only for a generic instantiation built from arguments
   * that are themselves still parameters - @c "Pass[T=T]" or @c "Vec[T=U8, A=A]", which a generic body produces
   * simply by naming a type in terms of its own parameters. Such a type has no layout to give and no size to answer
   * with, so code generation leaves its struct opaque and never builds anything against it.
   *
   * @n
   * Decided where the instantiation is created (see @c CreateGenericClsScope ), for the same reason
   * @c FunctionPrototypeAst::GenericSubstitution::IsConcrete is: every reader has to reach the same answer, and
   * asking separately is how they come to disagree.
   */
  bool IsConcrete = true;

  Vec<Shared<asts::TypeAst>> GenericConstraints;

  /**
   * For a symbol created from a generic argument ("Yield=T"), the argument's value type ("T"). The binding is
   * normally recoverable from @c LinkedScope, but when the value is itself an unresolved generic parameter there is
   * no scope to link to, and the mapping would otherwise be lost.
   */
  Shared<asts::TypeAst> GenericVal;

  /**
   * The symbol this one takes its derived properties from - copyability, zero-type-ness - when it does not carry
   * them itself: the template a generic substitution was made from, or the type an alias resolves to. Held as a
   * symbol rather than as a closure over one so that copying a @c TypeSymbol copies what it means - a closure
   * capturing the symbol it describes would go on describing the symbol it was copied from, which is what stops a
   * symbol table from being deep-copied.
   */
  Shared<TypeSymbol> DerivesFromSym;

  asts::utils::Visibility Visibility;

  Unique<asts::ConventionAst> Convention;

  TypeSymbol *GenericImpl;

  Shared<codegen::LlvmTypeSymInfo> LlvmInfo;

  /** Set when this symbol names an alias rather than a class; see @c AliasInfo . */
  Shared<AliasInfo> Alias;

  Vec<Weak<TypeSymbol>> AliasedBySyms;

  bool IsDirectlyCopyable = false;

  bool IsDirectlyZeroType;

  bool IsDirectlyThreadHazard = false;

  /**
   * The result of the qualifying walk in @c FqName , and the scope-linkage generation it was computed under. The walk
   * builds a namespace-qualified ast chain from the scopes above @c LinkedScope , all of which are fixed once the
   * symbol is in place, so the answer only changes when a scope moves in the tree - which the generation records. A
   * zero generation means nothing is cached yet.
   */
  mutable Shared<asts::TypeAst> _CachedFqName;
  mutable std::uint64_t _CachedFqNameGen = 0;

  TypeSymbol(
    Shared<asts::TypeIdentifierAst> name,
    asts::ClassPrototypeAst *type,
    Scope *scope,
    Scope *scope_defined_in,
    Scope *scope_module = nullptr,
    bool is_generic = false,
    bool is_directly_copyable = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPrivate,
    Unique<asts::ConventionAst> &&convention = nullptr,
    Vec<Shared<asts::TypeAst>> const &generic_constraints = {});

  TypeSymbol(
    TypeSymbol const &that);

  ~TypeSymbol() override;

  /**
   * Only an alias is rewritten per instantiation ("type T = ..." becomes "type T = Str"). Everything else a type symbol
   * holds is either the same from every instantiation, or - in the case of @c LlvmInfo - deliberately shared with the
   * template, because one written type is one llvm type however many instantiations name it.
   */
  SPP_ATTR_NODISCARD auto NeedsDeepCopy() const -> bool override;

  /**
   * Whether a value of this type is copied rather than moved.
   * @return Whether this type, or the type it derives copyability from, is copyable.
   */
  SPP_ATTR_NODISCARD auto IsCopyable() const -> bool;

  /**
   * Whether this type has no runtime representation.
   * @return Whether this type, or the type it derives it from, is a zero type.
   */
  SPP_ATTR_NODISCARD auto IsZeroType() const -> bool;

  /**
   * Whether a value of this type may cross a thread boundary. Every type may, unless @c !thread_hazard was written on
   * it or on something it reaches through its generic arguments or attributes,. A generic parameter is the one exception to "safe by
   * default": until it is bound there is nothing stopping it being instantiated with a hazard, so it answers yes only
   * where it was constrained to.
   *
   * @n
   * There is no separate "shareable" answer to give, the way Rust separates @c Send from @c Sync : the two differ only
   * where a value can be mutated through a shared borrow, and the exclusivity law leaves no way to do that. Nor is there an
   * opposite marker: nothing may claim to be safe while holding a hazard, and the types that guard shared state hold
   * their value behind a pointer rather than behind anything hazardous, so they answer honestly here.
   * @return Whether this type is thread-safe.
   */
  SPP_ATTR_NODISCARD auto IsThreadSafe() const -> bool;

  auto operator==(
    TypeSymbol const &that) const
    -> bool;

  /**
   * The symbol for the class this one names. Usually that is this symbol; the exception is a symbol that stands in for
   * a class without carrying its prototype - @c "Self", which links to the class's scope but has a null @c Type (see
   * @c AddSelfTypeSym ). Anything wanting the class rather than the name has to come through here, because reading
   * @c Type off a stand-in gets nothing.
   *
   * @n
   * The null @c Type on a stand-in is deliberate and must stay: it is what lets two @c "Self" symbols compare equal to
   * each other, which is how a method written in terms of @c "Self" is recognised as overriding an abstract one. So
   * the resolution is done by the readers that need a class, never by filling the prototype in.
   *
   * @return The class's symbol, or this symbol when it is already one (or names nothing at all).
   */
  SPP_ATTR_NODISCARD auto AsClassSymbol() const
    -> TypeSymbol*;

  /**
   * The symbol a bound generic parameter ultimately stands for. A bound parameter keeps the parameter's own name
   * ("T") but links to the argument's scope, and it is the argument that has the attributes, the methods and the sup
   * chain - so anything asking a property of the type rather than of the name it arrived under comes through here.
   * Resolved all the way, since an argument can itself be a parameter bound one level out.
   * @return The argument's symbol, or this symbol when it is not a bound parameter.
   */
  SPP_ATTR_NODISCARD auto AsBoundSymbol() const
    -> TypeSymbol*;

  /**
   * Discard this symbol's cached fully qualified name. Needed when @c LinkedScope is re-pointed after the symbol has
   * been built, which changes the chain the name is read off without moving any scope in the tree.
   */
  auto InvalidateFqNameCache() const
    -> void;

  /**
   * The fully qualified name of the type, as a type AST that re-resolves to this symbol from any scope.
   * @param ignore_dollar Leave a compiler-generated ("$Func") mock as a bare, module-local name. Defaults to
   * qualifying it like any other type: the name has to survive crossing a module boundary, because a function value
   * bound to a generic ("mod_a::a(b)" binding "F = mod_b::$B") is resolved in the caller's scope during inference,
   * before any generic substitution runs. Only opt out where a bare name is genuinely wanted.
   */
  SPP_ATTR_NODISCARD auto FqName(bool ignore_dollar = false) const
    -> Shared<asts::TypeAst>;

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
