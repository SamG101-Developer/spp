module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_temp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeBinaryExpressionTempAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeBinaryExpressionTempAst final {
    std::unique_ptr<TokenAst> tok_op;
    std::unique_ptr<TypeAst> rhs;

    explicit TypeBinaryExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
    ~TypeBinaryExpressionTempAst();
};
