module;
#include <spp/macros.hpp>

export module spp.asts.cmp_statement_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CmpStatementAst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct UseStatementVariableAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// A compile time definition statement at either the module or
/// superimposition level. It is analogous to Rust's "const"
/// statement.
SPP_EXP_CLS struct spp::asts::CmpStatementAst final :
  StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
  SPP_AST_KEY_FUNCTIONS(CmpStatementAst);

  friend struct UseStatementVariableAst;
  // Todo: Copy the "_Generated" logic from the "UseStatementAst" and add local insertions into testing?

  /// The annotations applied to this cmp statement, typically
  /// access modifiers in this context.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The "cmp" token, showing a compile time definition is
  /// being made.
  Unique<TokenAst> TokCmp;

  /// The name used to refer to the compile time definition,
  /// which must be unique within the scope.
  Shared<IdentifierAst> Name;

  /// The ":" token separating the name from the type.
  Unique<TokenAst> TokColon;

  /// The type the compile time definition holds. It must be
  /// specified rather than inferred, because the type must be
  /// known at a stage that completes before type-inference can
  /// be considered.
  Shared<TypeAst> Type;

  /// The "=" token separating the type from the value.
  Unique<TokenAst> TokAssign;

  /// The value, evaluated at compile time, so it must be
  /// constant. It can be any expression valid in a compile time
  /// context, such as a literal or an object initialization
  /// that only uses compile time values.
  Unique<ExpressionAst> Value;

  CmpStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(Value) &&value);

  ~CmpStatementAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto MarkFromUseStatement() -> void;

  SPP_ATTR_NODISCARD auto IsFromUseStatement() const -> bool;

  /// Analyse and fold this constant ahead of the stage 7 walk, if the walk has not reached it yet: a fold reads a
  /// constant through its symbol ("IdentifierAst::EvaluateCompTime"), which holds the value as written until then,
  /// never analysed. The statement is walked from its own scope, with metadata of its own, so its scopes are built
  /// where the walk would build them, and the walk skips it on reaching it. One still being analysed (a value that
  /// reads itself again) cannot be read, so "site" is raised as non-constant.
  auto AnalyseAhead(Ast const &site, ScopeManager const *sm, CompilerMetaData const *meta) -> void;

private:
  bool _FromUseStatement;

  /// How far stage 7 has got with this statement ("AnalyseAhead").
  enum class _Stage7Progress { kPending, kAnalysing, kAnalysedAhead, kDone } _Stage7 = _Stage7Progress::kPending;

  /// The value folded in stage 7, which replaces "Value" once stage 8 has checked the analysed one, as that walks the
  /// scopes the analysis built ("case" branches). Null for a value naming a generic, which is kept as written.
  Unique<ExpressionAst> _FoldedValue;

  /// Stage 7 in the statement's own scope: the value analysed, checked against the type, and folded.
  auto _AnalyseValue(ScopeManager *sm, CompilerMetaData *meta) -> void;

  Shared<VariableSymbol> _AliasSymbol;
};
