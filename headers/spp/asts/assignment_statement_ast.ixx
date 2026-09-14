module;
#include <spp/macros.hpp>

export module spp.asts.assignment_statement_ast;
import spp.asts.ast_kind;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(AssignmentStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

/// An assignment statement allows for one more more values to
/// be assigned inline. Compound assignment expressions are
/// binary expressions; assignment is restricted and Void
/// returning (a statements). Assignment is designed to support
/// "a, b = b, a"
SPP_EXP_CLS struct spp::asts::AssignmentStatementAst final : StatementAst {
  SPP_AST_KEY_FUNCTIONS(AssignmentStatementAst);

  /// The storage names on the left-hand-side.
  Vec<Unique<ExpressionAst>> Lhs;

  /// The "=" token.
  Unique<TokenAst> TokAssign;

  /// The values on the right-hand-side.
  Vec<Unique<ExpressionAst>> Rhs;

  AssignmentStatementAst(
    decltype(Lhs) &&lhs,
    decltype(TokAssign) &&tok_assign,
    decltype(Rhs) &&rhs);

  ~AssignmentStatementAst() override;

  /// The left-hand-side must be semi-symbolically valid, ie
  /// an identifier, postfix runtime member access, or a deref
  /// over a runtime member access or index/slice operation.
  /// Lots of mutability checks concerning mutable values and
  /// mutable borrow semantics.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Perform all the initialisation/moving checks, and possible
  /// partial move resolutions too.
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// As long as all the right-hand-side values are compile-time
  /// evaluatable, move the resolved right-side values into the
  /// left-side symbols.
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Run the codegen steps to convert the right-side values and
  /// inject them into the left-side symbols. All the right-side
  /// values are evaluated first, allowing "a, b = b, a" to evaluate.
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
