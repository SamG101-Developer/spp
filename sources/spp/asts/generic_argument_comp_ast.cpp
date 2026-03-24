module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_comp_ast;
import spp.asts.expression_ast;


SPP_MOD_BEGIN
spp::asts::GenericArgumentCompAst::GenericArgumentCompAst(
    decltype(val) &&val,
    const utils::OrderableTag order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}

spp::asts::GenericArgumentCompAst::~GenericArgumentCompAst() = default;

SPP_MOD_END
