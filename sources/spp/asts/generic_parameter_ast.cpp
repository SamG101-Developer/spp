module spp.asts.generic_parameter_ast;


spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name,
    const utils::OrderableTag order_tag) :
    OrderableAst(order_tag),
    name(std::move(name)) {
}
