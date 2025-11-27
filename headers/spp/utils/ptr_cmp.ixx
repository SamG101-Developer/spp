module;
#include <spp/macros.hpp>

export module spp.utils.ptr_cmp;
import spp.asts._fwd;
import std;


namespace spp::utils {
    export template <typename>
    struct SymNameCmp;

    export template <typename>
    struct PtrHash;

    export template <typename>
    struct PtrEq;
}


SPP_EXP template <typename>
struct spp::utils::SymNameCmp {
};


SPP_EXP template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::IdentifierAst>> {
    auto operator()(std::shared_ptr<asts::IdentifierAst> const &lhs, std::shared_ptr<asts::IdentifierAst> const &rhs) const -> bool;
};


SPP_EXP template <>
struct spp::utils::SymNameCmp<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &lhs, std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const -> bool;
};


SPP_EXP template <typename>
struct spp::utils::PtrHash {
};


SPP_EXP template <>
struct spp::utils::PtrHash<std::shared_ptr<spp::asts::IdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::IdentifierAst> const &p) const -> std::size_t;
};


SPP_EXP template <>
struct spp::utils::PtrHash<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &p) const -> std::size_t;
};


SPP_EXP template <typename>
struct spp::utils::PtrEq {
};


SPP_EXP template <>
struct spp::utils::PtrEq<std::shared_ptr<spp::asts::IdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::IdentifierAst> const &lhs, std::shared_ptr<asts::IdentifierAst> const &rhs) const -> bool;
};


SPP_EXP template <>
struct spp::utils::PtrEq<std::shared_ptr<spp::asts::TypeIdentifierAst>> {
    SPP_ATTR_HOT auto operator()(std::shared_ptr<asts::TypeIdentifierAst> const &lhs, std::shared_ptr<asts::TypeIdentifierAst> const &rhs) const -> bool;
};
