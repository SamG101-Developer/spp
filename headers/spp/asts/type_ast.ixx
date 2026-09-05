module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
  SPP_EXP_CLS struct ConventionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct TypePostfixExpressionAst;
  SPP_EXP_CLS struct TypeUnaryExpressionAst;
}

/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst :
  PrimaryExpressionAst, mixins::AbstractTypeAst, EnableLocalSharedFromThis<TypeAst> {
  SPP_GCC_VTABLE_FIX

  TypeAst();

  ~TypeAst() override;

  SPP_ATTR_NODISCARD virtual auto IsTypeIdentifier() const noexcept -> bool { return false; }

  SPP_ATTR_NODISCARD virtual auto IsSelfType() const noexcept -> bool { return false; }

  /**
   * A type sitting in expression position - the @c A of @c {A::new()} , or a comp argument naming one - is substituted
   * as the type it is. This is the one node where the expression walk and the type walk meet.
   */
  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  /**
   * Answer with the symbol this type resolved to last time it was asked for in @p scope (if that answer still stands).
   * @param scope The scope the lookup is being made in.
   * @param generation The current @c TypeLookupGeneration ; a remembered answer from any earlier one is discarded.
   * @param out Set to the remembered symbol when there is one. Untouched otherwise.
   * @return Whether @p out was set.
   */
  SPP_ATTR_NODISCARD SPP_ATTR_HOT
  auto TryCachedLookup(
    analyse::scopes::Scope const *const scope,
    const std::uint64_t generation,
    analyse::scopes::TypeSymbol *&out) const -> bool {
    if (_LookupScope != scope or _LookupGen != generation) { return false; }
    out = _LookupSym;
    return true;
  }

  /**
   * Remember what this type resolved to in @p scope , so the next identical lookup is a pointer comparison. A null
   * symbol is cached too: "not found" is as expensive to re-derive as a found symbol.
   */
  SPP_ATTR_HOT
  auto RememberLookup(
    analyse::scopes::Scope const *const scope,
    const std::uint64_t generation,
    analyse::scopes::TypeSymbol *const sym) const -> void {
    _LookupScope = scope;
    _LookupSym = sym;
    _LookupGen = generation;
  }

protected:
  mutable Shared<TypeAst> _CachedWithoutGenerics;
  mutable analyse::scopes::Scope const *_LookupScope;
  mutable analyse::scopes::TypeSymbol *_LookupSym;
  mutable std::uint64_t _LookupGen;
  mutable Str _CachedStringification;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeAst)
