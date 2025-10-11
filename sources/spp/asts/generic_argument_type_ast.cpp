#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericArgumentTypeAst::GenericArgumentTypeAst(
    decltype(val) val,
    const decltype(m_order_tag) order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}


spp::asts::GenericArgumentTypeAst::~GenericArgumentTypeAst() = default;
