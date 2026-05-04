module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.compiler.compiler_boot;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import spp.compiler.module_tree;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import spp.utils.files;
import llvm;
import genex;

#define PREP_SCOPE_MANAGER \
    auto const &mod_in_tree = *genex::find_if(tree, [&](auto &m) { return m->module_ast.get() == mod; })

#define PREP_SCOPE_MANAGER_AND_META(s)                                    \
    PREP_SCOPE_MANAGER;                                                   \
    spp::compiler::CompilerBoot::_MoveScopeManagerToNs(sm, *mod_in_tree); \
    auto meta = spp::asts::meta::CompilerMetaData();                       \
    meta.CurrentStage = (s)

SPP_MOD_BEGIN
auto spp::compiler::CompilerBoot::Lex(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void {
    // Lexing stage.
    for (auto const &mod : tree) {
        mod->tokens = lex::Lexer(mod->code, not mod->path.string().contains("/src/std/")).lex();
        mod->error_formatter = MakeUnique<utils::errors::ErrorFormatter>(mod->tokens, mod->path.string());
        bar.next();
    }
    bar.finish();
}

auto spp::compiler::CompilerBoot::Parse(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void {
    // Parsing stage.
    for (auto const &mod : tree) {
        mod->module_ast = parse::ParserSpp(mod->tokens, mod->error_formatter).parse();
        _Modules.EmplaceBack(mod->module_ast.get());
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();

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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();

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
        bar.next();
    }
    bar.finish();

    // Attach all LLVM type info to all types now.
    for (auto const &mod : _Modules) {
        auto ctx = codegen::LLvmCtx::NewCtx(mod->FilePath);
        sm->AttachLlvmTypeInfo(*mod, ctx.get());
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
        bar.next();
    }
    bar.finish();
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
        bar.next();
    }
    bar.finish();
}

auto spp::compiler::CompilerBoot::Stage11_CodeGen(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Code generation stage.
    for (auto const &[mod, ctx] : genex::views::zip(_Modules, _LlvmCtxs | genex::views::ptr)) {
        PREP_SCOPE_MANAGER_AND_META(12.0);
        meta.LlvmCtx = ctx;
        mod->Stage11_CodeGen(sm, &meta, ctx);
        sm->Reset();
        bar.next();
    }
    bar.finish();

    // Write the llvm modules to file.
    const auto out_path = tree.RootPath() / "out" / "llvm";
    std::filesystem::create_directories(out_path);
    std::cout << "Writing LLVM IR to: " << out_path << "\n";

    for (auto const &ctx : _LlvmCtxs) {
        // auto structs = ctx->module->getIdentifiedStructTypes();
        // for (auto const &strct : structs) {
        //     llvm::errs() << "Struct : " << strct->getName() << "\n";
        //     llvm::errs() << "\tIs opaque: " << (strct->isOpaque() ? "yes" : "no") << "\n";
        //     if (not strct->isOpaque()) {
        //         for (auto const &field : strct->elements()) {
        //             llvm::errs() << "\tField type: ";
        //             field->print(llvm::errs());
        //             llvm::errs() << "\n";
        //         }
        //     }
        // }

        llvm::errs() << "=== IR for module: " << ctx->Module->getName() << " ===\n";
        ctx->Module->print(llvm::errs(), nullptr);
        llvm::errs() << "=== End IR for module: " << ctx->Module->getName() << " ===\n";

        if (llvm::verifyModule(*ctx->Module, &llvm::errs())) {
            llvm::errs() << "Invalid module: " << ctx->Module->getName() << "\n";
            std::abort();
        }

        // auto ec = std::error_code();
        // auto file = out_path / (ctx->module->getName().str() + ".ll");
        // auto out = llvm::raw_fd_ostream(file.string(), ec, static_cast<llvm::sys::fs::OpenFlags>(0));
        // ctx->module->print(out, nullptr);
        // out.flush();
    }
}

auto spp::compiler::CompilerBoot::_ValidateEntryPoint(
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Get the "main.spp" main module (entry point).
    const auto main_mod = *genex::find_if(_Modules, [](auto const *mod) {
        return mod->FileName()->Val.ends_with("main.spp");
    });

    // Check whether the "main" function exists with the correct signature.
    const auto main_call = INJECT_CODE("main(std::vector::Vec[std::string::Str]())", parse_expression);
    sm->Reset();
    sm->MoveToNextScope(); // into the "main" scope.

    try {
        auto meta = asts::meta::CompilerMetaData();
        meta.CurrentStage = 9.0;
        main_call->Stage7_AnalyseSemantics(sm, &meta);
    }

    // Check that the "main" function exists,
    catch (analyse::errors::SppIdentifierUnknownError const &) {
        Raise<analyse::errors::SppMissingMainFunctionError>({sm->GlobalScope.get()}, ERR_ARGS(*main_mod));
    }

    // Check that if the "main" function exists, the signature is compatible.
    catch (analyse::errors::SppFunctionCallNoValidSignaturesError const &) {
        Raise<analyse::errors::SppMissingMainFunctionError>({sm->GlobalScope.get()}, ERR_ARGS(*main_mod));
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
    if (genex::contains(mod_ns, "src"s)) {
        const auto src_index = genex::find(mod_ns, "src"s) - mod_ns.begin() + 1z;
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
        if (const auto quick_ns_sym = sm->CurrentScope->GetNsSymbol(identifier_part, true); quick_ns_sym != nullptr) {
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
