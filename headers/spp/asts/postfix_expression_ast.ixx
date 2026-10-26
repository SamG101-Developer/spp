module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionAst);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionOperatorAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionAst);

  /// The base expression the postfix operation is applied to.
  Unique<ExpressionAst> Lhs;

  /// The postfix operator, indicating the type of operation
  /// being performed.
  Unique<PostfixExpressionOperatorAst> Op;

  struct {
    mutable Shared<TypeAst> CachedInference;
  } Source;

  PostfixExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(Op) &&op);

  ~PostfixExpressionAst() override;

  /// A postfix expression over @c lhs that is analysed already:
  /// an operator mapped to a call over its operand
  /// ("x.await", "x[i]") builds the call over a clone of it,
  /// which a second analysis would open the scopes of again.
  static auto NewOverAnalysedLhs(
    decltype(Lhs) &&lhs, decltype(Op) &&op) -> Unique<PostfixExpressionAst>;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto ReadExpr(
    ExprSubst const &sub) const -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// Set by "NewOverAnalysedLhs": stage 7 does not analyse "Lhs".
  bool _LhsAnalysed = false;
};
