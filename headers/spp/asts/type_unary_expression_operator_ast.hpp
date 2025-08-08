#ifndef TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
#define TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeUnaryExpressionOperatorAst : virtual Ast {
    using Ast::Ast;

public:
    virtual auto ns_parts() const -> std::vector<IdentifierAst const*> = 0;

    virtual auto type_parts() const -> std::vector<TypeIdentifierAst const*> = 0;
};


#endif //TYPE_UNARY_EXPRESSION_OPERATOR_AST_HPP
