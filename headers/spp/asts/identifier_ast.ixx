module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
}


SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst {
    std::string val;

    explicit IdentifierAst(
        std::size_t pos,
        decltype(val) val);
    ~IdentifierAst() override;
    auto to_rust() const -> std::string override;
};
