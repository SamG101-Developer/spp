module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_ast;
import spp.asts.type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
    std::unique_ptr<TypeUnaryExpressionOperatorAst> op;
    std::unique_ptr<TypeAst> rhs;

    explicit TypeUnaryExpressionAst(
        decltype(op) op,
        decltype(rhs) rhs);
    ~TypeUnaryExpressionAst() override;
    auto to_rust() const -> std::string override;
};
