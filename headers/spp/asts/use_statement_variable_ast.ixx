module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_variable_ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UseStatementVariableAst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct CmpStatementAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

/// Reduces a fully qualified variable into the current scope,
/// making the symbol accessible without its namespace.
/// Internal symbol mapping for variables or namespaces is
/// used.
SPP_EXP_CLS struct spp::asts::UseStatementVariableAst final : StatementAst, ModuleMemberAst {
  SPP_AST_KEY_FUNCTIONS(UseStatementVariableAst);

  /// The annotations applied to this use statement; typically
  /// access modifiers.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The "use" token that starts this statement.
  Unique<TokenAst> TokUse;

  /// The old (fully qualified) variable being reduced: for
  /// "use std::annotations::public", this is
  /// "std::annotations::public".
  Unique<ExpressionAst> OldVar;

  UseStatementVariableAst(
    decltype(Annotations) &&annotations,
    decltype(TokUse) &&tok_use,
    decltype(OldVar) old_var);

  ~UseStatementVariableAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

private:
  /// Whether this use statement has been generated yet. Use
  /// statements can be defined at the top level (module/sup)
  /// or inside function bodies; inside a function body all
  /// analysis steps must run together, otherwise they run in
  /// their correct stage.
  bool _Generated;

  /// The cmp statement generated from this use statement, so
  /// it is analysed uniformly with "type Str = std::Str".
  Unique<CmpStatementAst> _Conversion;
};
