module;
#include <spp/macros.hpp>

export module spp.compiler.compiler_boot;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import spp.compiler.out_layout;
import spp.utils.progress;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::compiler {
  SPP_EXP_CLS struct CompilerBoot;
  SPP_EXP_CLS struct Module;
  SPP_EXP_CLS struct ModuleTree;
}

SPP_EXP_CLS struct spp::compiler::CompilerBoot {
  /// Only tests whose fully qualified name contains this are built into the
  /// harness; empty builds all.
  Str TestNameFilter;

  /// Only tests in this group are built into the harness; empty builds all. Can
  /// be used with the test name filter too (double filter application).
  Str TestGroupFilter;

  /// The fully qualified name of each test the harness runs, in the order it
  /// runs them.
  Vec<Str> TestNames;

  /// For the C++ unit tests: code generation stops once every module has been
  /// verified and the modules link together into one. Nothing is optimised,
  /// written or linked, and an invalid module throws rather than aborting the
  /// process, so a "should pass" test fails on the code it generates as well as
  /// on its analysis.
  bool VerifyOnly = false;

  /// The build mode that a "!cfg(mode=...)" condition is checked against.
  BuildMode Mode = BuildMode::DEV;

  auto Lex(
    utils::ProgressBar &bar, ModuleTree &tree) -> void;

  auto Parse(
    utils::ProgressBar &bar, ModuleTree &tree) -> void;

  auto Stage1_PreProcess(
    utils::ProgressBar &bar, ModuleTree &tree,
    asts::Ast *ctx) -> void;

  auto Stage2_GenTopLvlScopes(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage3_GenTopLvlAliases(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage4_ResolveDeclarations(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage5_LoadSupScopes(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage5_5_AttachSupScopes(
    utils::ProgressBar &bar,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage6_PreAnalyseSemantics(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage7_AnalyseSemantics(
    utils::ProgressBar &bar, ModuleTree &tree, bool is_exe,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage8_CheckMemory(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage9_5_Monomorphise(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  auto Stage10_PreCodeGen(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm) -> void;

  SPP_ATTR_NODISCARD auto Stage11_CodeGen(
    utils::ProgressBar &bar, ModuleTree &tree,
    analyse::scopes::ScopeManager *sm,
    unsigned opt_level) -> bool;

private:
  Vec<asts::ModulePrototypeAst*> _Modules;

  /// The LLVM context every module of this compile is built into, so they can
  /// be linked. Owned by the compile, not the process: a named struct type
  /// lives in its context, and a later compile reusing it would be handed this
  /// one's layout for a same-named class. Declared before "_LlvmCtxs", so it
  /// outlives the modules in it.
  Unique<llvm::LLVMContext> _LlvmContext;

  /// Store each broad/custom LLVM context for each module, so the vector can be
  /// iterated and reused for multiple stages.
  Vec<Unique<codegen::LlvmCtx>> _LlvmCtxs;

  /// The prototype the program is entered through, resolved by stage 7 through
  /// "_ValidateEntryPoint". Nullptr for a library, which has no single way in.
  asts::FunctionPrototypeAst *_EntryPoint = nullptr;

  /// The linkage name of "_EntryPoint" , or an empty string if there is no
  /// entry point or it never reached code generation.
  SPP_ATTR_NODISCARD auto _EntryPointLlvmName() const -> Str;

  /// Run the validation specific to the entry point (is it present and callable
  /// etc.), and only for exes. For libraries it could be present, but would be
  /// meaningless. Todo: Enforce no main.spp for LIB builds?
  auto _ValidateEntryPoint(analyse::scopes::ScopeManager *sm) -> void;

  /// The source of the entry point a test build is run through: a "main" that
  /// calls every discovered unit test in turn, naming each one before it runs
  /// so that a test which takes the process down with it is still identifiable.
  auto _GenerateTestHarness(
    ModuleTree &tree, StrView name_filter, StrView group_filter) -> Str;

  /// When the module tree is first converted into the scope tree, namespace
  /// scopes either have to be created, or moved into.
  static auto _MoveScopeManagerToNs(
    analyse::scopes::ScopeManager *sm, Module const &mod) -> void;

  /// Run one stage over every module in order, each with the scope manager
  /// moved into its namespace and a fresh "meta" for the stage, its walk
  /// restarted after, and the progress bar" advanced (if given). An analysis
  /// stage (6-9) also makes the instantiations lookups read as they need them,
  /// for itself.
  auto _EachModule(
    ModuleTree &tree, analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerStage stage, utils::ProgressBar *bar,
    FuncRef<void(std::size_t, asts::meta::CompilerMetaData &)> body) -> void;
};
