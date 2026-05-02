module;
#include <spp/macros.hpp>

module spp.compiler.module_tree;
import spp.asts.module_prototype_ast;
import spp.utils.files;
import genex;
import glob;
import std;
import sys;

SPP_MOD_BEGIN
spp::compiler::Module::Module(
    std::filesystem::path path,
    Str code,
    Vec<lex::RawToken> tokens,
    Unique<asts::ModulePrototypeAst> module_ast,
    Shared<utils::errors::ErrorFormatter> error_formatter) :
    path(std::move(path)),
    code(std::move(code)),
    tokens(std::move(tokens)),
    module_ast(std::move(module_ast)),
    error_formatter(std::move(error_formatter)) {}

auto spp::compiler::Module::FromPath(std::filesystem::path const &path) {
    return MakeUnique<Module>(path, "", Vec<lex::RawToken>{}, nullptr, nullptr);
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
        | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
        | genex::to<Vec>();

    auto vcs_modules = glob::rglob(m_vcs_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
        | genex::to<Vec>();

    auto ffi_modules = glob::rglob(m_ffi_path / "**/*.spp")
        | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
        | genex::to<Vec>();

    // Remove the "main.spp" files from the vcs modules.
    auto filtered_vcs_modules = decltype(vcs_modules)();
    for (auto &&m : vcs_modules) {
        auto relative_path = std::filesystem::relative(m->path, m_vcs_path);
        auto inner_path = std::filesystem::path();
        constexpr auto sep = std::filesystem::path::preferred_separator;
        for (auto const &part : std::filesystem::path(relative_path.string() | genex::views::split(sep) | genex::views::drop(1) | genex::to<Vec>() | genex::views::join_with(sep) | genex::to<Str>())) {
            inner_path /= part;
        }

        // Check if the inner_path matches "src/main.spp" or startswith "ffi/":
        if (inner_path != std::filesystem::path("src/main.spp") and not inner_path.string().starts_with("ffi"s + std::filesystem::path::preferred_separator)) {
            filtered_vcs_modules.EmplaceBack(std::move(m));
        }
    }
    vcs_modules = std::move(filtered_vcs_modules);

    // Merge the src, vcs and ffi modules together.
    auto all_modules = std::move(src_modules);
    all_modules.AppendRange(std::move(vcs_modules));
    all_modules.AppendRange(std::move(ffi_modules));
    m_modules = std::move(all_modules);

    Lock();
    for (auto &&m : m_modules) {
        m->code = utils::files::read_file(std::filesystem::current_path() / m->path);
    }
    Unlock();
}

auto spp::compiler::ModuleTree::for_unit_tests(
    std::filesystem::path path,
    Str &&main_code)
    -> Unique<ModuleTree> {
    // Create a new ModuleTree with a single module containing the main_code.
    auto c = MakeUnique<ModuleTree>(std::move(path));
    c->m_modules[0]->code = std::move(main_code);
    return c;
}

auto spp::compiler::ModuleTree::Lock() -> void {
    m_lock_fd = sys::open(".lock", sys::O_RDWR | sys::O_CREAT);
    sys::flock(m_lock_fd, sys::LOCK_EX);
}

auto spp::compiler::ModuleTree::Unlock() const -> void {
    sys::flock(m_lock_fd, sys::LOCK_UN);
    sys::close(m_lock_fd);
}

auto spp::compiler::ModuleTree::begin()
    -> Vec<Unique<Module>>::iterator {
    return m_modules.begin();
}

auto spp::compiler::ModuleTree::end()
    -> Vec<Unique<Module>>::iterator {
    return m_modules.end();
}

auto spp::compiler::ModuleTree::GetModules()
    -> Vec<Module*> {
    return m_modules | genex::views::ptr | genex::to<Vec>();
}

auto spp::compiler::ModuleTree::RootPath() const
    -> std::filesystem::path {
    return m_root;
}

SPP_MOD_END
