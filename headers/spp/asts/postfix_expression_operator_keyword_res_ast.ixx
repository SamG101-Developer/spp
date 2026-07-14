module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.mixins.compiler_stages;
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

COMMON_AST_IMPORTS

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

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

private:
    Unique<PostfixExpressionAst> _MappedFunc;
};
