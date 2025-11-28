module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorEarlyReturnAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
    /**
     * The @c ? token that indicates an early return in a postfix expression. This token is used to signify that the
     * expression should be checked for its result-type failure type, and if it matches, the expression will lift the
     * error to the caller.
     */
    std::unique_ptr<TokenAst> tok_qst;

    /**
     * Construct the PostfixExpressionOperatorEarlyReturnAst with the arguments matching the members.
     * @param[in] tok_qst The @c ? token that indicates an early return in a postfix expression.
     */
    explicit PostfixExpressionOperatorEarlyReturnAst(
        decltype(tok_qst) &&tok_qst);

    ~PostfixExpressionOperatorEarlyReturnAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
