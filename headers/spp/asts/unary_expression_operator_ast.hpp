#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


struct spp::asts::UnaryExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;
};
