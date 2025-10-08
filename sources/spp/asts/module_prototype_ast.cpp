#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/asts/module_member_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>

#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>


spp::asts::ModulePrototypeAst::ModulePrototypeAst(
    decltype(impl) &&impl) :
    impl(std::move(impl)) {
}


spp::asts::ModulePrototypeAst::~ModulePrototypeAst() = default;


auto spp::asts::ModulePrototypeAst::pos_start() const -> std::size_t {
    return impl->pos_start();
}


auto spp::asts::ModulePrototypeAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


auto spp::asts::ModulePrototypeAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ModulePrototypeAst>(ast_clone(impl));
}


spp::asts::ModulePrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::ModulePrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::ModulePrototypeAst::name() const
    -> std::unique_ptr<IdentifierAst> {
    using namespace std::string_literals;
    // Split the file path intro parts.
    auto parts = std::vector<std::string>();
    for (auto const &entry : std::filesystem::directory_iterator(m_file_path)) {
        if (entry.is_directory()) {
            parts.emplace_back(entry.path().filename().string());
        }
    }

    // Check if "src" is in the file path.
    auto name = std::string();
    if (genex::algorithms::contains(parts, "src"s)) {
        name = parts
            | genex::views::drop(genex::algorithms::position(parts, [](auto &&x) { return x == "src"; }))
            | genex::views::intersperse("::"s)
            | genex::views::join
            | genex::to<std::string>();
    }
    else {
        name = std::vector{parts[0], parts[1] + ".spp"}
            | genex::views::intersperse("::"s)
            | genex::views::join
            | genex::to<std::string>();
    }

    return std::make_unique<IdentifierAst>(pos_start(), std::move(name));
}


auto spp::asts::ModulePrototypeAst::file_name() const
    -> std::unique_ptr<IdentifierAst> {
    using namespace std::string_literals;
    // Return the filepath as an IdentifierAst.
    return std::make_unique<IdentifierAst>(pos_start(), m_file_path.string());
}


auto spp::asts::ModulePrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Shift to implementation.
    Ast::stage_1_pre_process(ctx);
    impl->stage_1_pre_process(this);
}


auto spp::asts::ModulePrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_2_gen_top_level_scopes(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_3_gen_top_level_aliases(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_4_qualify_types(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_5_load_super_scopes(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_6_pre_analyse_semantics(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ModulePrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to implementation.
    impl->stage_8_check_memory(sm, meta);
}
