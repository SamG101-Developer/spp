module spp.utils.ptr_cmp;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;

import absl;


template <typename T>
auto spp::utils::SymNameCmp<std::shared_ptr<T>>::operator()(
    std::shared_ptr<T> const &lhs,
    std::shared_ptr<T> const &rhs) const
    -> bool {
    return *lhs < *rhs;
}


auto spp::utils::PtrHash<std::shared_ptr<spp::asts::IdentifierAst>>::operator()(
    std::shared_ptr<asts::IdentifierAst> const &p) const
    -> std::size_t {
    return absl::Hash<std::string>()(p->val);
}


auto spp::utils::PtrHash<std::shared_ptr<spp::asts::TypeIdentifierAst>>::operator()(
    std::shared_ptr<asts::TypeIdentifierAst> const &p) const
    -> std::size_t {
    return absl::Hash<std::string>()(p->name);
}


auto spp::utils::PtrEq<std::shared_ptr<spp::asts::IdentifierAst>>::operator()(
    std::shared_ptr<asts::IdentifierAst> const &lhs,
    std::shared_ptr<asts::IdentifierAst> const &rhs) const
    -> bool {
    return *lhs == *rhs;
}


auto spp::utils::PtrEq<std::shared_ptr<spp::asts::TypeIdentifierAst>>::operator()(
    std::shared_ptr<asts::TypeIdentifierAst> const &lhs,
    std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const
    -> bool {
    return *lhs == *rhs;
}
