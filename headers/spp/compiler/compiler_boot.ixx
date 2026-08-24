module;
#include <spp/macros.hpp>

export module spp.compiler.compiler_boot;
import spp.codegen.llvm_ctx;
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
  /**
   * The name the linked executable is given: the project's own folder name, so a project in "foo/" builds "foo".
   * @param[in] project_root The directory holding the project's "src", "out" and so on.
   */
  SPP_ATTR_NODISCARD static auto ExecutableName(
    std::filesystem::path const &project_root)
    -> Str;

  /** Only tests whose fully qualified name contains this are built into the harness; empty builds all. */
  Str TestNameFilter;

  /** Only tests in this group are built into the harness; empty builds all. */
  Str TestGroupFilter;

  /** How many tests the generated harness ended up running. */
  std::size_t TestCount = 0;

  /** The fully qualified name of each test the harness runs, in the order it runs them. */
  Vec<Str> TestNames;

  auto Lex(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void;

  auto Parse(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void;

  auto Stage1_PreProcess(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    asts::Ast *ctx)
    -> void;

  auto Stage2_GenTopLvlScopes(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage3_GenTopLvlAliases(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage4_QualifyTypes(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage5_LoadSupScopes(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage6_PreAnalyseSemantics(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage7_AnalyseSemantics(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    bool is_exe,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage8_CheckMemory(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage9_CompTimeResolve(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage9_5_Monomorphise(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage10_PreCodeGen(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void;

  auto Stage11_CodeGen(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm,
    unsigned opt_level)
    -> void;

private:
  Vec<asts::ModulePrototypeAst*> _Modules;
  Vec<Unique<codegen::LlvmCtx>> _LlvmCtxs;

  /**
   * The prototype the program is entered through, as resolved by @c _ValidateEntryPoint . Null for a library build,
   * which has no single way in.
   */
  asts::FunctionPrototypeAst *_EntryPoint = nullptr;

  /**
   * The linkage name of @c _EntryPoint , or an empty string if there is no entry point or it never reached code
   * generation.
   */
  SPP_ATTR_NODISCARD auto _EntryPointLlvmName() const
    -> Str;

  auto _ValidateEntryPoint(
    analyse::scopes::ScopeManager *sm)
    -> void;

  /**
   * Copy every module into one and run the optimization pipeline over that, so the optimizer can see across the file
   * boundaries the per-module walk leaves in place. Written beside the per-module ir as @c lto.ll rather than
   * replacing it.
   * @param[in] out_path The directory the per-module ir was written to.
   */
  auto _LinkTimeOptimize(
    std::filesystem::path const &out_path,
    unsigned opt_level)
    -> void;

  /**
   * Link @p object_file against the ffi runtimes the project's packages ship, producing the executable.
   * @param[in] object_file The object file emitted from the combined module.
   */
  auto _LinkExecutable(
    std::filesystem::path const &object_file)
    -> void;

  /**
   * Every native library a package ships, found by shape: @c \<package\>/ffi/\<name\>/lib/\<name\>.so .
   * @param[in] project_root The directory to sweep.
   */
  SPP_ATTR_NODISCARD static auto _FfiLibraries(
    std::filesystem::path const &project_root)
    -> Vec<std::filesystem::path>;

  /**
   * The source of the entry point a test build is run through: a "main" that calls every discovered unit test in turn,
   * naming each one before it runs so that a test which takes the process down with it is still identifiable.
   * @param[in] tree The modules already parsed, searched for functions carrying the "unit_test" annotation.
   * @param[in] name_filter Only tests whose fully qualified name contains this are included; empty includes all.
   * @param[in] group_filter Only tests whose group equals this are included; empty includes all.
   * @param[out] out_count How many tests the generated harness runs.
   */
  auto _GenerateTestHarness(
    ModuleTree &tree,
    StrView name_filter,
    StrView group_filter,
    std::size_t &out_count)
    -> Str;

  static auto _MoveScopeManagerToNs(
    analyse::scopes::ScopeManager *sm,
    Module const &mod)
    -> void;
};
