module spp.asts.generic_parameter_ast;


spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    name(std::move(name)) {
}
