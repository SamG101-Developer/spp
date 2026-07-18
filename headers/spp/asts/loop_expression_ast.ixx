module;
#include <spp/macros.hpp>

export module spp.asts.loop_expression_ast;
import spp.asts.primary_expression_ast;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct LoopElseStatementAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LoopExpressionAst : PrimaryExpressionAst {
protected:
    std::optional<std::tuple<ExpressionAst*, Shared<TypeAst>, analyse::scopes::Scope*>> m_loop_exit_type_info;

public:
    /**
     * The @c loop token that indicates the start of a loop expression.
     */
    Unique<TokenAst> TokLoop;

    /**
     * The body of the loop. This is a block of statements that will be executed for each iteration of the loop.
     */
    Unique<InnerScopeExpressionAst> Body;

    /**
     * The optional @c else block of the loop. This will be executed if the original condition is immediately false, or
     * the iterable is already exhausted (no loops take place).
     */
    Unique<LoopElseStatementAst> ElseBlock;

    /**
     * Construct the LoopExpressionAst with the arguments matching the members.
     * @param[in] tok_loop The @c loop token that indicates the start of a loop expression.
     * @param[in] body The body of the loop.
     * @param[in] else_block The optional @c else block of the loop.
     */
    LoopExpressionAst(
        decltype(TokLoop) &&tok_loop,
        decltype(Body) &&body,
        decltype(ElseBlock) &&else_block);

    ~LoopExpressionAst() override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
