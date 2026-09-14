module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_keyword_ast;
import spp.asts.ast_kind;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentTypeKeywordAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::asts::detail {
  template <>
  struct make_keyword_arg<GenericArgumentTypeAst> {
    using type = GenericArgumentTypeKeywordAst;
  };
}

/// A keyword type argument in a generic argument context. It
/// forces the argument to be matched by a keyword rather than
/// an index.
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeKeywordAst final : GenericArgumentTypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(GenericArgumentTypeKeywordAst);

  /// The name of the keyword argument, used to refer to the
  /// argument in the generic call.
  Shared<TypeAst> Name;

  /// The "=" token separating the argument name from its value.
  Unique<TokenAst> TokAssign;

  static auto FromSym(TypeSymbol const &sym) -> Unique<GenericArgumentTypeKeywordAst>;

  GenericArgumentTypeKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) val);

  ~GenericArgumentTypeKeywordAst() override;

  SPP_ATTR_NODISCARD auto EqualsGenericArgumentTypeKeyword(
    GenericArgumentTypeKeywordAst const &other) const
    -> Ordering override;
  SPP_ATTR_NODISCARD auto Equals(GenericArgumentAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  SPP_ATTR_NODISCARD auto ViewName() const -> StrView override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentTypeKeywordAst)
