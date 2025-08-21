#ifndef TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
#define TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;

public:
    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
