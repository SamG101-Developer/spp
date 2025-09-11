#include <spp/asts/generic_argument_ast.hpp>


spp::asts::GenericArgumentAst::~GenericArgumentAst() = default;


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_keyword(
    GenericArgumentCompKeywordAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_positional(
    GenericArgumentCompPositionalAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_keyword(
    GenericArgumentTypeKeywordAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_positional(
    GenericArgumentTypePositionalAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}
