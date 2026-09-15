module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.ast_kind;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.token_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentCompKeywordAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct VariableSymbol);

namespace spp::asts::detail {
  template <>
  struct make_keyword_arg<GenericArgumentCompAst> {
    using type = GenericArgumentCompKeywordAst;
  };
}

/// A keyword comp argument in a generic argument context. It
/// forces the argument to be matched by a keyword rather than
/// an index.
SPP_EXP_CLS struct spp::asts::GenericArgumentCompKeywordAst final : GenericArgumentCompAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericArgumentCompKeywordAst);

  /// The name of the keyword argument, used to refer to the
  /// argument in the generic call.
  Shared<TypeAst> Name;

  /// The "=" token separating the argument name from its value.
  Unique<TokenAst> TokAssign;

  static auto FromSym(VariableSymbol const &sym) -> Unique<GenericArgumentCompKeywordAst>;

  GenericArgumentCompKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~GenericArgumentCompKeywordAst() override;

  SPP_ATTR_NODISCARD auto EqualsGenericArgumentCompKeyword(
    GenericArgumentCompKeywordAst const &other) const
    -> Ordering override;
  SPP_ATTR_NODISCARD auto Equals(GenericArgumentAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  SPP_ATTR_NODISCARD auto ViewName() const -> StrView override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentCompKeywordAst)
