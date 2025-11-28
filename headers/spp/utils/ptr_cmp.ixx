module;
#include <spp/macros.hpp>

export module spp.utils.ptr_cmp;
import std;


namespace spp::utils {
    SPP_EXP_CLS template <typename>
    struct SymNameCmp;

    SPP_EXP_CLS template <typename>
    struct PtrHash;

    SPP_EXP_CLS template <typename>
    struct PtrEq;
}

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS template <typename>
struct spp::utils::SymNameCmp {
};


template <typename T>
struct spp::utils::SymNameCmp<std::shared_ptr<T>> {
    auto operator()(std::shared_ptr<T> const &lhs, std::shared_ptr<T> const &rhs) const -> bool;
};


SPP_EXP_CLS template <typename>
struct spp::utils::PtrHash {
};


template <>
struct spp::utils::PtrHash<std::shared_ptr<spp::asts::IdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::IdentifierAst> const &p) const -> std::size_t;
};


template <>
struct spp::utils::PtrHash<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &p) const -> std::size_t;
};


SPP_EXP_CLS template <typename>
struct spp::utils::PtrEq {
};


template <>
struct spp::utils::PtrEq<std::shared_ptr<spp::asts::IdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::IdentifierAst> const &lhs, std::shared_ptr<asts::IdentifierAst> const &rhs) const -> bool;
};


template <>
struct spp::utils::PtrEq<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &lhs, std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const -> bool;
};
