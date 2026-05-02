module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct UnaryExpressionAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionAst final : ExpressionAst {
    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    Unique<UnaryExpressionOperatorAst> Op;

    /**
     * The expression that is being operated on by the unary operator.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the unary operation.
     * @param[in] expr The expression that is being operated on by the unary operator.
     */
    UnaryExpressionAst(
        decltype(Op) &&tok_op,
        decltype(Expr) &&expr);

    ~UnaryExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
