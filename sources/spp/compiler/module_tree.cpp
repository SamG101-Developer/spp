module spp.compiler.module_tree;
import spp.asts.module_prototype_ast;
import glob;
import genex;
import std;


spp::compiler::Module::Module(
    std::filesystem::path path,
    std::string code,
    std::vector<lex::RawToken> tokens,
    std::unique_ptr<asts::ModulePrototypeAst> module_ast,
    std::shared_ptr<utils::errors::ErrorFormatter> error_formatter) :
    path(std::move(path)),
    code(std::move(code)),
    tokens(std::move(tokens)),
    module_ast(std::move(module_ast)),
    error_formatter(std::move(error_formatter)) {
}


auto spp::compiler::Module::from_path(std::filesystem::path const &path) {
    return std::make_unique<Module>(path, "", std::vector<lex::RawToken>{}, nullptr, nullptr);
}


spp::compiler::ModuleTree::ModuleTree(
    std::filesystem::path path) {
    using namespace std::string_literals;

    // Get all the spp module files from the src path.
    m_root = std::move(path);
    m_src_path = m_root / "src";
    m_vcs_path = m_root / "vcs";
    m_ffi_path = m_root / "ffi";

    // Get all the modules from the src and vcs path.
    auto src_modules = glob::rglob(m_src_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::from_path(p); })
        | genex::to<std::vector>();

    auto test = glob::rglob(m_vcs_path / "**/*.spp");
    auto vcs_modules = glob::rglob(m_vcs_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::from_path(p); })
        | genex::to<std::vector>();

    auto ffi_modules = glob::rglob(m_ffi_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::from_path(p); })
        | genex::to<std::vector>();

    // Remove the "main.spp" files from the vcs modules.
    auto filtered_vcs_modules = decltype(vcs_modules)();
    for (auto &&m : vcs_modules) {
        auto relative_path = std::filesystem::relative(m->path, m_vcs_path);
        auto inner_path = std::filesystem::path();
        constexpr auto sep = std::filesystem::path::preferred_separator;
        for (auto const &part : std::filesystem::path(relative_path.string() | genex::views::split(sep) | genex::views::drop(1) | genex::views::materialize | genex::views::join_with(sep) | genex::to<std::string>())) {
            inner_path /= part;
        }

        // Check if the inner_path matches "src/main.spp" or startswith "ffi/":
        if (inner_path != std::filesystem::path("src/main.spp") and not inner_path.string().starts_with("ffi"s + std::filesystem::path::preferred_separator)) {
            filtered_vcs_modules.emplace_back(std::move(m));
        }
    }
    vcs_modules = std::move(filtered_vcs_modules);

    // Merge the src, vcs and ffi modules together.
    auto all_modules = std::move(src_modules);
    all_modules.insert(all_modules.end(), std::make_move_iterator(vcs_modules.begin()), std::make_move_iterator(vcs_modules.end()));
    all_modules.insert(all_modules.end(), std::make_move_iterator(ffi_modules.begin()), std::make_move_iterator(ffi_modules.end()));
    m_modules = std::move(all_modules);
}


auto spp::compiler::ModuleTree::begin()
    -> std::vector<std::unique_ptr<Module>>::iterator {
    return m_modules.begin();
}


auto spp::compiler::ModuleTree::end()
    -> std::vector<std::unique_ptr<Module>>::iterator {
    return m_modules.end();
}


auto spp::compiler::ModuleTree::get_modules()
    -> std::vector<Module*> {
    return m_modules
        | genex::views::ptr
        | genex::to<std::vector>();
}
