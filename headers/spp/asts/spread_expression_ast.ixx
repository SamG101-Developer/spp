module;
#include <spp/macros.hpp>

export module spp.asts.spread_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(SpreadExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// An element of a tuple literal spreading a tuple, or a template's pack, into
/// it ("(a, ..t)"). A closed tuple's is expanded into its elements when the
/// literal is analysed ("TupleLiteralAst"); an open one's (a template's pack,
/// "Tup[Ts]") stays, and gives the literal's type its elements ("Tup[A, Ts]").
/// So none is left by code generation.
SPP_EXP_CLS struct spp::asts::SpreadExpressionAst final : ExpressionAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(SpreadExpressionAst);

  /// The ".." token.
  Unique<TokenAst> TokUnpack;

  /// The tuple, or pack, spread.
  Unique<ExpressionAst> Expr;

  SpreadExpressionAst(
    decltype(TokUnpack) &&tok_unpack,
    decltype(Expr) &&expr);

  ~SpreadExpressionAst() override;

  auto Stage7_AnalyseSemantics(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(
    ScopeManager *sm, CompilerMetaData *meta,
    codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ReadExpr(
    ExprSubst const &sub) const -> Shared<ExpressionAst> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::SpreadExpressionAst)
