module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;


SPP_EXP struct spp::asts::UnaryExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    ~UnaryExpressionOperatorAst() override;
};
