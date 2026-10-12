module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CaseExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::utils::case_utils, struct CaseCoverage);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// Represents conditional branching, either if-like or
/// switch-like. If the "of" keyword follows the condition,
/// pattern matching is used, by combining the condition with
/// the partial fragments that are the branches.
SPP_EXP_CLS struct spp::asts::CaseExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(CaseExpressionAst);

  /// The "case" keyword token.
  Unique<TokenAst> TokCase;

  /// The expression being matched against the branches. For
  /// pattern matching it may not be boolean (but combined
  /// with the partial fragments it will be). Otherwise, the
  /// expression itself must be boolean.
  Unique<ExpressionAst> Cond;

  /// The optional "of" keyword, indicating pattern matching
  /// against partial fragments.
  Unique<TokenAst> TokOf;

  /// The branches of the case expression. Symbols can be
  /// created inside the "case" expression scope, available to
  /// all branches, if need be.
  Vec<Unique<CaseExpressionBranchAst>> Branches;

  /// Set when this "case" was desugared from an "is" expression
  /// ("x is T(..)"), whose two branches yield "true" and
  /// "false".
  bool LoweredFromIsExpr = false;

  /// Set when this "case" is the lowering of the "?" operator,
  /// whose value branch yields the operand's value and whose
  /// "else" branch returns.
  bool LoweredFromTryOperator = false;

  CaseExpressionAst(
    decltype(TokCase) &&tok_case,
    decltype(Cond) &&cond,
    decltype(TokOf) &&tok_of,
    decltype(Branches) &&branches);

  ~CaseExpressionAst() override;

  static auto NewNonPatternMatch(
    decltype(TokCase) &&tok_case,
    decltype(Cond) &&cond,
    Unique<InnerScopeExpressionAst> &&first,
    decltype(Branches) &&branches)
    -> Unique<CaseExpressionAst>;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// A "case" block only terminates if one or more of its
  /// branches can terminate, as it has to be assumed that the
  /// terminating branch will execute, to cover all bases.
  SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

  /// What this "case" matches ("case_utils::CoverageOf"): worked out
  /// when Stage 7 analyses it, and read by every later stage.
  auto Coverage(ScopeManager *sm, CompilerMetaData *meta) -> CaseCoverage const&;

private:
  /// See "Coverage". Not carried by "Clone": a copy is analysed afresh.
  Shared<CaseCoverage> _Coverage;
};
