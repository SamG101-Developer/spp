module;
#include <spp/macros.hpp>

export module spp.asts.module_prototype_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ModulePrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct ModuleImplementationAst);
use(spp::compiler, struct CompilerBoot);

/// A prototype for a module, containing the implementation of
/// the module.
SPP_EXP_CLS struct spp::asts::ModulePrototypeAst final : Ast {
  /// The file path of the module prototype, used by the
  /// compiler to resolve module imports. Not got from parsing
  /// children asts.
  std::filesystem::path FilePath = "";

  /// The module implementation ast this prototype represents.
  Unique<ModuleImplementationAst> Impl;

  explicit ModulePrototypeAst(
    decltype(Impl) &&impl);

  ~ModulePrototypeAst() override;

  SPP_AST_KEY_FUNCTIONS(ModulePrototypeAst);

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

  SPP_ATTR_NODISCARD auto Name() const -> Unique<IdentifierAst>;
};
