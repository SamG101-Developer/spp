#ifndef TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
#define TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;
    friend struct TypeUnaryExpressionOperatorBorrowAst;
    friend struct TypeUnaryExpressionOperatorNamespaceAst;

protected:
    virtual auto equals_op_borrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> bool;

    virtual auto equals_op_namespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> bool;

public:
    virtual auto operator==(TypeUnaryExpressionOperatorAst const &other) const -> bool = 0;

    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
