module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordResAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordResAst final : PostfixExpressionOperatorAst {
    /**
     * The @c . token that indicates a member access operation.
     */
    Unique<TokenAst> TokDot;

    /**
     * The @c res token that indicates a keyword res operation.
     */
    Unique<TokenAst> TokRes;

    /**
     * The arguments passed to the res keyword. These will be passed into the mapped function for the generator type, to
     * provide uniform function call analysis.
     */
    Unique<FunctionCallArgumentGroupAst> FnArgGroup;

    /**
     * Construct the PostfixExpressionOperatorKeywordResAst with the arguments matching the members.
     * @param tok_dot The @c . token that indicates a member access operation.
     * @param tok_res The @c res token that indicates a keyword res operation.
     * @param arg_group The arguments passed to the res keyword.
     */
    PostfixExpressionOperatorKeywordResAst(
        decltype(TokDot) &&tok_dot,
        decltype(TokRes) &&tok_res,
        decltype(FnArgGroup) &&arg_group);

    ~PostfixExpressionOperatorKeywordResAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
    Shared<PostfixExpressionAst> _MappedFunc;
};
