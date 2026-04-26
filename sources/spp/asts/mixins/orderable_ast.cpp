module;
#include <spp/macros.hpp>

module spp.asts.mixins.orderable_ast;


SPP_MOD_BEGIN
spp::asts::mixins::OrderableAst::OrderableAst(
    const utils::OrderableTag order_tag) :
    m_order_tag(order_tag) {
}


spp::asts::mixins::OrderableAst::~OrderableAst() = default;


auto spp::asts::mixins::OrderableAst::get_order_tag() const
    -> utils::OrderableTag {
    // Readonly accessor to the internal ordering tag.
    return m_order_tag;
}

SPP_MOD_END
