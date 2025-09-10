#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/type_inferrable.hpp>


struct spp::asts::PostfixExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;
};
