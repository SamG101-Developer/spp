module;
#include <spp/macros.hpp>
#include <spp/version.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/codegen/llvm_pipeline.hpp>
#include <spp/compiler/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.compiler.compiler_boot;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.sup_attachment;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.string_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_artefact;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_entry;
import spp.codegen.llvm_harden_codegen;
import spp.codegen.llvm_harden_memory;
import spp.codegen.llvm_target;
import spp.compiler.cfg;
import spp.compiler.ffi;
import spp.compiler.module_tree;
import spp.lex.lexer;
import spp.lsp.resolution_index;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import spp.utils.features;
import spp.utils.files;
import spp.utils.ice;
import genex;
import llvm;

#define PREP_SCOPE_MANAGER \
  auto const &mod_in_tree = *genex::find_if(tree, [&](auto &m) { return m->ModuleAst.get() == mod; })

#define PREP_SCOPE_MANAGER_AND_META(s)                                  \
  PREP_SCOPE_MANAGER;                                                   \
  spp::compiler::CompilerBoot::_MoveScopeManagerToNs(sm, *mod_in_tree); \
  auto meta = spp::asts::meta::CompilerMetaData();                      \
  meta.IsTestHarness = mod_in_tree->IsTestHarness;                    \
  meta.CurrentStage = (s)

namespace {
  /// The command the link runs, the arguments after it: the one
  /// "[target.linkers]" names for the target; else "cc" for the
  /// host, and for another target its GNU cross compiler, or the
  /// newest versioned one. Nothing, reported, when none is on PATH.
  auto LinkerCommand() -> spp::Opt<spp::Str> {
    using namespace spp;
    const auto folder = Str(codegen::TargetFolderName());
    if (auto named = utils::features::Linker(folder); named.has_value()) { return named; }
    if (codegen::TargetIsHost()) { return Str("cc"); }

    const auto gcc = Str(codegen::TargetGnuTriple()) + "-gcc";
    if (utils::files::FindProgram(gcc).has_value()) { return gcc; }
    if (const auto versioned = utils::files::FindNewestVersion(gcc); versioned.has_value()) {
      return utils::files::NativeString(versioned->filename());
    }
    std::cerr
      << "Error: no linker for " << folder << ": neither '" << gcc << "' nor '" << gcc << "-N' is on PATH.\n"
      << "  Install the target's cross compiler (on Debian or Ubuntu, 'gcc-" << codegen::TargetGnuTriple()
      << "'), or name one under [target.linkers] in spp.toml, as " << folder << " = \"...\".\n";
    return Nullopt;
  }
}

