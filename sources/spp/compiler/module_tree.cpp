module;
#include <genex/to_container.hpp>
#include <genex/actions/remove_if.hpp>
#include <genex/views/borrow.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/join_with.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/move.hpp>
#include <genex/views/split.hpp>
#include <genex/views/transform.hpp>

module spp.compiler.module_tree;
import spp.asts.module_prototype_ast;
import glob;


auto spp::compiler::Module::from_path(std::filesystem::path const &path) {
    return Module(path, "", {}, nullptr, nullptr);
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
    auto filtered_vcs_modules = std::vector<Module>();
    for (auto &m : vcs_modules) {
        auto relative_path = std::filesystem::relative(m.path, m_vcs_path);
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

    // Merge the src, vcs and ffi modules together.
    m_modules = genex::views::concat(src_modules | genex::views::move, filtered_vcs_modules | genex::views::move, ffi_modules | genex::views::move) | genex::to<std::vector>();
}


auto spp::compiler::ModuleTree::begin()
    -> std::vector<Module>::iterator {
    return m_modules.begin();
}


auto spp::compiler::ModuleTree::end()
    -> std::vector<Module>::iterator {
    return m_modules.end();
}


auto spp::compiler::ModuleTree::get_modules()
    -> std::vector<Module>& {
    return m_modules;
}
