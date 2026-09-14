module;
#include <spp/macros.hpp>

export module spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;

use(spp::asts::mixins, struct OrderableAst);

/// An orderable ast is an ast such as a function parameter or
/// function argument, ie it needs to be ordered. Then, the
/// group ast like the function argument group ast will order
/// its vector based on the tags. Asts like the function
/// required parameter will internally update its own tag.
SPP_EXP_CLS struct spp::asts::mixins::OrderableAst {
  /// Initialise this ast with the tag at construction, so it
  /// is forced to have an initialised tag.
  explicit OrderableAst(utils::OrderableTag order_tag);

  virtual ~OrderableAst();

  /// Getter for the private ordering tag.
  SPP_ATTR_NODISCARD auto GetOrderTag() const -> utils::OrderableTag;

private:
  /// The order tag of this orderable AST.
  utils::OrderableTag m_order_tag;
};
