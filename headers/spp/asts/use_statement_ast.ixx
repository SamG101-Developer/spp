module;
#include <spp/macros.hpp>

export module spp.asts.use_statement_ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UseStatementAst);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeStatementAst);
use(spp::asts, struct TypeAst);

/// Reduces a fully qualified type into the current scope,
/// making the symbol accessible without its namespace. It is
/// internally mapped to a TypeStatementAst: "use std::Str" is
/// equivalent to "type Str = std::Str".
SPP_EXP_CLS struct spp::asts::UseStatementAst final : StatementAst, ModuleMemberAst {
  SPP_AST_KEY_FUNCTIONS(UseStatementAst);

  /// The annotations applied to this use statement; typically
  /// access modifiers.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The "use" token that starts this statement.
  Unique<TokenAst> TokUse;

  /// The old (fully qualified) type being reduced: for
  /// "use std::Str", this is "std::Str".
  Shared<TypeAst> OldType;

  UseStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokUse) &&tok_use,
    decltype(OldType) old_type);

  ~UseStatementAst() override;

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

  /// The type statement generated from this use statement, so
  /// new types are analysed uniformly with
  /// "type Str = std::Str".
  Unique<TypeStatementAst> _Conversion;
};
