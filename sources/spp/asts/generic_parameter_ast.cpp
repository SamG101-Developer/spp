module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_ast;


SPP_MOD_BEGIN
spp::asts::GenericParameterAst::GenericParameterAst(
    decltype(name) name,
    const utils::OrderableTag order_tag) :
    OrderableAst(order_tag),
    name(std::move(name)) {
}


spp::asts::GenericParameterAst::~GenericParameterAst() = default;
SPP_MOD_END
