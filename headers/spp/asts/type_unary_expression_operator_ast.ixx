module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_ast;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;

    auto operator<=>(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering;

    auto operator==(TypeUnaryExpressionOperatorAst const &) const -> bool;

    virtual auto equals(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering = 0;

    virtual auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> std::strong_ordering;

    virtual auto equals_op_namespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> std::strong_ordering;

    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
