module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeInlineConstraintsAst final : Ast {
    std::unique_ptr<TokenAst> tok_colon;
    std::vector<std::unique_ptr<TypeAst>> constraints;

    GenericParameterTypeInlineConstraintsAst(
        decltype(tok_colon) &&tok_colon,
        std::vector<std::unique_ptr<TypeAst>> &&constraints);
    ~GenericParameterTypeInlineConstraintsAst() override;
    auto to_rust() const -> std::string override;
};
