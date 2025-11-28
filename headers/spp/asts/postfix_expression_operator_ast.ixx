module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    using Ast::Ast;

    ~PostfixExpressionOperatorAst() override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
