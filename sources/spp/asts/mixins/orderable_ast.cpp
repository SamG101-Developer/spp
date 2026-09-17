module;
#include <spp/macros.hpp>

module spp.asts.mixins.orderable_ast;

SPP_MOD_BEGIN
OrderableAst::OrderableAst(
  const utils::OrderableTag order_tag) :
  m_order_tag(order_tag) {
}

OrderableAst::~OrderableAst() = default;

auto OrderableAst::GetOrderTag() const
  -> utils::OrderableTag {
  // Readonly accessor to the internal ordering tag.
  return m_order_tag;
}

SPP_MOD_END
