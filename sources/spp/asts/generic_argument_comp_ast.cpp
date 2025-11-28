module spp.asts.generic_argument_comp_ast;
import spp.asts.expression_ast;


spp::asts::GenericArgumentCompAst::GenericArgumentCompAst(
    decltype(val) &&val,
    const decltype(m_order_tag) order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}


spp::asts::GenericArgumentCompAst::~GenericArgumentCompAst() = default;


auto spp::asts::GenericArgumentCompAst::equals_generic_argument_comp_keyword(
    GenericArgumentCompKeywordAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentCompAst::equals_generic_argument_comp_positional(
    GenericArgumentCompPositionalAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentCompAst::equals_generic_argument_type_keyword(
    GenericArgumentTypeKeywordAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::GenericArgumentCompAst::equals_generic_argument_type_positional(
    GenericArgumentTypePositionalAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}
