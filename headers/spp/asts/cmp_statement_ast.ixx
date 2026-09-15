module;
#include <spp/macros.hpp>

export module spp.asts.cmp_statement_ast;
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

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto MarkFromUseStatement() -> void;

  SPP_ATTR_NODISCARD auto IsFromUseStatement() const -> bool;

private:
  bool _FromUseStatement;

  Shared<VariableSymbol> _AliasSym;
};
