module;
#include <spp/macros.hpp>

module spp.asts.mixins.orderable_ast;

SPP_MOD_BEGIN
spp::asts::mixins::OrderableAst::OrderableAst(
  const utils::OrderableTag order_tag) :
  _OrderTag(order_tag) {
}

spp::asts::mixins::OrderableAst::~OrderableAst() = default;

auto spp::asts::mixins::OrderableAst::GetOrderTag() const
  -> utils::OrderableTag {
  // Readonly accessor to the internal ordering tag.
  return _OrderTag;
}

SPP_MOD_END
