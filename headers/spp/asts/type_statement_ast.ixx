module;
#include <spp/macros.hpp>

export module spp.asts.type_statement_ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.statement_ast;
import spp.asts.sup_member_ast;
import spp.asts.type_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TypeStatementAst);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct UseStatementAst);

/// Aliases a type to a new name in this scope. Generic
/// parameters allow aliasing more complex types, such as
/// vectors or partially specialized hash maps, for example
/// "type SecureByteMap[T] = std::collections::HashMap[K=Byte,
/// V=T, A=SecureAlloc[(K, V)]]".
SPP_EXP_CLS struct spp::asts::TypeStatementAst final :
  StatementAst, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeStatementAst);

  /// The annotations applied to this type statement; typically
  /// access modifiers.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The "type" token that starts this statement.
  Unique<TokenAst> TokType;

  /// The type being defined: for "type Str = std::Str", this is
  /// "Str".
  Shared<TypeIdentifierAst> NewType;

  /// The generic parameters for the new type. For example,
  /// "type MyVector[T] = Vec[T, A=SomeAlloc]" defines "T" as a
  /// generic internal to this type statement only.
  Shared<GenericParameterGroupAst> GnParamGroup;

  /// The "=" token separating the new type from the old type.
  Unique<TokenAst> TokAssign;

  /// The type this statement aliases, as it was written: the
  /// "std::Str" of "type Str = std::Str". What it resolves to
  /// once the chain of aliases behind it has been followed is
  /// kept on the symbol instead, as "AliasInfo::Resolved": one
  /// piece of syntax, one meaning, and neither rewritten to
  /// hold the other at some point in the walk.
  Shared<TypeAst> OldType;

  struct {
    Shared<TypeAst> OriginalOldType;
  } Source;

  TypeStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokType) &&tok_type,
    decltype(NewType) new_type,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(TokAssign) &&tok_assign,
    decltype(OldType) old_type);

  ~TypeStatementAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

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
  bool _Generated;
  bool _FromUseStatement;
  Shared<TypeSymbol> _AliasSym;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeStatementAst)
