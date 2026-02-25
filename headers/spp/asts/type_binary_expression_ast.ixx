module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeBinaryExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeBinaryExpressionAst final : Ast {
    std::unique_ptr<TypeAst> lhs;
    std::unique_ptr<TokenAst> tok_op;
    std::unique_ptr<TypeAst> rhs;

    TypeBinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
    ~TypeBinaryExpressionAst() override;
    auto to_rust() const -> std::string override;
};
