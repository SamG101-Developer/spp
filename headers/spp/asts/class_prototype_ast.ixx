module;
#include <spp/macros.hpp>

export module spp.asts.class_prototype_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ClassPrototypeAst);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct ClassImplementationAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeStatementAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);

/// The prototype of a class, defining its structure: its name
/// and any generic parameters. The attributes are defined in
/// the implementation ast, making the scoping rules easier.
SPP_EXP_CLS struct spp::asts::ClassPrototypeAst final : Ast, ModuleMemberAst, SupMemberAst, mixins::VisibilityAst {
  SPP_AST_KEY_FUNCTIONS(ClassPrototypeAst);

  /// The annotations applied to this class prototype.
  /// Typically, access modifiers in this context.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The "cls" keyword starting the class prototype.
  Unique<TokenAst> TokCls;

  /// Optional "!zero_type" annotation, indicating the class is
  /// guaranteed to occupy no storage. Layout only: the class
  /// is still linear unless it is also "Copy".
  AnnotationAst *ZeroTypeAnnotation;

  /// The name used to refer to the class, which must be unique
  /// within the scope.
  Shared<TypeAst> Name;

  /// An optional generic parameter group, defining generic
  /// types the class can use.
  Shared<GenericParameterGroupAst> GnParamGroup;

  /// The class attributes defined on the class prototype: the
  /// properties accessible through instances of the class.
  Unique<ClassImplementationAst> Impl;

  ClassPrototypeAst(
    decltype(Annotations) &&annotations,
    decltype(TokCls) &&tok_cls,
    decltype(Name) name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Impl) &&impl);

  ~ClassPrototypeAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto RegisterGenericSubstitution(Scope *scope, Unique<ClassPrototypeAst> &&new_ast) -> void;

  SPP_ATTR_NODISCARD auto GetRegisteredGenericSubstitutions() const -> Vec<Pair<Scope*, ClassPrototypeAst*>>;

  SPP_ATTR_NODISCARD auto GetClsSym() const -> Shared<TypeSymbol>;

  auto FillLlvmLayout(ScopeManager const *sm, TypeSymbol const *type_sym, codegen::LlvmCtx const *ctx) const -> void;

private:
  Vec<Pair<Scope*, Unique<ClassPrototypeAst>>> _GenericSubstitutions;

  Shared<TypeSymbol> _ClsSym;

  auto _GenerateSymbols(ScopeManager *sm) -> TypeSymbol*;
};
