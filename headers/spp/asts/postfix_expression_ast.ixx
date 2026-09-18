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

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
