#ifndef INNER_SCOPE_EXPRESSION_HPP
#define INNER_SCOPE_EXPRESSION_HPP

#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


template <typename T>
struct spp::asts::InnerScopeExpressionAst final : InnerScopeAst<T>, PrimaryExpressionAst {
    using InnerScopeAst<T>::InnerScopeAst;
};


#endif //INNER_SCOPE_EXPRESSION_HPP
