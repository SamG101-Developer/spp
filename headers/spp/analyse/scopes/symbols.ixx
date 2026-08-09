module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.utils.visibility;
import spp.codegen.llvm_sym_info;
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
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
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
      return std::static_pointer_cast<Derived>(shared_from_this());
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
    asts::utils::Visibility visibility = asts::utils::Visibility::kPublic);

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

SPP_EXP_CLS struct spp::analyse::scopes::TypeSymbol final : Symbol {
  SPP_GCC_VTABLE_FIX

  Shared<asts::TypeIdentifierAst> Name;

  asts::ClassPrototypeAst *Type;

  Scope *LinkedScope;

  Scope *ScopeDefinedIn;

  Scope *ScopeModule;

  bool IsGeneric = false;

  Vec<Shared<asts::TypeAst>> GenericConstraints;

  /**
   * For a symbol created from a generic argument ("Yield=T"), the argument's value type ("T"). The binding is
   * normally recoverable from @c LinkedScope, but when the value is itself an unresolved generic parameter there is
   * no scope to link to, and the mapping would otherwise be lost.
   */
  Shared<asts::TypeAst> GenericVal;

  bool IsDirectlyCopyable = false;

  /**
   * The symbol this one derives its copyability from, if it is not directly copyable itself: the template a generic
   * substitution was made from, or the type an alias resolves to. Held as a symbol rather than as a closure over one
   * so that copying a @c TypeSymbol copies what it means - a closure capturing the symbol it describes would go on
   * describing the symbol it was copied from, which is what stops a symbol table from being deep-copied.
   */
  Shared<TypeSymbol> CopyableBaseSym;

  asts::utils::Visibility Visibility;

  Unique<asts::ConventionAst> Convention;

  TypeSymbol *GenericImpl;

  Shared<codegen::LlvmTypeSymInfo> LlvmInfo;

  Unique<asts::TypeStatementAst> AliasStmt;

  Vec<Shared<TypeSymbol>> AliasedBySyms;

  bool IsDirectlyZeroType;

  /**
   * The symbol this one derives its zero-type-ness from. See @c CopyableBaseSym.
   */
  Shared<TypeSymbol> ZeroTypeBaseSym;

  TypeSymbol(
    Shared<asts::TypeIdentifierAst> name,
    asts::ClassPrototypeAst *type,
    Scope *scope,
    Scope *scope_defined_in,
    Scope *scope_module = nullptr,
    bool is_generic = false,
    bool is_directly_copyable = false,
    asts::utils::Visibility visibility = asts::utils::Visibility::kPublic,
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

  auto operator==(
    TypeSymbol const &that) const
    -> bool;

  /**
   * The fully qualified name of the type, as a type AST that re-resolves to this symbol from any scope.
   * @param ignore_dollar Leave a compiler-generated ("$Func") mock as a bare, module-local name. Defaults to
   * qualifying it like any other type: the name has to survive crossing a module boundary, because a function value
   * bound to a generic ("mod_a::a(b)" binding "F = mod_b::$B") is resolved in the caller's scope during inference,
   * before any generic substitution runs. Only opt out where a bare name is genuinely wanted.
   */
  SPP_ATTR_NODISCARD auto FqName(bool ignore_dollar = false) const
    -> Shared<asts::TypeAst>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::Symbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::NamespaceSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::VariableSymbol)

SPP_GCC_VTABLE_FIX_IMPL(spp::analyse::scopes::TypeSymbol)