SPP_MOD_BEGIN
auto spp::compiler::CompilerBoot::Lex(
  utils::ProgressBar &bar,
  ModuleTree &tree)
  -> void {
  // Lexing stage.
  for (auto const &mod : tree) {
    auto lexer = lex::Lexer(mod->Code, not utils::files::NativeString(mod->Path).contains("/src/std/"));
    mod->Tokens = lexer.Lex();
    mod->Formatter = utils::errors::ErrorFormatter::ForFile(
      mod->Tokens, utils::files::DisplayString(mod->Path), lexer.PreludeTokenIndex());
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
    if (mod->IsTestHarness) { continue; }
    mod->ModuleAst = parse::ParserSpp(mod->Tokens, mod->Formatter).Parse();

    // What "!cfg" leaves out of this build goes before any scope
    // is made, so it never names anything; an editor greys it out.
    for (auto const &[start, end] : cfg::ApplyCfg(*mod->ModuleAst, BuildMode)) {
      lsp::resolution_index::RecordInactive(*mod->Formatter, start, end);
    }
    _Modules.emplace_back(mod->ModuleAst.get());
    bar.Next();
  }

  for (auto const &mod : tree) {
    if (not mod->IsTestHarness) { continue; }
    mod->Code = _GenerateTestHarness(tree, TestNameFilter, TestGroupFilter, TestCount);
    auto lexer = lex::Lexer(mod->Code, true);
    mod->Tokens = lexer.Lex();
    mod->Formatter = utils::errors::ErrorFormatter::ForFile(
      mod->Tokens, utils::files::DisplayString(mod->Path), lexer.PreludeTokenIndex());
    mod->ModuleAst = parse::ParserSpp(mod->Tokens, mod->Formatter).Parse();
    _Modules.emplace_back(mod->ModuleAst.get());
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
    mod->FilePath = mod_in_tree->Path;
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
    sm->RestartWalk();
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
    sm->RestartWalk();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage4_ResolveDeclarations(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Qualify types stage.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kResolveDeclarations);
    mod->Stage4_ResolveDeclarations(sm, &meta);
    sm->RestartWalk();
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
    sm->RestartWalk();
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
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kAttachSupScopes);
  analyse::utils::sup_attachment::AttachAllSuperScopes(*sm, &meta);
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage6_PreAnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Pre-analyse semantics stage.
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kPreAnalyseSemantics);
  asts::FunctionPrototypeAst::ClearPendingDefaults();

  // Every extension block's members are checked against its
  // super class before anything is pre-analysed, so a bad
  // override is reported where it is written, rather than
  // as an abstract type use wherever the type is first named.
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kPreAnalyseSemantics);
    mod->Impl->CheckExtensionMembers(sm, &meta);
    sm->RestartWalk();
  }

  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kPreAnalyseSemantics);
    mod->Stage6_PreAnalyseSemantics(sm, &meta);
    sm->RestartWalk();
    bar.Next();
  }
  asts::FunctionPrototypeAst::AnalysePendingDefaults(sm);
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage7_AnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  const bool is_exe,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Analyse semantics stage.
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kAnalyseSemantics);
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kAnalyseSemantics);
    mod->Stage7_AnalyseSemantics(sm, &meta);
    sm->RestartWalk();
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
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kCheckMemory);
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCheckMemory);
    mod->Stage8_CheckMemory(sm, &meta);
    sm->RestartWalk();
    bar.Next();
  }
  bar.Finish();

  // Give every module a context to build into, all in this
  // compile's LLVM context.
  _LlvmContext = MakeUnique<llvm::LLVMContext>();
  for (auto const &mod : _Modules) {
    auto ctx = codegen::LlvmCtx::NewCtx(mod->FilePath, *_LlvmContext);
    ctx->Sm = sm;
    _LlvmCtxs.emplace_back(std::move(ctx));
  }
}

auto spp::compiler::CompilerBoot::Stage9_CompTimeResolve(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Comptime resolution stage.
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kCompTimeResolve);
  for (auto const &mod : _Modules) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCompTimeResolve);
    mod->Stage9_CompTimeResolve(sm, &meta);
    sm->RestartWalk();
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
  using namespace analyse::utils;

  // Monomorphisation stage. Not a walk over the modules -
  // see "MonomorphiseToFixedPoint" - so there is no per-module
  // progress to report, only the whole thing being done.
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = asts::meta::CompilerStage::kMonomorphise;
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kMonomorphise);
  monomorphization::MonomorphiseToFixedPoint(sm, &meta);
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage10_PreCodeGen(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // An instantiation made from here on would never be generated, so lookups stop making them.
  analyse::utils::monomorphization::StopInstantiatingOnRead();

  // Code generation stage.
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kPreCodeGen);
    mod->Stage10_PreCodeGen(sm, &meta, ctx);
    sm->RestartWalk();
    bar.Next();
  }
  bar.Finish();
}

auto spp::compiler::CompilerBoot::Stage11_CodeGen(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm,
  const unsigned opt_level)
  -> bool {
  // Code generation stage.
  for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    PREP_SCOPE_MANAGER_AND_META(asts::meta::CompilerStage::kCodeGen);
    mod->Stage11_CodeGen(sm, &meta, ctx);
    sm->RestartWalk();
    bar.Next();
  }
  bar.Finish();

  // Write the llvm modules to file. The unit tests only verify:
  // their parallel processes share one project directory, and
  // nothing reads the files back.
  const auto &out = tree.Out();
  if (not VerifyOnly) {
    std::filesystem::create_directories(out.LlvmRoot());
    std::cout << "Writing LLVM IR to: " << out.LlvmRoot() << std::endl;
  }

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
      auto details = Str();
      auto stream = llvm::raw_string_ostream(details);
      if (not llvm::verifyModule(*ctx->Module, &stream)) { return true; }
      invalid_modules.emplace_back(ctx->Module->getName().str() + when + ":\n" + details);
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
    if (VerifyOnly) { continue; }
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
  // All invalid modules are reported together: code generation
  // made invalid IR, which is a compiler bug.
  if (not invalid_modules.empty()) {
    auto report = std::to_string(invalid_modules.size()) + " invalid module(s) from code generation:";
    for (auto const &module : invalid_modules) { report += "\n" + module; }
    Ice(report);
  }

  // Link every module together now that all of them have been
  // built.
  return _LinkTimeOptimize(out, opt_level);
}

