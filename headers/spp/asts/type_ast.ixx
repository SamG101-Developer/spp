module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct ConventionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypePostfixExpressionAst);
use(spp::asts, struct TypeUnaryExpressionAst);

/// The base class for all type asts.
SPP_EXP_CLS struct spp::asts::TypeAst :
  PrimaryExpressionAst,
  mixins::AbstractTypeAst,
  EnableLocalSharedFromThis<TypeAst> {
  SPP_GCC_VTABLE_FIX;

  TypeAst();

  ~TypeAst() override;

  SPP_ATTR_NODISCARD virtual auto IsTypeIdentifier() const noexcept -> bool {
    return false;
  }

  SPP_ATTR_NODISCARD virtual auto IsSelfType() const noexcept -> bool {
    return false;
  }

  /// A type in expression position (the "A" of "A::new()", or
  /// a comp argument naming a type) is substituted as the type
  /// it is. This is the one node where the expression walk and
  /// the type walk meet.
  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  /// A clone of this type with "arg_group" on its right-most part.
  /// A plain name builds its own ("TypeIdentifierAst").
  SPP_ATTR_NODISCARD auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> override;

  /// Get the symbol this type resolved to the last time it was
  /// looked up in "scope", if that answer still stands. An
  /// answer remembered under an earlier "TypeLookupGeneration"
  /// is discarded. "out" is only set when there is an answer.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto TryCachedLookup(
    Scope const *const scope, const std::uint64_t generation,
    TypeSymbol *&out) const -> bool {
    if (_LookupScope != scope or _LookupGen != generation) { return false; }
    out = _LookupSym;
    return true;
  }

  /// Remember what this type resolved to in "scope", so the
  /// next identical lookup is a pointer comparison. A null
  /// symbol is cached too: "not found" is as expensive to
  /// re-derive as a found symbol.
  SPP_ATTR_HOT auto RememberLookup(
    Scope const *const scope,
    const std::uint64_t generation,
    TypeSymbol *const sym) const
    -> void {
    _LookupScope = scope;
    _LookupSym = sym;
    _LookupGen = generation;
  }

  /// The symbol this type resolved to where it was written, if it
  /// was stamped with one ("TypeSymbol::FqName" stamps the names it
  /// hands out). A lookup of a stamped type asks "Scope::Canon" what
  /// that symbol means from the scope asking, instead of resolving
  /// the spelling again there - which binds a caller's "T" to a
  /// callee's parameter of the same name.
  SPP_ATTR_NODISCARD auto Stamp() const noexcept -> TypeSymbol* {
    return _Stamp;
  }

  /// Stamp this type with the symbol it resolved to; see "Stamp".
  auto SetStamp(TypeSymbol *const sym) const noexcept -> void {
    _Stamp = sym;
  }

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// A copy of this type that reports "written"'s source span as
  /// its own position. For a type rebuilt from its symbol (a
  /// qualified name, an alias's target, a bound generic) that
  /// replaces one written in source, so errors point at what was
  /// written rather than at the declaration. A copy, because the
  /// rebuilt type may be shared by every use of the symbol. A
  /// written type with no real span leaves the copy unstamped.
  SPP_ATTR_NODISCARD auto WithSourceSpanOf(TypeAst const &written) const -> Shared<TypeAst>;

  /// The text of the type written in source that this one was
  /// rebuilt from (see "WithSourceSpanOf"), for error messages
  /// to show beside the rebuilt, qualified form. Empty when this
  /// type replaced nothing written.
  SPP_ATTR_NODISCARD auto WrittenText() const -> Str const& {
    return _WrittenText;
  }

protected:
  mutable Shared<TypeAst> _CachedWithoutGenerics;
  mutable Scope const *_LookupScope;
  mutable TypeSymbol *_LookupSym;
  mutable std::uint64_t _LookupGen;
  mutable TypeSymbol *_Stamp = nullptr;
  mutable Str _CachedStringification;

  /// Whether this type reports "_SpanStart" to "_SpanEnd" as its
  /// position, in place of its parts'; see "WithSourceSpanOf".
  bool _HasSourceSpan = false;
  std::size_t _SpanStart = 0;
  std::size_t _SpanEnd = 0;

  /// The text this type replaced in source; see "WrittenText".
  Str _WrittenText;

  /// Carry the span override, and the written text, onto a
  /// clone of this type.
  auto CopySourceSpanTo(TypeAst &clone) const -> void {
    clone._HasSourceSpan = _HasSourceSpan;
    clone._SpanStart = _SpanStart;
    clone._SpanEnd = _SpanEnd;
    clone._WrittenText = _WrittenText;
  }
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeAst)
