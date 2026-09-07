module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>
#include <spp/compiler/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.compiler.compiler_boot;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.string_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.compiler.module_tree;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import spp.utils.features;
import spp.utils.files;
import genex;
import llvm;

#define PREP_SCOPE_MANAGER \
  auto const &mod_in_tree = *genex::find_if(tree, [&](auto &m) { return m->module_ast.get() == mod; })

#define PREP_SCOPE_MANAGER_AND_META(s)                                  \
  PREP_SCOPE_MANAGER;                                                   \
  spp::compiler::CompilerBoot::_MoveScopeManagerToNs(sm, *mod_in_tree); \
  auto meta = spp::asts::meta::CompilerMetaData();                      \
  meta.IsTestHarness = mod_in_tree->is_test_harness;                    \
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
  // Parsing stage. Do the filtering for the test hardness
  // modules, depending on the how the compiler has been
  // invoked.
  for (auto const &mod : tree) {
    if (mod->is_test_harness) { continue; }
    mod->module_ast = parse::ParserSpp(mod->tokens, mod->error_formatter).parse();
    _Modules.EmplaceBack(mod->module_ast.get());
    bar.Next();
  }

  for (auto const &mod : tree) {
    if (not mod->is_test_harness) { continue; }
    mod->code = _GenerateTestHarness(tree, TestNameFilter, TestGroupFilter, TestCount);
    mod->tokens = lex::Lexer(mod->code, true).Lex();
    mod->error_formatter = MakeUnique<utils::errors::ErrorFormatter>(
      mod->tokens, utils::files::DisplayString(mod->path));
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kGenTopLvlScopes);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kGenTopLvlAliases);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kQualifyTypes);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kLoadSupScopes);
    mod->Stage5_LoadSupScopes(sm, &meta);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage5_5_AttachSupScopes(
  utils::ProgressBar &bar,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Attach all super scopes now. One pass over the whole
  // scope tree rather than a walk over the modules, so there
  // is no per-module progress to report, only the whole
  // thing being done.
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = asts::meta::CompilerStage::kAttachSupScopes;
  sm->AttachAllSuperScopes(&meta);
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage6_PreAnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Pre-analyse semantics stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kPreAnalyseSemantics);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kAnalyseSemantics);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCheckMemory);
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCompTimeResolve);
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
  meta.CurrentStage = asts::meta::CompilerStage::kMonomorphise;
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
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kPreCodeGen);
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
  const unsigned opt_level)
  -> void {
  // Code generation stage.
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCodeGen);
    meta.LlvmCtx = ctx;
    mod->Stage11_CodeGen(sm, &meta, ctx);
    sm->Reset();
    bar.Next();
  }
  bar.Finish();

  // Write the llvm modules to file.
  const auto &out = tree.Out();
  std::filesystem::create_directories(out.LlvmRoot());
  std::cout << "Writing LLVM IR to: " << out.LlvmRoot() << std::endl;

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
    // Coroutine lowering only: optimizing a module here would be undone by optimizing the combined module below, and
    // every build reaches that now.
    if (verify("")) {
      codegen::RunCoroLoweringPipeline(ctx->Module.get());
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

  // Link every module together now that all of them have been
  // built.
  _LinkTimeOptimize(out, opt_level);
}

auto spp::compiler::CompilerBoot::_EntryPointLlvmName() const
  -> Str {
  if (_EntryPoint == nullptr) { return {}; }
  const auto llvm_func = _EntryPoint->GetLlvmFunc();
  if (llvm_func == nullptr or llvm_func->Target == nullptr) { return {}; }
  return llvm_func->Target->getName().str();
}

