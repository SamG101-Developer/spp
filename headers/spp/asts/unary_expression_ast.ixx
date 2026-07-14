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

COMMON_AST_IMPORTS

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

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};
