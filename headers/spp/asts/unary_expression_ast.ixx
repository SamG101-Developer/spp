module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
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
    std::unique_ptr<UnaryExpressionOperatorAst> op;

    /**
     * The expression that is being operated on by the unary operator.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the unary operation.
     * @param[in] expr The expression that is being operated on by the unary operator.
     */
    UnaryExpressionAst(
        decltype(op) &&tok_op,
        decltype(expr) &&expr);

    ~UnaryExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
