module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_ast;
import spp.asts._fwd;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BinaryExpressionAst;
}


/**
 * The BinaryExpressionAst represents a binary expression in the source code, which consists of two operands (left-hand
 * side and right-hand side) and an operator that defines the operation to be performed on those operands. Each operator
 * maps the expressions into a method on the left-hand-side type, so @code 1 + 2@endcode will map to the method
 * @code 1.add(2)@endcode.
 */
SPP_EXP_CLS struct spp::asts::BinaryExpressionAst final : ExpressionAst {
private:
    std::shared_ptr<PostfixExpressionAst> m_mapped_func;

public:
    /**
     * The left-hand side expression of the binary expression. This is the first operand.
     */
    std::unique_ptr<ExpressionAst> lhs;

    /**
     * The operator token that represents the binary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the binary expression. This is the second operand.
     */
    std::unique_ptr<ExpressionAst> rhs;

    /**
     * Construct the BinaryExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the binary expression.
     * @param[in] tok_op The operator token that represents the binary operation.
     * @param[in] rhs The right-hand side expression of the binary expression.
     */
    BinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~BinaryExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    /**
     * Ensure the operator exists over the left-hand-side type, compatible with the right-hand-side type. This is done
     * by. Also handle any binary fold operations, like @code a + ..@endcode.
     * @param[in] sm The scope manager to use for type checking.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Forward the memory checking to the mapped function. This checks the created argument group for the mapped
     * function.
     * @param[in] sm The scope manager to use for memory checking.
     * @param[in,out] meta Associated metadata.
     */
    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Forward the code generation to the mapped function. This just generates a standard function call. Some functions
     * like @code (1 + 2)@endcode will map to LLVM IR directly.
     * @param sm The scope manager to use for code generation.
     * @param meta Associated metadata.
     * @param ctx The LLVM context to use for code generation.
     * @return The LLVM value generated from this AST.
     */
    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    /**
     * Forward the type checking to the mapped function. This just applies standard type inference from a function call.
     * @param[in] sm The scope manager to use for type inference.
     * @param[in,out] meta Associated metadata.
     * @return The inferred type of the binary expression, which is the return type of the mapped function.
     */
    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