auto spp::compiler::CompilerBoot::_LinkTimeOptimize(
  OutLayout const &out,
  const unsigned opt_level)
  -> void {
  // Guard.
  if (_LlvmCtxs.IsEmpty()) { return; }

  const auto lto_module = MakeUnique<llvm::Module>("spp.lto", *_LlvmCtxs[0]->Context);
  codegen::ApplyTargetToModule(lto_module.get());

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
    has_entry_point = codegen::EmitCEntryPoint(
      lto_module.get(), entry.c_str(),
      utils::features::Enabled(utils::features::ConfigKey::MemoryStackSplit));
  }

  if (has_entry_point) {
    constexpr auto preserved = "main";
    codegen::RunInternalizePass(lto_module.get(), &preserved, 1);
  }

  // Before optimizing, not after: a misnamed intrinsic reads as a call to
  // an opaque external, so the passes neither honour it nor keep it up to
  // date as they move code around. Naming a stale lifetime marker back into
  // existence afterwards is worse than never having had it - the backend
  // colours stack slots by them, and reuses a slot that is still live.
  codegen::RunOptimizationPipeline(lto_module.get(), opt_level);

  // And again, for the ones the pipeline introduced itself. These are
  // placed by the pass that built them, so they are correct where they
  // are; only their names are not.

  auto ec = std::error_code();
  auto ir_out = llvm::raw_fd_ostream(
    utils::files::NativeString(out.LtoIrFile()), ec,
    static_cast<llvm::sys::fs::OpenFlags>(0));
  lto_module->print(ir_out, nullptr);
  ir_out.flush();

  // Only a program gets built into something runnable. A library
  // has no entry point, so there is nothing for a linker to make an
  // executable out of, and the ir is the whole of what it produces.
  if (not has_entry_point) { return; }
  const auto object_file = out.ObjectFile();
  FEATURE_GATE(MemoryStackProtect) { codegen::ApplyStackProtector(lto_module.get()); }
  FEATURE_GATE(MemoryStackProbe) { codegen::ApplyStackClashProtection(lto_module.get()); }
  FEATURE_GATE(MemoryStackSplit) { codegen::ApplySafeStack(lto_module.get()); }
  if (not codegen::EmitObjectFile(lto_module.get(), utils::files::NativeString(object_file).c_str())) { return; }

  // A cross build stops at the object, no linking available for
  // now.
  if (not codegen::TargetIsHost()) {
    std::cout
      << "Built object for " << codegen::TargetFolderName() << ": "
      << utils::files::DisplayString(object_file) << "\n"
      << "Not linking: a cross build has no linker or ffi runtime for its target here." << std::endl;
    return;
  }
  _LinkExecutable(out);
}

