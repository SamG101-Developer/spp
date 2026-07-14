module;
#include <spp/macros.hpp>

export module spp.asts.loop_expression_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.compiler_stages;
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

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::LoopExpressionAst : PrimaryExpressionAst {
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

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

protected:
    std::tuple<ExpressionAst*, Unique<TypeAst>, analyse::scopes::Scope*>* _LoopExitTypeInfo;
};