auto spp::compiler::CompilerBoot::_EntryPointLlvmName() const -> Str {
  if (_EntryPoint == nullptr) { return {}; }
  const auto llvm_func = _EntryPoint->GetLlvmFn();
  if (llvm_func == nullptr or llvm_func->Target == nullptr) { return {}; }
  return llvm_func->Target->getName().str();
}

auto spp::compiler::CompilerBoot::_LinkTimeOptimize(
  OutLayout const &out, const unsigned opt_level) -> bool {
  // Guard.
  if (_LlvmCtxs.empty()) { return true; }

  const auto lto_module = MakeUnique<llvm::Module>("spp.lto", *_LlvmCtxs[0]->Context);
  codegen::ApplyTargetToModule(lto_module.get());

  // Link every module into the special lto "root" module. This
  // creates one meta-module containing all the definitions.
  for (auto const &ctx : _LlvmCtxs | genex::views::ptr) {
    if (codegen::LinkIntoLtoModule(lto_module.get(), ctx->Module.get())) { continue; }
    Ice("could not link module " + ctx->Module->getName().str() + " into the lto module");
  }

  // Do a final pass on the lto module before internalizing. This
  // checks for errors in the combined module.
  auto lto_details = Str();
  auto lto_stream = llvm::raw_string_ostream(lto_details);
  if (llvm::verifyModule(*lto_module, &lto_stream)) { Ice("invalid lto module:\n" + lto_details); }

  // The unit tests stop here: the combined module is valid, and
  // optimising, emitting and linking it would add nothing more.
  if (VerifyOnly) { return true; }

  // Internalize all the definitions in the lto module before
  // optimizing. The C entry point is added before internalizing,
  // so that it is the thing being kept and the s++ one it calls is
  // free to be internalized into it along with everything else.
  const auto entry = _EntryPointLlvmName();
  const auto has_entry_point = not entry.empty();
  if (has_entry_point) {
    codegen::EmitCEntryPoint(
      lto_module.get(), entry.c_str(),
      utils::features::Enabled(utils::features::ConfigKey::MemoryStackSplit),
      utils::features::Applied(utils::features::ConfigKey::MemoryMapDenyWx, codegen::TargetIsLinux()),
      utils::features::Applied(utils::features::ConfigKey::MemoryMapSeal, codegen::TargetIsLinux()));
    constexpr auto preserved = "main";
    codegen::RunInternalizePass(lto_module.get(), &preserved, 1);
  }

  codegen::RunOptimizationPipeline(lto_module.get(), opt_level);

  auto ec = std::error_code();
  auto ir_out = llvm::raw_fd_ostream(
    utils::files::NativeString(out.LtoIrFile()), ec,
    static_cast<llvm::sys::fs::OpenFlags>(0));
  lto_module->print(ir_out, nullptr);
  ir_out.flush();

  // Only a program gets built into something runnable. A library
  // has no entry point, so there is nothing for a linker to make an
  // executable out of, and the ir is the whole of what it produces.
  if (not has_entry_point) { return true; }
  const auto object_file = out.ObjectFile();
  FEATURE_GATE(MemoryStackProtect) { codegen::ApplyStackProtector(lto_module.get()); }
  FEATURE_GATE(MemoryStackProbe) { codegen::ApplyStackClashProtection(lto_module.get()); }
  FEATURE_GATE(MemoryStackSplit) { codegen::ApplySafeStack(lto_module.get()); }
  FEATURE_GATE(BinaryCodegenSls) { if (not codegen::ApplySlsHardening(lto_module.get())) { return false; } }
  FEATURE_GATE(BinaryCodegenScrub) { if (not codegen::ApplyRegisterScrubbing(lto_module.get())) { return false; } }
  FEATURE_GATE(BinaryCodegenCfi) { if (not codegen::ApplyCfi(lto_module.get())) { return false; } }
  FEATURE_GATE(BinaryCodegenJumpTableBounds) {
    if (not codegen::ApplyJumpTableBounds(lto_module.get())) { return false; }
  }
  codegen::ApplyFfiUnwindGuard(lto_module.get());
  codegen::ApplyUnwindTables(lto_module.get());

  // What the program was built with, for "spp audit" to read
  // back out of it: every on/off key, set or defaulted.
  const auto record = std::format(
    "spp {}\ntarget {}\nmode {}\n{}", SPP_VERSION, codegen::TargetFolderName(), out.Mode,
    utils::features::EffectiveSettings(codegen::TargetIsLinux()));
  codegen::EmitHardeningRecord(lto_module.get(), record.c_str());
  if (not codegen::EmitObjectFile(lto_module.get(), utils::files::NativeString(object_file).c_str())) { return false; }

  // A cross build names its object first, as "spp build --target"
  // has always printed it. Only a Linux target is linked: the link
  // below asks an ELF linker for ELF hardening.
  if (not codegen::TargetIsHost()) {
    std::cout
      << "Built object for " << codegen::TargetFolderName() << ": "
      << utils::files::DisplayString(object_file) << std::endl;
    if (not codegen::TargetIsLinux()) {
      std::cout << "Not linking: only a Linux target is linked here, and " << codegen::TargetFolderName()
        << " is not one." << std::endl;
      return true;
    }
  }
  return _LinkExecutable(out);
}

