module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeOptionalAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeOptionalAst final : GenericParameterTypeAst {
    std::unique_ptr<TokenAst> tok_assign;
    std::unique_ptr<TypeAst> default_val;

    GenericParameterTypeOptionalAst(
        decltype(name) &&name,
        decltype(constraints) &&constraints,
        decltype(tok_assign) &&tok_assign,
        decltype(default_val) &&default_val);
    ~GenericParameterTypeOptionalAst() override;
    auto to_rust() const -> std::string override;
};
