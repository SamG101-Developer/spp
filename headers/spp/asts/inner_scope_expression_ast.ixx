module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.analyse.utils.drop_utils;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use_common(InnerScopeExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

use_alias(spp::asts, Ast);

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

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Whether the value of the final statement in this scope
  /// goes nowhere. A block hands its final statement's value
  /// to whoever wrote the block, so normally the answer is no
  /// and that statement is exempt from the discarded-value
  /// check. A function body is the exception: S++ returns
  /// through "ret", so a body's final statement is discarded
  /// like any other.
  SPP_ATTR_NODISCARD virtual auto DiscardsFinalMember() const -> bool;

  /// A block diverges when its last statement does.
  SPP_ATTR_NODISCARD auto Diverges(ScopeManager *sm, CompilerMetaData *meta) -> bool override;

  SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// Member "i" makes a temporary whose value goes nowhere: it is
  /// dropped once made ("expr_utils::DiscardValue").
  auto SetDropsDiscarded(std::size_t i) -> void;

protected:
  /// What this scope still owns at its closing brace, dropped there
  /// after what it deferred ("OwnedIn").
  Vec<analyse::utils::drop_utils::OwnedValue> _OwnedAtEnd;

  /// What each member's "is" bindings still own at its end, dropped
  /// there ("drop_utils::EndStatementBindings"), one entry per member.
  Vec<Vec<analyse::utils::drop_utils::OwnedValue>> _StatementDrops;

  /// Per member, whether its value is a temporary dropped once made.
  Vec<bool> _DropsDiscarded;
};
