#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypeUnaryExpressionOperatorBorrowAst;
    friend struct TypeUnaryExpressionOperatorNamespaceAst;

    auto operator<=>(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering;

    auto operator==(TypeUnaryExpressionOperatorAst const &) const -> bool;

protected:
    virtual auto equals(TypeUnaryExpressionOperatorAst const &) const -> std::strong_ordering = 0;

    virtual auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> std::strong_ordering;

    virtual auto equals_op_namespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> std::strong_ordering;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
