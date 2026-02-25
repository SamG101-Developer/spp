module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.generic_parameter_type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeVariadicAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeVariadicAst final : GenericParameterTypeAst {
    std::unique_ptr<TokenAst> tok_ellipsis;

    explicit GenericParameterTypeVariadicAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        decltype(name) &&name,
        decltype(constraints) &&constraints);
    ~GenericParameterTypeVariadicAst() override;
    auto to_rust() const -> std::string override;
};
