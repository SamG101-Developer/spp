#include <spp/asts/module_prototype_ast.hpp>
#include <spp/compiler/module_tree.hpp>

#include <genex/actions/remove_if.hpp>
#include <genex/views/borrow.hpp>
#include <genex/views/chunk.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/flatten_with.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/move.hpp>
#include <genex/views/to.hpp>
#include <genex/views/transform.hpp>
#include <Glob-1.0/glob/glob.h>


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
        | genex::views::to<std::vector>();

    auto vcs_modules = glob::rglob(m_vcs_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::from_path(p); })
        | genex::views::to<std::vector>();

    auto ffi_modules = glob::rglob(m_ffi_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::from_path(p); })
        | genex::views::to<std::vector>();

    // Remove the "main.spp" files from the vcs modules.
    for (auto &m : vcs_modules | genex::views::borrow) {
        auto relative_path = std::filesystem::relative(m.path, m_vcs_path);
        auto inner_path = std::filesystem::path();
        auto sep = std::filesystem::path::preferred_separator;
        for (auto const &part : std::filesystem::path(relative_path.string() | genex::views::chunk(sep) | genex::views::drop(1) | genex::views::materialize() | genex::views::flatten_with(sep) | genex::views::to<std::string>())) {
            inner_path /= part;
        }

        // Check if the inner_path matches "src/main.spp" or startswith "ffi/":
        if (inner_path == std::filesystem::path("src/main.spp") or inner_path.string().starts_with("ffi"s + std::filesystem::path::preferred_separator)) {
            vcs_modules |= genex::actions::remove_if([&m](auto &&x) { return x.path == m.path; });
        }
    }

    // Merge the src, vcs and ffi modules together.
    m_modules = genex::views::concat(src_modules | genex::views::move, vcs_modules | genex::views::move, ffi_modules | genex::views::move) | genex::views::to<std::vector>();
    for (auto &m : m_modules) {
        m.path = m.path.string().substr(std::filesystem::current_path().string().size());
    }
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
