#include <spp/asts/generic_argument_ast.hpp>


spp::asts::GenericArgumentAst::~GenericArgumentAst() = default;


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_keyword(
    GenericArgumentCompKeywordAst const &) const
    -> bool {
    return false;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_comp_positional(
    GenericArgumentCompPositionalAst const &) const
    -> bool {
    return false;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_keyword(
    GenericArgumentTypeKeywordAst const &) const
    -> bool {
    return false;
}


auto spp::asts::GenericArgumentAst::equals_generic_argument_type_positional(
    GenericArgumentTypePositionalAst const &) const
    -> bool {
    return false;
}
