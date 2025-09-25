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
}

/// @endcond


template <typename>
struct spp::utils::SymNameCmp {
};


template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>> {
    auto operator()(
        std::shared_ptr<asts::IdentifierAst> const &lhs,
        std::shared_ptr<asts::IdentifierAst> const &rhs) const
        -> bool;
};


template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    auto operator()(
        std::shared_ptr<asts::TypeIdentifierAst> const &lhs,
        std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const
        -> bool;
};
