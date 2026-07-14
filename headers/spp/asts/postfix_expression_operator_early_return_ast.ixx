module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorEarlyReturnAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorEarlyReturnAst final : PostfixExpressionOperatorAst {
    /**
     * The @c ? token that indicates an early return in a postfix expression. This token is used to signify that the
     * expression should be checked for its result-type failure type, and if it matches, the expression will lift the
     * error to the caller.
     */
    Unique<TokenAst> TokQst;

    /**
     * Construct the PostfixExpressionOperatorEarlyReturnAst with the arguments matching the members.
     * @param[in] tok_qst The @c ? token that indicates an early return in a postfix expression.
     */
    explicit PostfixExpressionOperatorEarlyReturnAst(
        decltype(TokQst) &&tok_qst);

    ~PostfixExpressionOperatorEarlyReturnAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};
