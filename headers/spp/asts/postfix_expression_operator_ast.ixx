module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorAst;
    SPP_EXP_CLS struct TypeAst; // TODO: GCC BUG REQUIRES THIS
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : virtual Ast, mixins::TypeInferrableAst {
    PostfixExpressionOperatorAst();

    ~PostfixExpressionOperatorAst() override;

    SPP_ATTR_NODISCARD virtual auto expr_parts() const -> std::vector<Ast*>;
};
