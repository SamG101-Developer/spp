#pragma once
#include <memory>

/// @cond
namespace spp::asts {
    struct IdentifierAst;
    struct TypeIdentifierAst;
}

namespace spp::utils {
    template <typename>
    struct SymNameCmp;

    template <typename>
    struct PtrHash;

    template <typename>
    struct PtrEq;
}

/// @endcond


template <typename>
struct spp::utils::SymNameCmp {
};


template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>> {
    auto operator()(std::shared_ptr<asts::IdentifierAst> const &lhs, std::shared_ptr<asts::IdentifierAst> const &rhs) const -> bool;
};


template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &lhs, std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const -> bool;
};


template <typename>
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


template <typename>
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
