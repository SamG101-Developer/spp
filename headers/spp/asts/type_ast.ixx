module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.analyse.scopes.type_key;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct TypeRef);

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

  /// A type written inside an expression (the "A" of "A::new()", a call's type arguments) is read by its identity
  /// ("type_resolution::ReadType"), as a declaration's types are, whatever kind of type it is.
  SPP_ATTR_NODISCARD auto ReadExpr(
    analyse::scopes::ExprSubst const &sub) const -> Shared<ExpressionAst> override;

  /// "ReadExpr", answered as the type it still is.
  SPP_ATTR_NODISCARD auto ReadExprType(
    analyse::scopes::ExprSubst const &sub) const -> Shared<TypeAst>;

  /// A clone of this type with "arg_group" on its right-most part.
  /// A plain name builds its own ("TypeIdentifierAst").
  SPP_ATTR_NODISCARD auto WithGns(
    Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> override;

  /// The identity this type resolved to where it was written
  /// ("NameTypeIdOf"), if it was resolved there
  /// ("TypeSymbol::FqName" writes it into the names it hands out). A
  /// lookup of it reads that identity through the scope asking
  /// ("Scope::FindBoundTypeSymbolById"), instead of resolving the spelling
  /// again there - which binds a caller's "T" to a callee's
  /// parameter of the same name.
  SPP_ATTR_NODISCARD auto StampedTypeId() const noexcept -> TypeId {
    return _StampedTypeId;
  }

  /// Record the identity this type resolved to where it is
  /// written; see "StampedTypeId".
  auto StampTypeId(const TypeId id) const noexcept -> void {
    _StampedTypeId = id;
  }

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// A copy of this type that reports "written"'s source span as
  /// its own position. For a type rebuilt from its symbol (a
  /// qualified name, an alias's target, a bound generic) that
  /// replaces one written in source, so errors point at what was
  /// written rather than at the declaration. A copy, because the
  /// rebuilt type may be shared by every use of the symbol. A
  /// written type with no real span leaves the copy unstamped.
  SPP_ATTR_NODISCARD auto WithSourceSpanOf(
    TypeAst const &written) const -> Shared<TypeAst>;

  /// A copy of this type that errors point at @p site, such as the
  /// expression the type was inferred from ("1" for "S32"). Unlike
  /// "WithSourceSpanOf", it keeps this type's own spelling.
  SPP_ATTR_NODISCARD auto WithSourceSpanAt(
    Ast const &site) const -> Shared<TypeAst>;

  /// The text of the type written in source that this one was
  /// rebuilt from (see "WithSourceSpanOf"), for error messages
  /// to show beside the rebuilt, qualified form. Empty when this
  /// type replaced nothing written.
  SPP_ATTR_NODISCARD auto WrittenText() const -> Str const& {
    return _WrittenText;
  }

protected:
  mutable Shared<TypeAst> _CachedWithoutGns;
  mutable TypeId _StampedTypeId = nullptr;
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
