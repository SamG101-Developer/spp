module spp.asts.mixins.orderable_ast;


spp::asts::mixins::OrderableAst::OrderableAst(
    const OrderableTag order_tag) :
    m_order_tag(order_tag) {
}


spp::asts::mixins::OrderableAst::~OrderableAst() = default;


auto spp::asts::mixins::OrderableAst::get_order_tag() const
    -> OrderableTag {
    return m_order_tag;
}
