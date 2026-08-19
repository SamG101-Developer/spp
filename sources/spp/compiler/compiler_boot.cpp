module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>
#include <spp/parse/macros.hpp>

module spp.compiler.compiler_boot;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import spp.compiler.module_tree;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import spp.utils.files;
import genex;
import llvm;

#define PREP_SCOPE_MANAGER \
  auto const &mod_in_tree = *genex::find_if(tree, [&](auto &m) { return m->module_ast.get() == mod; })

#define PREP_SCOPE_MANAGER_AND_META(s)                                  \
  PREP_SCOPE_MANAGER;                                                   \
  spp::compiler::CompilerBoot::_MoveScopeManagerToNs(sm, *mod_in_tree); \
  auto meta = spp::asts::meta::CompilerMetaData();                      \
  meta.CurrentStage = (s)

SPP_MOD_BEGIN
auto spp::compiler::CompilerBoot::Lex(
  utils::ProgressBar &bar,
  ModuleTree &tree)
  -> void {
  // Lexing stage.
  for (auto const &mod : tree) {
    mod->tokens = lex::Lexer(mod->code, not utils::files::NativeString(mod->path).contains("/src/std/")).Lex();
    mod->error_formatter = MakeUnique<utils::errors::ErrorFormatter>(mod->tokens,
                                                                     utils::files::DisplayString(mod->path));
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Parse(
  utils::ProgressBar &bar,
  ModuleTree &tree)
  -> void {
  // Parsing stage.
  for (auto const &mod : tree) {
    mod->module_ast = parse::ParserSpp(mod->tokens, mod->error_formatter).parse();
    _Modules.EmplaceBack(mod->module_ast.get());
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage1_PreProcess(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  asts::Ast *ctx)
  -> void {
  // Pre-processing stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER;
    mod->FilePath = mod_in_tree->path;
    mod->Stage1_PreProcess(ctx);
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage2_GenTopLvlScopes(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Generate top-level scopes stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(4.0);
    mod->Stage2_GenTopLvlScopes(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage3_GenTopLvlAliases(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Generate top-level aliases stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(5.0);
    mod->Stage3_GenTopLvlAliases(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage4_QualifyTypes(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Qualify types stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(6.0);
    mod->Stage4_QualifyTypes(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage5_LoadSupScopes(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Load super scopes stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(7.0);
    mod->Stage5_LoadSupScopes(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();

  // Attach all super scopes now.
  // Todo: New progress bar here
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = 7.5;
  sm->AttachAllSuperScopes(&meta);
}

auto spp::compiler::CompilerBoot::Stage6_PreAnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Pre-analyse semantics stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(8.0);
    mod->Stage6_PreAnalyseSemantics(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage7_AnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  const bool is_exe,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Analyse semantics stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(9.0);
    mod->Stage7_AnalyseSemantics(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();

  // Validate entry point now.
  if (is_exe) { _ValidateEntryPoint(sm); }
}

auto spp::compiler::CompilerBoot::Stage8_CheckMemory(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Check memory stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(10.0);
    mod->Stage8_CheckMemory(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();

  // Give every module a context to build into.
  for (auto const &mod : _Modules) {
    auto ctx = codegen::LlvmCtx::NewCtx(mod->FilePath);
    ctx->Sm = sm;
    _LlvmCtxs.EmplaceBack(std::move(ctx));
  }
}

auto spp::compiler::CompilerBoot::Stage9_CompTimeResolve(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Comptime resolution stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(11.0);
    mod->Stage9_CompTimeResolve(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage9_5_Monomorphise(
  utils::ProgressBar &bar,
  ModuleTree &,
  analyse::scopes::ScopeManager *sm)
  -> void {
  //
  using analyse::utils::monomorphization_utils::MonomorphiseToFixedPoint;

  // Monomorphisation stage. Not a walk over the modules -
  // see "MonomorphiseToFixedPoint" - so there is no per-module
  // progress to report, only the whole thing being done.
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = 11.5;
  MonomorphiseToFixedPoint(sm, &meta);
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage10_PreCodeGen(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Code generation stage.
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    PREP_SCOPE_MANAGER_AND_META(11.0);
    mod->Stage10_PreCodeGen(sm, &meta, ctx);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage11_CodeGen(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm,
  const bool optimize,
  const bool lto)
  -> void {
  // Code generation stage.
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    PREP_SCOPE_MANAGER_AND_META(12.0);
    meta.LlvmCtx = ctx;
    mod->Stage11_CodeGen(sm, &meta, ctx);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();

  // Write the llvm modules to file.
  const auto out_path = tree.RootPath() / "out" / "llvm";
  std::filesystem::create_directories(out_path);
  std::cout << "Writing LLVM IR to: " << out_path << std::endl;

  // Paired with the modules, because the file each context belongs
  // to comes from the module's own path. Every module is verified
  // before any of them stops the run, so one bad module does not
  // hide the rest.
  auto invalid_modules = Vec<Str>();
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    // Verified before anything transforms it, so a complaint here
    // is about what codegen produced rather than about what a pass
    // made of it.
    const auto verify = [&](Str const &when) {
      if (not llvm::verifyModule(*ctx->Module, &llvm::errs())) { return true; }
      llvm::errs() << "Invalid module" << when << ": " << ctx->Module->getName() << "\n";
      invalid_modules.EmplaceBack(ctx->Module->getName().str());
      return false;
    };

    // Run the coroutine and optimization pipelines over verified
    // codegen.
    if (verify("")) {
      codegen::RunCoroLoweringPipeline(ctx->Module.get());
      if (verify(" after lowering") and optimize and not lto) {
        codegen::RunOptimizationPipeline(ctx->Module.get());
        verify(" after optimization");
      }
    }

    // Written last, so the file on disk is the module as it will
    // actually be built.
    const auto file = tree.LlvmOutPathFor(mod->FilePath);
    std::filesystem::create_directories(file.parent_path());

    auto ec = std::error_code();
    auto out = llvm::raw_fd_ostream(
      utils::files::NativeString(file), ec,
      static_cast<llvm::sys::fs::OpenFlags>(0));
    ctx->Module->print(out, nullptr);
    out.flush();
  }

  // Report on all the modules now that they have been built.
  // All invalid modules are reported before aborting.
  if (not invalid_modules.IsEmpty()) {
    llvm::errs() << invalid_modules.Len() << " invalid module(s):\n";
    for (auto const &name : invalid_modules) { llvm::errs() << "  " << name << "\n"; }
    std::abort();
  }

  // Run the link-time optimization now that all modules have
  // been built. This means we don't have top copy definitions
  // into every module; just declaration stubs.
  if (optimize and lto) {
    _LinkTimeOptimize(out_path);
  }
}

auto spp::compiler::CompilerBoot::ExecutableName(
  std::filesystem::path const &project_root)
  -> Str {
  return utils::files::NativeString(project_root.filename());
}

auto spp::compiler::CompilerBoot::_EntryPointLlvmName() const
  -> Str {
  if (_EntryPoint == nullptr) { return {}; }
  const auto llvm_func = _EntryPoint->GetLlvmFunc();
  if (llvm_func == nullptr or llvm_func->Target == nullptr) { return {}; }
  return llvm_func->Target->getName().str();
}

auto spp::compiler::CompilerBoot::_LinkTimeOptimize(
  std::filesystem::path const &out_path)
  -> void {
  // Guard.
  if (_LlvmCtxs.IsEmpty()) { return; }

  const auto lto_module = MakeUnique<llvm::Module>("spp.lto", *_LlvmCtxs[0]->Context);

  // Link every module into the special lto "root" module. This
  // creates one meta-module containing all the definitions.
  for (auto const &ctx : _LlvmCtxs | genex::views::ptr) {
    if (codegen::LinkIntoLtoModule(lto_module.get(), ctx->Module.get())) { continue; }
    llvm::errs() << "Failed to link module into the lto module: " << ctx->Module->getName() << "\n";
    return;
  }

  // Do a final pass on the lto module before internalizing. This
  // checks for errors in the combined module.
  if (llvm::verifyModule(*lto_module, &llvm::errs())) {
    llvm::errs() << "Invalid lto module\n";
    return;
  }

  // Internalize all the definitions in the lto module before
  // optimizing. The C entry point is added before internalizing,
  // so that it is the thing being kept and the s++ one it calls is
  // free to be internalized into it along with everything else.
  auto has_entry_point = false;
  if (const auto entry = _EntryPointLlvmName(); not entry.empty()) {
    has_entry_point = codegen::EmitCEntryPoint(lto_module.get(), entry.c_str());
  }

  if (has_entry_point) {
    constexpr auto preserved = "main";
    codegen::RunInternalizePass(lto_module.get(), &preserved, 1);
  }

  codegen::RunOptimizationPipeline(lto_module.get());

  // See "ScrubCorruptLifetimeIntrinsics": the names llvm builds
  // for these come out with trailing garbage in this build, and
  // an unrecognised "llvm.*" name is emitted as a call to a
  // symbol nothing defines. Todo: fix this,
  if (const auto scrubbed = codegen::ScrubCorruptLifetimeIntrinsics(lto_module.get()); scrubbed > 0) {
    llvm::errs() << "Warning: dropped " << scrubbed << " call(s) to misnamed lifetime intrinsics\n";
  }

  if (llvm::verifyModule(*lto_module, &llvm::errs())) {
    llvm::errs() << "Invalid lto module after optimization\n";
    return;
  }

  auto ec = std::error_code();
  auto out = llvm::raw_fd_ostream(
    utils::files::NativeString(out_path / "lto.ll"), ec,
    static_cast<llvm::sys::fs::OpenFlags>(0));
  lto_module->print(out, nullptr);
  out.flush();

  // Only a program gets built into something runnable. A library
  // has no entry point, so there is nothing for a linker to make an
  // executable out of, and the ir is the whole of what it produces.
  if (not has_entry_point) { return; }
  const auto object_file = out_path / "spp.o";
  if (not codegen::EmitObjectFile(lto_module.get(), utils::files::NativeString(object_file).c_str())) { return; }
  _LinkExecutable(object_file);
}

auto spp::compiler::CompilerBoot::_LinkExecutable(
  std::filesystem::path const &object_file)
  -> void {
  // The object holds calls into the ffi runtime and nothing
  // else external, so the link is the object plus whatever
  // shared libraries the project's packages ship.
  const auto out_dir = object_file.parent_path().parent_path();
  const auto exe_file = out_dir / CompilerBoot::ExecutableName(out_dir.parent_path());
  const auto lib_dir = out_dir / "lib";
  std::filesystem::create_directories(lib_dir);

  auto command = Str("cc -o ") + utils::files::NativeString(exe_file) + " " + utils::files::NativeString(object_file);
  for (auto const &lib : _FfiLibraries(out_dir.parent_path())) {
    // Staged beside the executable rather than linked where it
    // sits, so that what the loader needs travels with what
    // was built and the path baked into it is a relative one.
    const auto staged = lib_dir / lib.filename();
    std::filesystem::copy_file(lib, staged, std::filesystem::copy_options::overwrite_existing);
    command += " " + utils::files::NativeString(staged);

    // What the loader asks for at run time is the library's so
    // name, not the name of the file it was linked from, and
    // a runtime shipped as "sppc.so" calls itself "libsppc.so".
    const auto name = utils::files::NativeString(staged.filename());
    if (not name.starts_with("lib")) {
      std::filesystem::copy_file(
        staged, lib_dir / ("lib" + name), std::filesystem::copy_options::overwrite_existing);
    }
  }
  command += " -Wl,-rpath,'$ORIGIN/lib'";

  std::cout << "Linking: " << exe_file << std::endl;
  if (const auto status = std::system(command.c_str()); status != 0) {
    llvm::errs() << "Linking failed (" << status << "): " << command << "\n";
    return;
  }
  std::cout << "Built executable: " << exe_file << std::endl;
}

auto spp::compiler::CompilerBoot::_FfiLibraries(
  std::filesystem::path const &project_root)
  -> Vec<std::filesystem::path> {
  // "<package>/ffi/<name>/lib/<name>.so" is what a package ships
  // its native code as, both for the project itself and for
  // anything it has vendored, so the whole tree is swept for
  // that shape rather than any one place being named.
  auto libraries = Vec<std::filesystem::path>();
  if (not std::filesystem::exists(project_root)) { return libraries; }

  for (auto const &entry : std::filesystem::recursive_directory_iterator(project_root)) {
    if (not entry.is_regular_file() or entry.path().extension() != ".so") { continue; }
    const auto lib_dir = entry.path().parent_path();
    if (lib_dir.filename() != "lib" or lib_dir.parent_path().parent_path().filename() != "ffi") { continue; }

    // One name, one library: a package and something it vendored
    // can both ship the same runtime, and linking two copies of
    // it is at best redundant and at worst two sets of its state.
    const auto already_found = genex::any_of(libraries, [&](auto const &found) {
      return found.filename() == entry.path().filename();
    });
    if (not already_found) { libraries.EmplaceBack(entry.path()); }
  }
  return libraries;
}

auto spp::compiler::CompilerBoot::_ValidateEntryPoint(
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Check whether the "main" function exists with the correct
  // signature: `fun main()`.
  const auto main_call = INJECT_CODE("main()", parse_expression);
  auto main_scope = static_cast<analyse::scopes::Scope*>(nullptr);
  for (auto const &top_level_child : sm->GlobalScope->Children) {
    const auto n = std::get_if<analyse::scopes::ScopeIdentifierName>(&top_level_child->Name);
    if (n and n->Name->Val == "main") {
      main_scope = top_level_child.get();
      break;
    }
  }
  sm->Reset(main_scope);

  try {
    auto meta = asts::meta::CompilerMetaData();
    meta.CurrentStage = 9.0;
    main_call->Stage7_AnalyseSemantics(sm, &meta);

    // Remember what the call resolved to. It is the one function the program is entered through, and so the one
    // thing link-time internalizing has to keep externally visible - everything else is only ever reached from
    // inside, whatever its declared visibility says.
    if (const auto pf = main_call->To<asts::PostfixExpressionAst>(); pf != nullptr) {
      if (const auto call = pf->Op->To<asts::PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
        _EntryPoint = call->Target();
      }
    }
  }

  // Check that the "main" function exists,
  catch (analyse::errors::SppIdentifierUnknownError const &) {
    Raise<analyse::errors::SppMissingMainFunctionError>({sm->GlobalScope.get()}, ERR_ARGS());
  }

  // Check that if the "main" function exists, the signature is compatible.
  catch (analyse::errors::SppFunctionCallNoValidSignaturesError const &) {
    Raise<analyse::errors::SppMissingMainFunctionError>({sm->GlobalScope.get()}, ERR_ARGS());
  }

  sm->Reset();
}

auto spp::compiler::CompilerBoot::_MoveScopeManagerToNs(
  analyse::scopes::ScopeManager *sm,
  Module const &mod)
  -> void {
  using namespace std::string_literals;
  // Create the module namespace as a list of strings.
  auto mod_ns = Vec<Str>(mod.path.begin(), mod.path.end());
  if (genex::contains(mod_ns, "src"_str)) {
    const auto src_index = genex::find(mod_ns, "src"_str) - mod_ns.begin() + 1z;
    mod_ns = Vec(mod_ns.begin() + src_index, mod_ns.end());
    mod_ns.Back().erase(mod_ns.Back().length() - 4);
  }
  else {
    mod_ns = Vec{mod_ns[mod_ns.Len() - 2]};
  }

  // Iterate over the parts of the module namespace.
  for (auto const &part : mod_ns) {
    // Convert the string part into an IdentifierAst node.
    auto identifier_part = MakeShared<asts::IdentifierAst>(0uz, Str(part));

    // If the part exists in the current scope (starting from the global scope), then move into it.
    if (const auto quick_ns_sym = sm->CurrentScope->GetNsSymbol(identifier_part.get(), true); quick_ns_sym != nullptr) {
      const auto ns_scope = quick_ns_sym->LinkedScope;
      sm->Reset(ns_scope);
    }

    // Otherwise, create a new scope and move into it.
    else {
      const auto ns_sym = MakeShared<analyse::scopes::NamespaceSymbol>(identifier_part, nullptr);
      sm->CurrentScope->AddNsSymbol(ns_sym);
      const auto ns_scope_name = analyse::scopes::ScopeIdentifierName(identifier_part);
      const auto ns_scope = sm->CreateAndMoveIntoNewScope(ns_scope_name, nullptr, mod.error_formatter.get());
      ns_sym->LinkedScope = ns_scope;
      ns_sym->LinkedScope->NsSym = ns_sym;
      ns_sym->LinkedScope->AstNode = mod.module_ast.get();
    }
  }
}

SPP_MOD_END