auto spp::compiler::CompilerBoot::_LinkExecutable(
  OutLayout const &out)
  -> void {
  // The object holds calls into the ffi runtime and nothing
  // else external, so the link is the object plus whatever
  // shared libraries the project's packages ship.
  const auto object_file = out.ObjectFile();
  const auto exe_file = out.ExecutablePath();
  const auto lib_dir = out.LibRoot();
  std::filesystem::create_directories(lib_dir);

  auto command = Str("cc -o ") + utils::files::NativeString(exe_file) + " " + utils::files::NativeString(object_file);
  for (auto const &lib : _FfiLibraries(out.Root)) {
    // Staged beside the executable rather than linked where it
    // sits, so that what the loader needs travels with what
    // was built and the path baked into it is a relative one.
    const auto staged = lib_dir / lib.filename();
    std::filesystem::copy_file(lib, staged, std::filesystem::copy_options::overwrite_existing);
    command += " " + utils::files::NativeString(staged);
  }

  command += " -lm";
  command += " -Wl,-rpath,'$ORIGIN/lib'";

  // Binary hardening, done by attaching flags to the linker.
  FEATURE_GATE(BinaryLinkHarden) {
    command += " -pie";                     // No fixed load address to write an exploit against.
    command += " -Wl,-z,relro,-z,now";      // Read-only after startup, and nothing left to bind later.
    command += " -Wl,-z,noexecstack";       // The stack is data; say so in the header rather than by accident.
    command += " -Wl,-z,separate-code";     // Code in its own mapping, so no data shares a page with it.
    command += " -Wl,-z,defs";              // A symbol nothing defines is a link error, not a run-time surprise.
  }

  // "-z nodlopen" is not here on purpose: it sets a flag on a shared
  // object saying it may not be opened by name, and a linker asked
  // for it while building an executable drops it rather than
  // recording anything. It belongs on the library link, once there
  // is one, and a flag that produces no bit in the artefact is worse
  // than an absent one - the whole point of writing the mitigations
  // down is that a built binary can be audited for what it actually
  // got.

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
  // "<package>/ffi/<name>/lib<name>.so" is what a package ships
  // its native code as, beside the stub that declares it, both
  // for the project itself and for anything it has vendored, so
  // the whole tree is swept for that shape rather than any one
  // place being named.
  auto libraries = Vec<std::filesystem::path>();
  if (not std::filesystem::exists(project_root)) { return libraries; }

  // What counts as a library is the host's own extension, not
  // ".so" everywhere: a package may ship one library per platform
  // beside its stub, and the ones for other hosts are not this
  // link's to make sense of.
  const auto extension = "." + utils::files::SharedLibraryExtension();
  for (auto const &entry : std::filesystem::recursive_directory_iterator(project_root)) {
    if (not entry.is_regular_file()) { continue; }
    if (utils::files::NativeString(entry.path().extension()) != extension) { continue; }
    if (entry.path().parent_path().parent_path().filename() != "ffi") { continue; }

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
    meta.CurrentStage = asts::meta::CompilerStage::kAnalyseSemantics;
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

auto spp::compiler::CompilerBoot::_GenerateTestHarness(
  ModuleTree &tree,
  const StrView name_filter,
  const StrView group_filter,
  std::size_t &out_count)
  -> Str {
  // The annotation is matched on its written name rather than
  // on a resolved symbol, because this runs before anything
  // is analysed - the tests have to be known in order to write
  // the entry point that reaches them. Todo: Make this better.
  const auto test_annotation_of = [](asts::FunctionPrototypeAst const *fun) -> asts::AnnotationAst const* {
    for (auto const &a : fun->Annotations) {
      const auto written = a->Name->ToString();
      if (written == "test" or written.ends_with("::test")) { return a.get(); }
    }
    return nullptr;
  };

  // A group is only read off the keyword form, which mirrors
  // how "!ffi" reads its symbol. An unmarked test belongs to
  // the group its module names.
  const auto group_of = [](asts::AnnotationAst const *annotation, Str const &fallback) -> Str {
    if (annotation->FnArgGroup == nullptr) { return fallback; }
    const auto arg = annotation->FnArgGroup->At("group");
    if (arg == nullptr) { return fallback; }
    const auto literal = arg->Val->To<asts::StringLiteralAst>();
    if (literal == nullptr) { return fallback; }
    const auto written = literal->CppVal();
    return written.empty() ? fallback : written;
  };

  auto body = Str();
  auto count = 0uz;
  TestNames.Clear();
  for (auto const &mod : tree) {
    if (mod->is_test_harness or mod->module_ast == nullptr) { continue; }

    auto const &ns_parts = mod->ns_parts;
    auto ns = Str();
    for (auto const &part : ns_parts) { ns += part + "::"; }
    const auto module_group = ns_parts.IsEmpty() ? Str() : ns_parts.Back();

    for (auto *member : asts::AstBody(mod->module_ast.get())) {
      const auto fun = member->To<asts::FunctionPrototypeAst>();
      if (fun == nullptr) { continue; }
      const auto annotation = test_annotation_of(fun);
      if (annotation == nullptr) { continue; }

      const auto fq_name = ns + fun->Name->Val;
      const auto group = group_of(annotation, module_group);
      if (not name_filter.empty() and not StrView(fq_name).contains(name_filter)) { continue; }
      if (not group_filter.empty() and group != group_filter) { continue; }

      const auto len = std::to_string(fq_name.length());
      body += "  case run_all or (only.len() == " + len + "_uz and only.ends_with(\"" + fq_name + "\")) {\n";
      body += "    std::console::println(\"[RUN  ] " + group + " :: " + fq_name + "\")\n";
      body += "    " + fq_name + "()\n";
      body += "    std::console::println(\"[  OK ] " + group + " :: " + fq_name + "\")\n";
      body += "  }\n";
      TestNames.EmplaceBack(fq_name);
      ++count;
    }
  }

  out_count = count;
  return Str("fun main() -> Void {\n")
    + "  let only = std::process::get_env(\"SPP_TEST_ONLY\").unwrap_or(Str::from(\"\"))\n"
    + "  let run_all = only.len() == 0_uz\n"
    + body
    + "  std::console::println(\"[ DONE ]\")\n"
    + "  std::mem::ops::drop(only)}\n";
}

auto spp::compiler::CompilerBoot::_MoveScopeManagerToNs(
  analyse::scopes::ScopeManager *sm,
  Module const &mod)
  -> void {
  //
  auto const &mod_ns = mod.ns_parts;

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
