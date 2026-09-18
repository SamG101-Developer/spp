module;
#include <spp/macros.hpp>

export module spp.asts.case_expression_branch_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CaseExpressionBranchAst);
use(spp::asts, struct BinaryExpressionAst);
use(spp::asts, struct CasePatternVariantAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct PatternGuardAst);
use(spp::asts, struct StatementAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

/// A branch on a "case" block. It contains the patterns to
/// match the "case" expression against, can be "guarded", and
/// contains the body of the block.
SPP_EXP_CLS struct spp::asts::CaseExpressionBranchAst final : Ast, mixins::TypeInferrableAst {
  SPP_AST_KEY_FUNCTIONS(CaseExpressionBranchAst);

  /// The optional comparison operator, for pattern matching
  /// branches like "== 123 { ... }".
  Unique<TokenAst> Op;

  /// The patterns this branch matches against. There can be
  /// more than 1 for non-destructuring operations.
  Vec<Unique<CasePatternVariantAst>> Patterns;

  /// The optional guard, a boolean expression that must be
  /// true, for destructuring patterns only.
  Unique<PatternGuardAst> Guard;

  /// The statements executed if the branch matches.
  Unique<InnerScopeExpressionAst> Body;

  CaseExpressionBranchAst(
    decltype(Op) &&op,
    decltype(Patterns) &&patterns,
    decltype(Guard) &&guard,
    decltype(Body) &&body);

  ~CaseExpressionBranchAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  auto MarkForIterLoopYield() -> void;

private:
  bool _ForIterLoopYield;

  /// Save the generated combined pattern expressions for
  /// codegen, without re-walking asts and scopes, which messes
  /// up the scope manager's alignment.
  Vec<Unique<BinaryExpressionAst>> _MappedPatFuncs;

  /// With multiple patterns, the llvm value is a logical OR of
  /// all the pattern matches (analysis guarantees they are all
  /// boolean). With 1 pattern, such as case-of patterns, its
  /// codegen is returned directly.
  auto _CodegenCombinePatterns(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) const -> llvm::Value*;
};
