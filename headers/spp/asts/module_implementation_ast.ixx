module;
#include <spp/macros.hpp>

export module spp.asts.module_implementation_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ModuleImplementationAst);

/// The implementation of a module, containing the module
/// members that define its functionality and structure.
SPP_EXP_CLS struct spp::asts::ModuleImplementationAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(ModuleImplementationAst);

  /// The module members in the implementation: function and
  /// class implementations, and other module-level constructs.
  Vec<Unique<Ast>> Members;

  explicit ModuleImplementationAst(
    decltype(Members) &&members);

  ~ModuleImplementationAst() override;

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

  auto Stage11_CodeGen(ScopeManager *, CompilerMetaData *, codegen::LlvmCtx *) -> llvm::Value* override;
};
