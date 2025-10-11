#include <spp/asts/generic_parameter_ast.hpp>


spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    name(std::move(name)) {
}
