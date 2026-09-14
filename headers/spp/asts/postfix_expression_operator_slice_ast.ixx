module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_slice_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorSliceAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorSliceAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorSliceAst);

  /// The "[" token that starts the slice expression.
  Unique<TokenAst> TokL;

  /// The optional "mut" token, making the slice mutable.
  Unique<TokenAst> TokMut;

  /// The lower bound expression used for the slice.
  Unique<ExpressionAst> ExprLBound;

  /// The "to" keyword, separating the bounds of the slice.
  Unique<TokenAst> TokTo;

  /// The upper bound expression used for the slice.
  Unique<ExpressionAst> ExprRBound;

  /// The "]" token that ends the slice expression.
  Unique<TokenAst> TokR;

  PostfixExpressionOperatorSliceAst(
    decltype(TokL) &&tok_l,
    decltype(TokMut) &&tok_mut,
    decltype(ExprLBound) &&expr_l_bound,
    decltype(TokTo) &&tok_to,
    decltype(ExprRBound) &&expr_r_bound,
    decltype(TokR) &&tok_r);

  ~PostfixExpressionOperatorSliceAst() override;

  /// The left-hand-side must be "sliceable", in the same way
  /// that an iterator-based loop's condition must be
  /// "iterable". Either "SliceRef" or "SliceMut" must be
  /// superimposed over the left-hand-side, depending on the
  /// presence of the "mut" keyword.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Infer the type from the mapped slice function on the
  /// left-hand-side type. For example, "hello"[5 to 7] becomes
  /// "hello".slice_ref(5, 6), and its return type is used.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};
