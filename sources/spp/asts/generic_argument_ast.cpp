module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_ast;

SPP_MOD_BEGIN
spp::asts::GenericArgumentAst::GenericArgumentAst(
    const utils::OrderableTag order_tag) :
    OrderableAst(order_tag) {
}

spp::asts::GenericArgumentAst::~GenericArgumentAst() = default;

auto spp::asts::GenericArgumentAst::operator<=>(
    GenericArgumentAst const &other) const
    -> Ordering {
    return Equals(other);
}

auto spp::asts::GenericArgumentAst::operator==(
    GenericArgumentAst const &other) const
    -> bool {
    return Equals(other) == Ordering::equal;
}

auto spp::asts::GenericArgumentAst::EqualsGenericArgumentCompKeyword(
    GenericArgumentCompKeywordAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::GenericArgumentAst::EqualsGenericArgumentCompPositional(
    GenericArgumentCompPositionalAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::GenericArgumentAst::EqualsGenericArgumentTypeKeyword(
    GenericArgumentTypeKeywordAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::GenericArgumentAst::EqualsGenericArgumentTypePositional(
    GenericArgumentTypePositionalAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::GenericArgumentAst::Equals(
    GenericArgumentAst const &) const
    -> Ordering {
    return Ordering::less;
}

auto spp::asts::GenericArgumentAst::ViewName() const
    -> StrView {
    // Overridden by the keyword argument ASTs to view the keyword name.
    return "";
}

SPP_MOD_END
