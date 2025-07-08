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
};


#endif //TYPE_AST_HPP
