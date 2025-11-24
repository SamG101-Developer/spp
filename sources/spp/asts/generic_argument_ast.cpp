#include <spp/asts/generic_argument_ast.hpp>


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_keyword(
    GenericArgumentCompKeywordAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_positional(
    GenericArgumentCompPositionalAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_keyword(
    GenericArgumentTypeKeywordAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_positional(
    GenericArgumentTypePositionalAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


spp::asts::GenericArgumentAst::GenericArgumentAst(
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag) {
}


spp::asts::GenericArgumentAst::~GenericArgumentAst() = default;


auto spp::asts::GenericArgumentAst::operator<=>(
    GenericArgumentAst const &other) const
    -> std::strong_ordering {
    return equals(other);
}


auto spp::asts::GenericArgumentAst::operator==(
    GenericArgumentAst const &other) const
    -> bool {
    return equals(other) == std::strong_ordering::equal;
}
