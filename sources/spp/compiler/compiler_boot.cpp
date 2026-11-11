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
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.string_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.compiler.cfg;
import spp.compiler.compiler_link;
import spp.compiler.module_tree;
import spp.compiler.prelude;
import spp.lex.lexer;
import spp.lsp.resolution_index;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.env;
import spp.utils.error_formatter;
import spp.utils.files;
import spp.utils.ice;
import spp.utils.stats;
import genex;
import llvm;

SPP_MOD_BEGIN
/// [CHECKED]
auto spp::compiler::CompilerBoot::Lex(
  utils::ProgressBar &bar, ModuleTree &tree) -> void {
  // Lex each module, assigning the tokens into the module. Also generate an
  // error formatter now that the tokens are present. Update progress bars.
  for (auto const &mod : tree) {
    auto lexer = lex::Lexer(mod->Code, compiler::PreludeFor(mod->NsParts));
    mod->Tokens = lexer.Lex();
    mod->Formatter = utils::errors::ErrorFormatter::ForFile(
      mod->Tokens, utils::files::DisplayString(mod->Path),
      lexer.PreludeTokenIndex());
    bar.Next();
  }
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Parse(
  utils::ProgressBar &bar, ModuleTree &tree) -> void {
  // Parse each module's tokens, and remove any asts that have "!cfg" mismatches
  // with the system info.
  for (auto const &mod : tree) {
    if (mod->IsTestHarness) { continue; }
    mod->ModuleAst = parse::ParserSpp(mod->Tokens, mod->Formatter).Parse();

    // What "!cfg" leaves out of this build goes before any scope is made, so it
    // never names anything; an editor greys it out.
    for (auto const &[start, end] : cfg::ApplyCfg(*mod->ModuleAst, Mode)) {
      lsp::resolution_index::RecordInactive(*mod->Formatter, start, end);
    }

    // Save the module ast into the list of modules, which every future stage
    // then iterates over (the module ast is the entry point for analysis).
    _Modules.emplace_back(mod->ModuleAst.get());
    bar.Next();
  }

  // Handle test harness setup if required, generating the hardness runner. This
  // is the harness entry point, so needs a lex and parse after being created.
  for (auto const &mod : tree) {
    if (not mod->IsTestHarness) { continue; }
    mod->Code = _GenerateTestHarness(tree, TestNameFilter, TestGroupFilter);
    auto lexer = lex::Lexer(mod->Code, compiler::PreludeFor(mod->NsParts));
    mod->Tokens = lexer.Lex();
    mod->Formatter = utils::errors::ErrorFormatter::ForFile(
      mod->Tokens, utils::files::DisplayString(mod->Path),
      lexer.PreludeTokenIndex());
    mod->ModuleAst = parse::ParserSpp(mod->Tokens, mod->Formatter).Parse();
    _Modules.emplace_back(mod->ModuleAst.get());
    bar.Next();
  }
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage1_PreProcess(
  utils::ProgressBar &bar, ModuleTree &tree, asts::Ast *ctx) -> void {
  // Run the pre-processing steps for each module in the tree. This will edit
  // the ast tree particularly around functions.
  for (auto const &mod : _Modules) {
    auto const &mod_in_tree = *genex::find_if(
      tree, [&](auto &m) { return m->ModuleAst.get() == mod; });
    mod->FilePath = mod_in_tree->Path;
    mod->Owner = mod_in_tree->Owner;
    mod->Stage1_PreProcess(ctx);
    bar.Next();
  }
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage2_GenTopLvlScopes(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the top-level scope generation stage. This generates type scopes for
  // classes, function scopes, superimposition scopes, etc. Works with the scope
  // manager and scope tree extensively.
  asts::TypeIdentifierAst::ClearPendingVisibility();
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kGenTopLvlScopes, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage2_GenTopLvlScopes(sm, &meta);
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage3_GenTopLvlAliases(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the top-level alias scope generation stage. This generates type scopes
  // for aliased types, which is only possible now that all the concrete types
  // are ready. Separate to stage 2 because of walk structure (not query).
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kGenTopLvlAliases, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage3_GenTopLvlAliases(sm, &meta);
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage4_ResolveDeclarations(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the resolve declarations stage. This does symbol stamping prior to sup
  // scope checks, which are inter-module.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kResolveDeclarations, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage4_ResolveDeclarations(sm, &meta);
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage5_LoadSupScopes(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the load super scopes stage. This files the sup blocks and marks the
  // non-generic type they are targeting.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kLoadSupScopes, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage5_LoadSupScopes(sm, &meta);
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage5_5_AttachSupScopes(
  utils::ProgressBar &bar, analyse::scopes::ScopeManager *sm) -> void {
  // Attach all super scopes now. One pass over the whole scope tree rather than
  // a walk over the modules, so there is no per-module progress to report, only
  // the whole thing being done.
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = asts::meta::CompilerStage::kAttachSupScopes;
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kAttachSupScopes);
  analyse::utils::sup_attachment::AttachAllSuperScopes(*sm, &meta);
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage6_PreAnalyseSemantics(
  utils::ProgressBar &bar,
  ModuleTree &tree,
  analyse::scopes::ScopeManager *sm)
  -> void {
  // Empty a static list for any previous compiled in the same process. No work
  // is done here.
  asts::FunctionPrototypeAst::ClearPendingDefaults();

  // Every extension block's members are checked against its super class before
  // anything is pre-analysed, so a bad override is reported where it is
  // written, rather than as an abstract type use wherever the type is first
  // named.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kPreAnalyseSemantics, nullptr,
    [&](const auto i, auto &meta) {
      _Modules[i]->Impl->CheckExtensionMembers(sm, &meta);
    });

  // Every class's visibility is on its symbol now, so the names read before it
  // was (signatures, attributes) are checked, as the rest are where they are
  // read. Todo: Is this a workaround? Does the visibility engine need fixing?
  asts::TypeIdentifierAst::CheckPendingVisibility(sm);

  // Run the pre-analyse semantics stage. This unstamps some types that need re-
  // analysing from stage 7, unlocking new analysis features, doing "Self" type
  // substitution, and anything else needed for inner-function-scope
  // inter-module analysis from stage 7.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kPreAnalyseSemantics, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage6_PreAnalyseSemantics(sm, &meta);
    });

  // Fixed point queue-drain for optional parameters that have to be checked
  // before stage 7, but need all of stage 6 to have been finished too.
  asts::FunctionPrototypeAst::AnalysePendingDefaults(sm);
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage7_AnalyseSemantics(
  utils::ProgressBar &bar, ModuleTree &tree, const bool is_exe,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the analyse semantics stage. This is almost all semantic analysis in
  // S++, particularly within functions (where code is actually running).
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kAnalyseSemantics, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage7_AnalyseSemantics(sm, &meta);
    });
  bar.Finish();

  // For exe runs, we need to validate the entry point too. This checks for
  // existence, if it can be called, etc.
  if (is_exe) { _ValidateEntryPoint(sm); }
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage8_CheckMemory(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the check memory stage. This enforces all memory rules, and some
  // mutability rules that stage 7 couldn't cover.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kCheckMemory, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage8_CheckMemory(sm, &meta);
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage9_5_Monomorphise(
  utils::ProgressBar &bar, ModuleTree &,
  analyse::scopes::ScopeManager *sm) -> void {
  // Run the monomorphisation engine. This is not a walk over the modules, it's
  // fixed point, so there is no per-module progress to report, only the whole
  // thing being done.
  auto meta = asts::meta::CompilerMetaData();
  meta.CurrentStage = asts::meta::CompilerStage::kMonomorphise;
  analyse::utils::monomorphization::StartInstantiatingOnRead(
    sm->Tree(), asts::meta::CompilerStage::kMonomorphise);
  analyse::utils::monomorphization::MonomorphiseToFixedPoint(
    sm, &meta);
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage10_PreCodeGen(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm) -> void {
  // An instantiation made from here on would never be generated, so lookups
  // stop making them.
  analyse::utils::monomorphization::StopInstantiatingOnRead();

  // Give every module a context to build into, all in this compile's LLVM
  // context.
  _LlvmContext = MakeUnique<llvm::LLVMContext>();
  for (auto const &mod : _Modules) {
    auto ctx = codegen::LlvmCtx::NewCtx(
      utils::files::NativeString(mod->FilePath), *_LlvmContext);
    ctx->Sm = sm;
    _LlvmCtxs.emplace_back(std::move(ctx));
  }

  // Run the pre-code generation stage. This creates function declarations (not
  // definitions), cmp constant declarations, coroutine lowering etc, which all
  // has to be done before the main codegen starts.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kPreCodeGen, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage10_PreCodeGen(sm, &meta, _LlvmCtxs[i].get());
    });
  bar.Finish();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::Stage11_CodeGen(
  utils::ProgressBar &bar, ModuleTree &tree,
  analyse::scopes::ScopeManager *sm, const unsigned opt_level) -> bool {
  // Code generation stage. Run the LLVM code generation steps for every ast in
  // every module. This will produce the non-linked IR code.
  _EachModule(
    tree, sm, asts::meta::CompilerStage::kCodeGen, &bar,
    [&](const auto i, auto &meta) {
      _Modules[i]->Stage11_CodeGen(sm, &meta, _LlvmCtxs[i].get());
    });
  bar.Finish();
  utils::stats::BeginPhase("verify-and-lower-coroutines");

  // Write the llvm modules to file. The unit tests only verify IR, not linking;
  // the unit tests' parallel processes share one project directory, and nothing
  // reads the files back.
  const auto &out = tree.Out();
  if (not VerifyOnly) {
    std::filesystem::create_directories(out.LlvmRoot());
    std::cout << "Writing LLVM IR to: " << out.LlvmRoot() << '\n';
  }

  // Every module is verified before any of them stops the run, so one bad
  // module does not hide the rest.
  auto invalid_modules = Vec<Str>();
  for (
    auto const &[mod, ctx] :
    genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
    // Verified before anything transforms it, so a complaint here is about what
    // codegen produced rather than about what a pass made of it.
    const auto verify = [&](Str const &when) {
      auto details = Str();
      auto stream = llvm::raw_string_ostream(details);
      if (not llvm::verifyModule(*ctx->Module, &stream)) { return true; }
      invalid_modules.emplace_back(
        ctx->Module->getName().str() + when + ":\n" + details);
      return false;
    };

    // Turn the "cold_path" markers into branch weights over verified codegen,
    // while each is still in the block it was emitted into. Nothing else runs
    // per module: coroutines are lowered with the combined module, where a
    // coroutine from another file can be inlined and its frame elided.
    if (verify("")) {
      codegen::ApplyColdPathHints(ctx->Module.get());
    }

    // Written last: the module as codegen left it, hints applied. It is
    // linked, lowered and optimised with the rest ("lto.ll" holds the result).
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

  // Report on all the modules now that they have been built. All invalid
  // modules are reported together: code generation made invalid IR, which is a
  // compiler bug.
  if (not invalid_modules.empty()) {
    auto report = std::to_string(invalid_modules.size())
      + " invalid module(s) from code generation:";
    for (auto const &module : invalid_modules) { report += "\n" + module; }
    Ice(report);
  }

  // Link every module together now that all of them have been built. This is
  // the LTO IR, which itself is not fully complete, as the memory/binary
  // hardness features still need stacking on it.
  return LinkTimeOptimize(
    _LlvmCtxs, _EntryPointLlvmName(), out, opt_level, VerifyOnly);
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::_ValidateEntryPoint(
  analyse::scopes::ScopeManager *sm) -> void {
  // Check whether the "main" function exists with the correct signature:
  // `fun main()`. Get the scope of the "main" module.
  auto main_scope = static_cast<analyse::scopes::Scope*>(nullptr);
  for (auto const &top_level_child : sm->GlobalScope()->Children) {
    const auto n = get_if<analyse::scopes::ScopeIdentifierName>(
      &top_level_child->Name);
    if (n and n->Name->Val == "main") {
      main_scope = top_level_child.get();
      break;
    }
  }
  sm->RestartWalk(main_scope);

  try {
    // Create and analyse the "main()" call inside the scope for main.spp - this
    // will ensure that the symbol is both discoverable and callable correctly.
    const auto main_call = INJECT_CODE("main()", ParseExpression);
    auto meta = asts::meta::CompilerMetaData();
    meta.CurrentStage = asts::meta::CompilerStage::kAnalyseSemantics;
    main_call->Stage7_AnalyseSemantics(sm, &meta);

    // Remember what the call resolved to. It is the one function the program is
    // entered through, and so the one thing link-time internalizing has to keep
    // externally visible - everything else is only ever reached from inside,
    // whatever its declared visibility says. Todo: Until we do exporting?
    if (
      const auto pf = main_call->To<asts::PostfixExpressionAst>();
      pf != nullptr) {
      if (const auto call = pf->Op->To<
        asts::PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
        _EntryPoint = call->Target();
      }
    }
  }

  // Check that the "main" function exists by the identifier "main". Wrap the
  // error into a special one for the entry function.
  catch (analyse::errors::SppIdentifierUnknownError const &) {
    Raise<analyse::errors::SppMissingMainFunctionError>(ERR_ARGS());
  }

  // Check that if the "main" function exists, the signature is compatible. Wrap
  // the error into a special one for the entry function.
  catch (analyse::errors::SppFunctionCallNoValidSignaturesError const &) {
    Raise<analyse::errors::SppMissingMainFunctionError>(ERR_ARGS());
  }

  sm->RestartWalk();
}

/// [CHECKED]
auto spp::compiler::CompilerBoot::_EntryPointLlvmName() const -> Str {
  // The entry point must have been discovered (ie from "main"), but will only
  // be present for an executable. Get the LLVM function and get that function's
  // name.
  if (_EntryPoint == nullptr) { return {}; }
  const auto llvm_func = _EntryPoint->GetLlvmFn();
  if (llvm_func == nullptr or llvm_func->Target == nullptr) { return {}; }
  return llvm_func->Target->getName().str();
}

auto spp::compiler::CompilerBoot::_GenerateTestHarness(
  ModuleTree &tree, const StrView name_filter,
  const StrView group_filter) -> Str {
  // Search the annotations of a function prototype for a "test" annotation,
  // indicating a S++ unit test function.
  // Todo: This will need to change for custom annotations.
  const auto test_annotation_of = [](
    asts::FunctionPrototypeAst const *fun) -> asts::AnnotationAst* {
    for (auto const &a : fun->Annotations) {
      const auto written = a->Name->ToString();
      if (written == "test" or written.ends_with("::test")) { return a.get(); }
    }
    return nullptr;
  };

  // A group is only read off the keyword form, which mirrors how "!ffi" reads
  // its symbol. An unmarked test belongs to the group its module names.
  const auto group_of = [](
    asts::AnnotationAst const *annotation, Str const &fallback) {
    // Get the "group" argument from the annotation's function argument for the
    // "group" parameter.
    if (annotation->FnArgGroup == nullptr) { return fallback; }
    const auto arg = annotation->FnArgGroup->At("group");
    if (arg == nullptr) { return fallback; }

    // Parse the argument as a string literal, and extract the C++ string value
    // and return it.
    const auto literal = arg->Val->To<asts::StringLiteralAst>();
    if (literal == nullptr) { return fallback; }
    const auto written = literal->CppVal();
    return written.empty() ? fallback : written;
  };

  const auto has_name = not name_filter.empty();
  const auto has_group = not group_filter.empty();
  auto body = Str();
  TestNames.clear();
  for (auto const &mod : tree) {
    if (mod->IsTestHarness or mod->ModuleAst == nullptr) { continue; }

    auto const &ns_parts = mod->NsParts;
    auto ns = Str();
    for (auto const &part : ns_parts) { ns += part + "::"; }
    const auto module_group = ns_parts.empty() ? Str() : ns_parts.back();

    for (const auto member : asts::AstBody(mod->ModuleAst.get())) {
      const auto fun = member->To<asts::FunctionPrototypeAst>();
      if (fun == nullptr) { continue; }
      const auto annotation = test_annotation_of(fun);
      if (annotation == nullptr) { continue; }

      // Apply the name and group filter, skipping the S++ test if the filters
      // don't match.
      const auto fq_name = ns + fun->Name->Val;
      const auto group = group_of(annotation, module_group);
      if (has_name and not fq_name.contains(name_filter)) { continue; }
      if (has_group and group != group_filter) { continue; }

      // Create the S++ code to run the test, with printing wrappers. This forms
      // the runner script that S++ executes.
      const auto len = std::to_string(fq_name.length());
      body += "  case run_all or (only.len() == " + len + "_uz and only.ends_with(\"" + fq_name + "\")) {\n";
      body += "    std::console::println(\"[RUN  ] " + group + " :: " + fq_name + "\")\n";
      body += "    " + fq_name + "()\n";
      body += "    std::console::println(\"[  OK ] " + group + " :: " + fq_name + "\")\n";
      body += "  }\n";
      TestNames.emplace_back(fq_name);
    }
  }

  // Boilerplate code that then runs all the tests using the wrapper code from
  // above.
  return Str("fun main() -> Void {\n")
    + "  let only = std::process::get_env(\"" + Str(utils::env::NameOf(utils::env::Var::TestOnly))
    + "\").unwrap_or(Str::from(\"\"))\n"
    + "  let run_all = only.len() == 0_uz\n"
    + body
    + "  std::console::println(\"[ DONE ]\")\n"
    + "  std::mem::ops::drop(only)}\n";
}

auto spp::compiler::CompilerBoot::_MoveScopeManagerToNs(
  analyse::scopes::ScopeManager *sm, Module const &mod) -> void {
  // Get the namespace parts of the module and iterate them, moving to the most
  // internal part of the namespace.
  auto const &mod_ns = mod.NsParts;
  for (auto const &part : mod_ns) {
    const auto is_top_level = sm->CurrentScope->Parent == nullptr;

    // Convert the string part into an "IdentifierAst" node. All namespace
    // scopes use identifier asts for their name.
    auto identifier_part = MakeShared<asts::IdentifierAst>(0uz, Str(part));

    // If the part exists in the current scope (starting from the global scope),
    // then move into it.
    const auto quick_ns_sym = sm->CurrentScope->FindNsSymbol(
      identifier_part.get(), true);

    if (quick_ns_sym != nullptr) {
      const auto ns_scope = quick_ns_sym->LinkedScope;
      sm->RestartWalk(ns_scope);
    }

    // Otherwise, create a new scope and move into it.
    else {
      const auto ns_sym = MakeShared<analyse::scopes::NamespaceSymbol>(
        identifier_part, nullptr);

      sm->CurrentScope->AddNsSymbol(ns_sym);
      const auto ns_scope_name = analyse::scopes::ScopeIdentifierName(
        identifier_part);

      const auto ns_scope = sm->CreateAndMoveIntoNewScope(
        ns_scope_name, nullptr, mod.Formatter.get());

      ns_sym->LinkedScope = ns_scope;
      ns_sym->LinkedScope->LinkedNamespaceSymbol = ns_sym;
      ns_sym->LinkedScope->AstNode = mod.ModuleAst.get();
      if (is_top_level) {
        ns_sym->Owner = mod.Owner;
        ns_sym->Declares = mod.Declares;
      }
    }

    // A package's stubs mark its namespace as theirs, whichever module made it.
    if (is_top_level and mod.IsFfi) {
      sm->CurrentScope->LinkedNamespaceSymbol->IsFfiPackage = true;
      sm->CurrentScope->LinkedNamespaceSymbol->Owner = mod.Owner;
      sm->CurrentScope->LinkedNamespaceSymbol->Declares = mod.Declares;
    }
  }
}

auto spp::compiler::CompilerBoot::_EachModule(
  ModuleTree &tree, analyse::scopes::ScopeManager *sm,
  const asts::meta::CompilerStage stage, utils::ProgressBar *bar,
  const FuncRef<void(std::size_t, asts::meta::CompilerMetaData &)> body)
  -> void {
  // An analysis stage reads an instantiation as a lookup needs one, made for
  // that stage.
  using asts::meta::CompilerStage;
  if (
    stage >= CompilerStage::kPreAnalyseSemantics and
    stage <= CompilerStage::kCheckMemory) {
    analyse::utils::monomorphization::StartInstantiatingOnRead(
      sm->Tree(), stage);
  }

  // For each module, add some boilerplate configurations, and then run the body
  // that was passed in.
  for (auto i = 0uz; i < _Modules.size(); ++i) {
    auto const &mod_in_tree = *genex::find_if(
      tree, [&](auto &m) { return m->ModuleAst.get() == _Modules[i]; });
    _MoveScopeManagerToNs(sm, *mod_in_tree);
    auto meta = asts::meta::CompilerMetaData();
    meta.IsTestHarness = mod_in_tree->IsTestHarness;
    meta.CurrentStage = stage;
    body(i, meta);
    sm->RestartWalk();
    if (bar != nullptr) { bar->Next(); }
  }
}

SPP_MOD_END
