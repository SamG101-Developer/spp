module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_async_ast;
import spp.asts.unary_expression_operator_ast;
import spp.codegen.llvm_ctx;
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
    std::unique_ptr<TokenAst> tok_async;

    /**
     * Construct the UnaryExpressionOperatorAsyncAst with the arguments matching the members.
     * @param tok_async The @c async keyword that indicates an asynchronous operation.
     */
    explicit UnaryExpressionOperatorAsyncAst(
        decltype(tok_async) &&tok_async);

    ~UnaryExpressionOperatorAsyncAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
