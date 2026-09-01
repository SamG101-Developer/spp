module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct BinaryExpressionAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

/**
 * The BinaryExpressionAst represents a binary expression in the source code, which consists of two operands (left-hand
 * side and right-hand side) and an operator that defines the operation to be performed on those operands. Each operator
 * maps the expressions into a method on the left-hand-side type, so @code 1 + 2@endcode will map to the method
 * @code 1.add(2)@endcode.
 */
SPP_EXP_CLS struct spp::asts::BinaryExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS;

  /**
   * The left-hand side expression of the binary expression. This is the first operand.
   */
  Unique<ExpressionAst> Lhs;

  /**
   * The operator token that represents the binary operation. This indicates the type of operation being performed.
   */
  Unique<TokenAst> TokOp;

  /**
   * The right-hand side expression of the binary expression. This is the second operand.
   */
  Unique<ExpressionAst> Rhs;

  struct {
    std::size_t OriginalPosStart;
    std::size_t OriginalPosEnd;
  } Source;

  /**
   * Whether this is one of the two logical keyword operators, @c and or @c or.
   *
   * @n
   * They are the only binary operators that do not map to a method on their left operand. Every other one does,
   * which is what lets a type give it a meaning; these two cannot, because their meaning is control flow rather than
   * a value: the right operand is evaluated only if the left one did not already settle the answer. A method call
   * evaluates its argument to pass it, so an overloadable @c and would evaluate both sides whatever the left said -
   * which is what @c not avoids by being built in, and what these now avoid the same way. Both operands are required
   * to be @c Bool for the same reason.
   */
  SPP_ATTR_NODISCARD auto IsLogicalOperator() const -> bool;

  /**
   * Construct the BinaryExpressionAst with the arguments matching the members.
   * @param[in] lhs The left-hand side expression of the binary expression.
   * @param[in] tok_op The operator token that represents the binary operation.
   * @param[in] rhs The right-hand side expression of the binary expression.
   */
  BinaryExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs);

  ~BinaryExpressionAst() override;

  /**
   * Ensure the operator exists over the left-hand-side type, compatible with the right-hand-side type. This is done
   * by. Also handle any binary fold operations, like @code a + ..@endcode.
   * @param[in] sm The scope manager to use for type checking.
   * @param[in,out] meta Associated metadata.
   */
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /**
   * Forward the memory checking to the mapped function. This checks the created argument group for the mapped
   * function.
   * @param[in] sm The scope manager to use for memory checking.
   * @param[in,out] meta Associated metadata.
   */
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /**
   * Resolve the binary expression at compile time. This maps to the comptime resolution of the mapped function.
   * @param sm The scope manager to use for resolution.
   * @param meta Associated metadata.
   * @return The result of the compile time resolution.
   */
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /**
   * Forward the code generation to the mapped function. This just generates a standard function call. Some functions
   * like @code (1 + 2)@endcode will map to LLVM IR directly.
   * @param sm The scope manager to use for code generation.
   * @param meta Associated metadata.
   * @param ctx The LLVM context to use for code generation.
   * @return The LLVM value generated from this AST.
   */
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /**
   * Forward the type checking to the mapped function. This just applies standard type inference from a function call.
   * @param[in] sm The scope manager to use for type inference.
   * @param[in,out] meta Associated metadata.
   * @return The inferred type of the binary expression, which is the return type of the mapped function.
   */
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
  /**
   * The AST that represents the functional version of this binary expression. For example, @code 1 + 2@endcode
   * becomes @c 1.add(2). The mapped function itself has its own internal mapping, in this case that would be
   * @c std::number::S32::add(1, 2).
   */
  Shared<PostfixExpressionAst> _MappedFunc;

  /**
   * Whether the logical path has already analysed this expression. @c _MappedFunc is what marks every other operator
   * as done - it is set once and returned early on afterwards - and a logical operator never gets one, so it needs a
   * mark of its own. Without it the analysis runs again on each visit, and the comparison-chain rewrite moves the
   * operands out of the expression a second time.
   */
  bool _LogicalAnalysed = false;

  /**
   * Whether the operator is @c and or @c or, decided from the token and then remembered. It cannot be read back off
   * @c TokOp on demand: converting the expression into a call moves the operands and the operator out of it, so by
   * the time anything asks, the token is gone. It is settled again after the comparison-chain rewrite, which is what
   * turns @code a < b < c@endcode into an @c and that was not written as one.
   */
  bool _IsLogical;
};
