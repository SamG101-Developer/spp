#ifndef TYPE_AST_HPP
#define TYPE_AST_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/mixins/abstract_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;
    friend struct TypeIdentifierAst;
    friend struct TypeUnaryExpressionAst;
    friend struct TypePostfixExpressionAst;

    friend auto operator==(TypeAst const &lhs_type, TypeAst const &rhs_type) -> bool {
        return lhs_type.equals(rhs_type);
    }

protected:
    virtual auto equals_type_identifier(TypeIdentifierAst const &) const -> bool;
    virtual auto equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> bool;
    virtual auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> bool;
    virtual auto equals(TypeAst const &other) const -> bool = 0;
};


#endif //TYPE_AST_HPP
