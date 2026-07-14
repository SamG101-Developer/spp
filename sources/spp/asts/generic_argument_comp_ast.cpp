module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_comp_ast;
import spp.asts.expression_ast;

SPP_MOD_BEGIN
spp::asts::GenericArgumentCompAst::GenericArgumentCompAst(
    decltype(Val) &&val,
    const utils::OrderableTag order_tag) :
    GenericArgumentAst(order_tag),
    Val(std::move(val)) {
}

spp::asts::GenericArgumentCompAst::~GenericArgumentCompAst() = default;
SPP_MOD_END
