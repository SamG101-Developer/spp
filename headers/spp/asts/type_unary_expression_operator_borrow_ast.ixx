module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.type_unary_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
    std::unique_ptr<ConventionAst> conv;

    explicit TypeUnaryExpressionOperatorBorrowAst(
        decltype(conv) &&conv);
    ~TypeUnaryExpressionOperatorBorrowAst() override;
    auto to_rust() const -> std::string override;
};