auto spp::compiler::CompilerBoot::_LinkExecutable(
  OutLayout const &out) -> bool {
  // The object holds calls into the ffi runtime and nothing
  // else external, so the link is the object plus whatever
  // shared libraries the project's packages ship.
  const auto object_file = out.ObjectFile();
  const auto exe_file = out.ExecutablePath();
  const auto lib_dir = out.LibRoot();
  std::filesystem::create_directories(lib_dir);

  const auto linker = LinkerCommand();
  if (not linker.has_value()) { return false; }
  const auto libraries = ffi::Libraries(out.Root);
  if (not libraries.has_value()) { return false; }

  // musl has no fortified calls ("__memcpy_chk" and friends are
  // glibc's), so a library built against it has none to find.
  FEATURE_GATE(BinaryLinkFortifiedFfi) {
    if (not codegen::TargetIsMusl() and not ffi::CheckFortified(out.Root, *libraries)) { return false; }
  }
  FEATURE_GATE(BinaryLinkRelroFfi) {
    if (not ffi::CheckRelro(out.Root, *libraries)) { return false; }
  }
  FEATURE_GATE(BinaryLinkSegmentsFfi) {
    if (not ffi::CheckSegments(out.Root, *libraries)) { return false; }
  }

  auto command = *linker
    + " -o " + utils::files::NativeString(exe_file)
    + " " + utils::files::NativeString(object_file);
  for (auto const &lib : *libraries) {
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
    command += " -pie"; // No fixed load address to write an exploit against.
    command += " -Wl,-z,relro,-z,now"; // Read-only after startup, and nothing left to bind later.
    command += " -Wl,-z,noexecstack"; // The stack is data; say so in the header rather than by accident.
    command += " -Wl,-z,separate-code"; // Code in its own mapping, so no data shares a page with it.
    command += " -Wl,-z,defs"; // A symbol nothing defines is a link error, not a run-time surprise.
    command += " -Wl,-z,text"; // A text relocation is a link error, so code is never writable at load.
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
    return false;
  }

  // Checked in the output rather than asked of the linker: binutils
  // before 2.39 has no "--error-rwx-segments", and before 2.31
  // ignores "-z separate-code" with a warning.
  FEATURE_GATE(BinaryLinkHarden) {
    const auto fault = codegen::UnhardenedSegments(utils::files::NativeString(exe_file).c_str());
    if (not fault) {
      std::cerr << "Error: cannot read the linked " << exe_file << ": " << fault.error() << "\n";
      return false;
    }
    if (fault->has_value()) {
      std::cerr
        << "Error: " << exe_file << " was linked with " << **fault << ".\n"
        << "  The linker does not separate code from data: use binutils 2.31 or newer (on RHEL 8, "
        << "gcc-toolset-12), or set 'harden = false' under [binary.link] in spp.toml.\n";
      return false;
    }
  }
  std::cout << "Built executable: " << exe_file << std::endl;
  return true;
}

