module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(InnerScopeExpressionAst);
use(spp::asts, struct Ast);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::InnerScopeExpressionAst : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(InnerScopeExpressionAst);

  /// The "{" token that starts the inner scope.
  Unique<TokenAst> TokL;

  /// The statements in the inner scope.
  Vec<Unique<StatementAst>> Members;

  /// The "}" token that ends the inner scope.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<InnerScopeExpressionAst>;

  InnerScopeExpressionAst(
    decltype(TokL) &&tok_l,
    decltype(Members) &&members,
    decltype(TokR) &&tok_r);

  ~InnerScopeExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Whether the value of the final statement in this scope
  /// goes nowhere. A block hands its final statement's value
  /// to whoever wrote the block, so normally the answer is no
  /// and that statement is exempt from the discarded-value
  /// check. A function body is the exception: S++ returns
  /// through "ret", so a body's final statement is discarded
  /// like any other.
  SPP_ATTR_NODISCARD virtual auto DiscardsFinalMember() const -> bool;

  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

  SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
