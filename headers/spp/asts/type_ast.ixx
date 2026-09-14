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

  /// Get the symbol this type resolved to the last time it was
  /// looked up in "scope", if that answer still stands. An
  /// answer remembered under an earlier "TypeLookupGeneration"
  /// is discarded. "out" is only set when there is an answer.
  SPP_ATTR_NODISCARD SPP_ATTR_HOT auto TryCachedLookup(
    Scope const *const scope,
    const std::uint64_t generation,
    TypeSymbol *&out) const
    -> bool {
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

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

protected:
  mutable Shared<TypeAst> _CachedWithoutGenerics;
  mutable Scope const *_LookupScope;
  mutable TypeSymbol *_LookupSym;
  mutable std::uint64_t _LookupGen;
  mutable Str _CachedStringification;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeAst)
