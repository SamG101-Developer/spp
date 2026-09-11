module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.ast_kind;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.token_ast;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct VariableSymbol;
}

SPP_AST_COMMON_FWD_DECL(GenericArgumentCompKeywordAst) {
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::detail {
  template <>
  struct make_keyword_arg<GenericArgumentCompAst> {
    using type = GenericArgumentCompKeywordAst;
  };
}

/**
 * The GenericArgumentCompKeywordAst represents a keyword argument in a generic argument context. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompKeywordAst final : GenericArgumentCompAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericArgumentCompKeywordAst);

  /**
   * The name of the keyword argument. This is the identifier that is used to refer to the argument in the generic
   * call.
   */
  Shared<TypeAst> Name;

  /**
   * The token that represents the assignment operator @c = in the keyword argument. This separates the name of the
   * argument from the expression that is being passed as the argument's value.
   */
  Unique<TokenAst> TokAssign;

  static auto FromSym(analyse::scopes::VariableSymbol const &sym) -> Unique<GenericArgumentCompKeywordAst>;

  /**
   * Construct the GenericArgumentCompKeywordAst with the arguments matching the members.
   * @param name The name of the keyword argument.
   * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
   * @param val The value of the generic comp argument.
   */
  GenericArgumentCompKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~GenericArgumentCompKeywordAst() override;

  SPP_ATTR_NODISCARD auto EqualsGenericArgumentCompKeyword(
    GenericArgumentCompKeywordAst const &other) const -> Ordering override;
  SPP_ATTR_NODISCARD auto Equals(
    GenericArgumentAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  SPP_ATTR_NODISCARD auto ViewName() const -> StrView override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentCompKeywordAst)
