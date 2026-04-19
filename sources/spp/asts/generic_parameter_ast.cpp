module spp.asts;


spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name,
    const utils::OrderableTag order_tag) :
    OrderableAst(order_tag),
    name(std::move(name)) {
}


spp::asts::GenericParameterAst::~GenericParameterAst() = default;
