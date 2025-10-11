#include <spp/asts/expression_ast.hpp>
#include <spp/asts/generic_argument_comp_ast.hpp>


spp::asts::GenericArgumentCompAst::GenericArgumentCompAst(
    decltype(val) &&val,
    const decltype(m_order_tag) order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}


spp::asts::GenericArgumentCompAst::~GenericArgumentCompAst() = default;
