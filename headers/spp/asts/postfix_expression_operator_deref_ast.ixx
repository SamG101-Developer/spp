module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorDerefAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorDerefAst final : PostfixExpressionOperatorAst {
    /**
     * The @c * token that indicates a dereference operation. This is used to extract a copyable value from a borrow.
     */
    Unique<TokenAst> TokDeref;

    /**
     * Construct the PostfixExpressionOperatorDerefAst with the arguments matching the members.
     * @param tok_deref The @c * token that indicates a dereference operation.
     */
    explicit PostfixExpressionOperatorDerefAst(
        decltype(TokDeref) &&tok_deref);

    ~PostfixExpressionOperatorDerefAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
