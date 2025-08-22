#include <spp/asts/type_ast.hpp>


auto spp::asts::TypeAst::equals_type_identifier(TypeIdentifierAst const &) const -> bool {
    return false;
}


auto spp::asts::TypeAst::equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> bool {
    return false;
}


auto spp::asts::TypeAst::equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> bool {
    return false;
}
