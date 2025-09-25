#include <spp/utils/ptr_cmp.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


auto spp::utils::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>>::operator()(
    std::shared_ptr<asts::IdentifierAst> const &lhs,
    std::shared_ptr<asts::IdentifierAst> const &rhs) const
    -> bool {
    return *lhs < *rhs;
}


auto spp::utils::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>>::operator()(
    std::shared_ptr<asts::TypeIdentifierAst> const &lhs,
    std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const
    -> bool {
    return *lhs < *rhs;
}
