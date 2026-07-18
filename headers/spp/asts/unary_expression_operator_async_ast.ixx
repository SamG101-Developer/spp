module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_async_ast;
import spp.asts.unary_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAsyncAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
    /**
     * The @c async keyword that indicates an asynchronous operation. This is used to mark the following function call
     * as called asynchronously.
     */
    Unique<TokenAst> TokAsync;

    /**
     * Construct the UnaryExpressionOperatorAsyncAst with the arguments matching the members.
     * @param tok_async The @c async keyword that indicates an asynchronous operation.
     */
    explicit UnaryExpressionOperatorAsyncAst(
        decltype(TokAsync) &&tok_async);

    ~UnaryExpressionOperatorAsyncAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
