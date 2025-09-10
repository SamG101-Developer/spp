#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypeUnaryExpressionOperatorBorrowAst;
    friend struct TypeUnaryExpressionOperatorNamespaceAst;

    friend auto operator==(TypeUnaryExpressionOperatorAst const &lhs, TypeUnaryExpressionOperatorAst const &rhs) -> bool {
        return lhs.equals(rhs);
    }

protected:
    virtual auto equals(TypeUnaryExpressionOperatorAst const &) const -> bool = 0;

    virtual auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> bool;

    virtual auto equals_op_namespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> bool;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;
};
