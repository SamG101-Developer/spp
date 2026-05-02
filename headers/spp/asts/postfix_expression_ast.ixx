module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionAst final : ExpressionAst {
    /**
     * The left-hand side expression of the postfix expression. This is the base expression on which the postfix operation
     * is applied.
     */
    Unique<ExpressionAst> Lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    Unique<PostfixExpressionOperatorAst> Op;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the postfix expression.
     * @param[in] op The operator token that represents the postfix operation.
     */
    PostfixExpressionAst(
        decltype(Lhs) &&lhs,
        decltype(Op) &&op);

    ~PostfixExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<Ast *> override;
};
