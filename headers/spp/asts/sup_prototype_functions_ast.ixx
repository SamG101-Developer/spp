module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_functions_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.module_member_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(SupPrototypeFunctionsAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct SupImplementationAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// A superimposition of methods over a type, used to add
/// behaviour to a type. For example, to extend the "std::Str"
/// type with additional methods:
///
///   sup std::Str {
///       fun to_upper() -> std::Str { ... }
///   }
SPP_EXP_CLS struct spp::asts::SupPrototypeFunctionsAst final : Ast, ModuleMemberAst {
  SPP_AST_KEY_FUNCTIONS(SupPrototypeFunctionsAst);

  /// The "sup" keyword that starts the superimposition.
  Unique<TokenAst> TokSup;

  /// The generics available for this superimposition, used to
  /// superimpose over generic types (all generics must be used
  /// by the type being extended).
  Unique<GenericParameterGroupAst> GnParamGroup;

  /// The name of the type being extended. This type gains the
  /// methods defined in the body of this superimposition.
  Shared<TypeAst> Name;

  /// The body of the superimposition: the methods (each a
  /// FunctionPrototypeAst) being added to the type.
  Unique<SupImplementationAst> Impl;

  SupPrototypeFunctionsAst(
    decltype(TokSup) &&tok_sup,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Name) name,
    decltype(Impl) &&impl);

  ~SupPrototypeFunctionsAst() override;

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
};
