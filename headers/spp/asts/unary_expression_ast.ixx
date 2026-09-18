module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct UnaryExpressionOperatorAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::UnaryExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(UnaryExpressionAst);

  /// The unary operator applied to the expression.
  Unique<UnaryExpressionOperatorAst> Op;

  /// The expression being operated on by the unary operator.
  Unique<ExpressionAst> Expr;

  UnaryExpressionAst(
    decltype(Op) &&tok_op,
    decltype(Expr) &&expr);

  ~UnaryExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
