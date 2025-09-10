#pragma once
#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/mixins/abstract_type_ast.hpp>


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst, std::enable_shared_from_this<TypeAst> {
    using PrimaryExpressionAst::PrimaryExpressionAst;
};
