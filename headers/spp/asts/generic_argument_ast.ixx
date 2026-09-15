module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentAst);
use(spp::asts, struct GenericArgumentCompKeywordAst);
use(spp::asts, struct GenericArgumentCompPositionalAst);
use(spp::asts, struct GenericArgumentTypeKeywordAst);
use(spp::asts, struct GenericArgumentTypePositionalAst);

namespace spp::asts::detail {
  SPP_EXP_CLS template <typename GenericArgType>
  struct make_keyword_arg {
    using type = GenericArgType;
  };

  SPP_EXP_CLS
  template <typename T>
  using make_keyword_arg_t = typename make_keyword_arg<T>::type;

  SPP_EXP_CLS template <typename GenericArgType>
  struct make_positional_arg {
    using type = GenericArgType;
  };

  SPP_EXP_CLS
  template <typename T>
  using make_positional_arg_t = typename make_positional_arg<T>::type;
}

/// The base class for all generic arguments. It is inherited
/// by "GenericArgumentCompAst" and "GenericArgumentTypeAst",
/// which are in turn inherited for the positional and keyword
/// variants.
SPP_EXP_CLS struct spp::asts::GenericArgumentAst : Ast, mixins::OrderableAst {
  explicit GenericArgumentAst(utils::OrderableTag order_tag);
  ~GenericArgumentAst() override;
  auto operator<=>(GenericArgumentAst const &other) const -> Ordering;
  auto operator==(GenericArgumentAst const &other) const -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentCompKeyword(
    GenericArgumentCompKeywordAst const &) const
    -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentCompPositional(
    GenericArgumentCompPositionalAst const &) const
    -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentTypeKeyword(
    GenericArgumentTypeKeywordAst const &) const
    -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentTypePositional(
    GenericArgumentTypePositionalAst const &) const
    -> Ordering;
  SPP_ATTR_NODISCARD virtual auto Equals(GenericArgumentAst const &other) const -> Ordering = 0;

  SPP_ATTR_NODISCARD virtual auto ViewName() const -> StrView;
};
