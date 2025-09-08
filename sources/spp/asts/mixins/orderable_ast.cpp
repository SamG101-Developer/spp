#include <spp/asts/mixins/orderable_ast.hpp>


spp::asts::mixins::OrderableAst::OrderableAst(const OrderableTag order_tag) :
    m_order_tag(order_tag) {
}


spp::asts::mixins::OrderableAst::~OrderableAst() = default;
