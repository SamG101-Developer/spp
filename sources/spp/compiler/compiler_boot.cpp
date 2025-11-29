module;
#include <genex/actions/drop.hpp>
#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/find.hpp>
#include <genex/algorithms/find_if.hpp>

#include <spp/parse/macros.hpp>

module spp.compiler.compiler_boot;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.module_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import spp.utils.files;


#define PREP_SCOPE_MANAGER \
    auto const &mod_in_tree = *genex::algorithms::find_if(tree, [&](auto &m) { return m.module_ast.get() == mod; })


#define PREP_SCOPE_MANAGER_AND_META(s)                                      \
    PREP_SCOPE_MANAGER;                                                     \
    spp::compiler::CompilerBoot::move_scope_manager_to_ns(sm, mod_in_tree); \
    auto meta = spp::asts::meta::CompilerMetaData();                        \
    meta.current_stage = (s)


auto spp::compiler::CompilerBoot::lex(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void {
    // Lexing stage.
    for (auto &mod : tree) {
        mod.code = utils::files::read_file(std::filesystem::current_path() / mod.path);
        mod.tokens = lex::Lexer(mod.code).lex();
        mod.error_formatter = std::make_unique<utils::errors::ErrorFormatter>(mod.tokens, mod.path.string());
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::parse(
    utils::ProgressBar &bar,
    ModuleTree &tree)
    -> void {
    // Parsing stage.
    for (auto &mod : tree) {
        mod.module_ast = parse::ParserSpp(mod.tokens, mod.error_formatter).parse();
        m_modules.emplace_back(mod.module_ast.get());
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_1_pre_process(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    asts::Ast *ctx)
    -> void {
    // Pre-processing stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER;
        mod->m_file_path = mod_in_tree.path;
        mod->stage_1_pre_process(ctx);
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_2_gen_top_level_scopes(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Generate top-level scopes stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(4.0);
        mod->stage_2_gen_top_level_scopes(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_3_gen_top_level_aliases(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Generate top-level aliases stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(5.0);
        mod->stage_3_gen_top_level_aliases(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_4_qualify_types(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Qualify types stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(6.0);
        mod->stage_4_qualify_types(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_5_load_super_scopes(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Load super scopes stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(7.0);
        mod->stage_5_load_super_scopes(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();

    // Attach all super scopes now.
    // Todo: New progress bar here
    auto meta = asts::meta::CompilerMetaData();
    meta.current_stage = 7.5;
    sm->attach_all_super_scopes(&meta);
}


auto spp::compiler::CompilerBoot::stage_6_pre_analyse_semantics(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Pre-analyse semantics stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(8.0);
        mod->stage_6_pre_analyse_semantics(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::stage_7_analyse_semantics(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Analyse semantics stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(9.0);
        mod->stage_7_analyse_semantics(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();

    // Validate entry point now.
    validate_entry_point(sm);
}


auto spp::compiler::CompilerBoot::stage_8_check_memory(
    utils::ProgressBar &bar,
    ModuleTree &tree,
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Check memory stage.
    for (auto &mod : m_modules) {
        PREP_SCOPE_MANAGER_AND_META(10.0);
        mod->stage_8_check_memory(sm, &meta);
        sm->reset();
        bar.next();
    }
    bar.finish();
}


auto spp::compiler::CompilerBoot::validate_entry_point(
    analyse::scopes::ScopeManager *sm)
    -> void {
    // Get the "main.spp" main module (entry point).
    const auto main_mod = *genex::algorithms::find_if(m_modules, [](auto const *mod) {
        return mod->file_name()->val.ends_with("main.spp");
    });

    // Check whether the "main" function exists with the correct signature.
    const auto main_call = INJECT_CODE("main::main(std::vector::Vec[std::string::Str]())", parse_expression);
    sm->reset();

    try {
        auto meta = asts::meta::CompilerMetaData();
        meta.current_stage = 9.0;
        main_call->stage_7_analyse_semantics(sm, &meta);
    }

    catch (analyse::errors::SppFunctionCallNoValidSignaturesError const &) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMissingMainFunctionError>().with_args(
            *main_mod).with_scopes({sm->global_scope.get()}).raise();
    }

    catch (analyse::errors::SppIdentifierUnknownError const &) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMissingMainFunctionError>().with_args(
            *main_mod).with_scopes({sm->global_scope.get()}).raise();
    }
}


auto spp::compiler::CompilerBoot::move_scope_manager_to_ns(
    analyse::scopes::ScopeManager *sm,
    Module const &mod)
    -> void {
    using namespace std::string_literals;
    // Create the module namespace as a list of strings.
    auto mod_ns = std::vector<std::string>(mod.path.begin(), mod.path.end());
    if (genex::algorithms::contains(mod_ns, "src"s)) {
        const auto src_index = genex::algorithms::find(mod_ns, "src"s) - mod_ns.begin() + 1;
        mod_ns |= genex::actions::drop(src_index);
        mod_ns.back().erase(mod_ns.back().size() - 4);
    }
    else {
        mod_ns = std::vector{mod_ns[mod_ns.size() - 2]};
    }

    // Iterate over the parts of the module namespace.
    for (auto const &part : mod_ns) {
        // Convert the string part into an IdentifierAst node.
        auto identifier_part = std::make_shared<asts::IdentifierAst>(0, std::string(part));

        // If the part exists in the current scope (starting from the global scope), then move into it.
        if (const auto quick_ns_sym = sm->current_scope->get_ns_symbol(identifier_part, true); quick_ns_sym != nullptr) {
            const auto ns_scope = quick_ns_sym->scope;
            sm->reset(ns_scope);
        }

        // Otherwise, create a new scope and move into it.
        else {
            const auto ns_sym = std::make_shared<analyse::scopes::NamespaceSymbol>(identifier_part, nullptr);
            sm->current_scope->add_ns_symbol(ns_sym);
            const auto ns_scope = sm->create_and_move_into_new_scope(identifier_part, nullptr, mod.error_formatter.get());
            ns_sym->scope = ns_scope;
            ns_sym->scope->ns_sym = ns_sym;
            ns_sym->scope->ast = mod.module_ast.get();
        }
    }
}