auto spp::compiler::CompilerBoot::_ValidateEntryPoint(
  analyse::scopes::ScopeManager *sm) -> void {
  // Check whether the "main" function exists with the correct
  // signature: `fun main()`.
  const auto main_call = INJECT_CODE("main()", ParseExpression);
  auto main_scope = static_cast<analyse::scopes::Scope*>(nullptr);
  for (auto const &top_level_child : sm->GlobalScope()->Children) {
    const auto n = get_if<analyse::scopes::ScopeIdentifierName>(&top_level_child->Name);
    if (n and n->Name->Val == "main") {
      main_scope = top_level_child.get();
      break;
    }
  }
  sm->RestartWalk(main_scope);

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
    Raise<analyse::errors::SppMissingMainFunctionError>(ERR_ARGS());
  }

  // Check that if the "main" function exists, the signature is compatible.
  catch (analyse::errors::SppFunctionCallNoValidSignaturesError const &) {
    Raise<analyse::errors::SppMissingMainFunctionError>(ERR_ARGS());
  }

  sm->RestartWalk();
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
  TestNames.clear();
  for (auto const &mod : tree) {
    if (mod->IsTestHarness or mod->ModuleAst == nullptr) { continue; }

    auto const &ns_parts = mod->NsParts;
    auto ns = Str();
    for (auto const &part : ns_parts) { ns += part + "::"; }
    const auto module_group = ns_parts.empty() ? Str() : ns_parts.back();

    for (auto *member : asts::AstBody(mod->ModuleAst.get())) {
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
      TestNames.emplace_back(fq_name);
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
  auto const &mod_ns = mod.NsParts;

  // Iterate over the parts of the module namespace.
  for (auto const &part : mod_ns) {
    const auto is_top_level = sm->CurrentScope->Parent == nullptr;
    // Convert the string part into an IdentifierAst node.
    auto identifier_part = MakeShared<asts::IdentifierAst>(0uz, Str(part));

    // If the part exists in the current scope (starting from the global scope), then move into it.
    if (const auto quick_ns_sym = sm->CurrentScope->FindNsSymbol(identifier_part.get(), true);
      quick_ns_sym != nullptr) {
      const auto ns_scope = quick_ns_sym->LinkedScope;
      sm->RestartWalk(ns_scope);
    }

    // Otherwise, create a new scope and move into it.
    else {
      const auto ns_sym = MakeShared<analyse::scopes::NamespaceSymbol>(identifier_part, nullptr);
      sm->CurrentScope->AddNsSymbol(ns_sym);
      const auto ns_scope_name = analyse::scopes::ScopeIdentifierName(identifier_part);
      const auto ns_scope = sm->CreateAndMoveIntoNewScope(ns_scope_name, nullptr, mod.Formatter.get());
      ns_sym->LinkedScope = ns_scope;
      ns_sym->LinkedScope->LinkedNamespaceSymbol = ns_sym;
      ns_sym->LinkedScope->AstNode = mod.ModuleAst.get();
      if (is_top_level) { ns_sym->Owner = mod.Owner; }
    }

    // A package's stubs mark its namespace as theirs, whichever module made it.
    if (is_top_level and mod.IsFfi) {
      sm->CurrentScope->LinkedNamespaceSymbol->IsFfiPackage = true;
      sm->CurrentScope->LinkedNamespaceSymbol->Owner = mod.Owner;
    }
  }
}

SPP_MOD_END
