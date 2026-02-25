module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableSingleIdentifierAliasAst final : Ast {
    std::unique_ptr<IdentifierAst> name;

    LocalVariableSingleIdentifierAliasAst(
        decltype(name) &&name);
    ~LocalVariableSingleIdentifierAliasAst() override;
    auto to_rust() const -> std::string override;
};
