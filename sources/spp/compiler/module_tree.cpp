module;
#include <spp/macros.hpp>

module spp.compiler.module_tree;
import spp.asts.module_prototype_ast;
import spp.utils.files;
import genex;
import std;

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

auto spp::compiler::Module::TestHarness(
  std::filesystem::path const &tst_root)
  -> Unique<Module> {
  auto mod = MakeUnique<Module>(tst_root / "main.spp", "", Vec<lex::RawToken>{}, nullptr, nullptr);
  mod->is_test_harness = true;
  return mod;
}

spp::compiler::ModuleTree::ModuleTree(
  std::filesystem::path path,
  TestScope const &tests) {
  using namespace std::string_literals;

  // Get all the spp module files from the src path.
  m_root = std::move(path);
  m_src_path = m_root / "src";
  m_vcs_path = m_root / "vcs";
  m_ffi_path = m_root / "ffi";
  m_tst_path = m_root / "tst";

  // The libraries under "vcs", by folder name, and the source roots every module is measured against.
  auto vcs_libs = Vec<Pair<Str, std::filesystem::path>>();
  if (std::filesystem::exists(m_vcs_path)) {
    for (auto const &entry : std::filesystem::directory_iterator(m_vcs_path)) {
      if (not entry.is_directory()) { continue; }
      vcs_libs.EmplaceBack(spp::utils::files::NativeString(entry.path().filename()), entry.path());
    }
  }

  m_source_roots = Vec{m_src_path, m_tst_path};
  for (auto const &[_, lib_path] : vcs_libs) {
    m_source_roots.EmplaceBack(lib_path / "src");
    m_source_roots.EmplaceBack(lib_path / "tst");
  }

  // Get all the modules from the src and vcs path.
  auto src_modules = spp::utils::files::GlobSpp(m_src_path)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  auto vcs_modules = spp::utils::files::GlobSpp(m_vcs_path)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  auto ffi_modules = spp::utils::files::GlobSpp(m_ffi_path)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  // The project's own tests, and only when they were asked for - see "TestScope".
  auto tst_modules = tests.project
    ? spp::utils::files::GlobSpp(m_tst_path)
    | genex::views::filter([this](auto const &p) { return p != m_tst_path / "main.spp"; })
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>()
    : decltype(ffi_modules)();

  // Remove the "main.spp" files from the vcs modules.
  auto filtered_vcs_modules = decltype(vcs_modules)();
  for (auto &&m : vcs_modules) {
    auto relative_path = std::filesystem::relative(m->path, m_vcs_path);
    const auto lib_name = spp::utils::files::NativeString(*relative_path.begin());
    auto inner_path = std::filesystem::path();

    relative_path = relative_path.lexically_relative(*relative_path.begin()); // strip first component.
    for (auto const &part : relative_path) {
      inner_path /= part;
    }

    //
    const auto inner_native = spp::utils::files::NativeString(inner_path);
    const auto is_ffi = inner_native.starts_with("ffi"s + std::filesystem::path::preferred_separator);
    const auto is_tst = inner_native.starts_with("tst"s + std::filesystem::path::preferred_separator);
    const auto keep_tst = tests.WantsLib(lib_name) and inner_path != std::filesystem::path("tst/main.spp");

    if (inner_path != std::filesystem::path("src/main.spp") and not is_ffi and (not is_tst or keep_tst)) {
      filtered_vcs_modules.EmplaceBack(std::move(m));
    }
  }
  vcs_modules = std::move(filtered_vcs_modules);

  // A test build is entered through the generated harness,
  // not through the project's own "main", so the latter is
  // left out entirely - two "main" functions in the one
  // namespace would be a duplicate definition.
  if (tests.Any()) {
    src_modules |= genex::actions::remove_if(
      [this](auto const &m) { return m->path == m_src_path / "main.spp"; });
    tst_modules.EmplaceBack(Module::TestHarness(m_tst_path));
  }

  // Merge the src, vcs and ffi modules together.
  auto all_modules = std::move(src_modules);
  all_modules.AppendRange(std::move(vcs_modules));
  all_modules.AppendRange(std::move(ffi_modules));
  all_modules.AppendRange(std::move(tst_modules));
  m_modules = std::move(all_modules);

  // Measure every module's namespace now that all the roots
  // are known.
  for (auto const &m : m_modules) {
    m->ns_parts = m->is_test_harness ? Vec{Str("main")} : NamespaceOf(m->path);
  }

  Lock();
  for (auto &&m : m_modules) {
    if (m->is_test_harness) { continue; }
    m->code = utils::files::ReadFile(std::filesystem::current_path() / m->path);
  }
  Unlock();
}

auto spp::compiler::ModuleTree::NamespaceOf(
  std::filesystem::path const &module_path) const
  -> Vec<Str> {
  // The longest root the module sits under is the one that
  // produced it: "vcs/std/src" beats nothing, and a nested
  // "src" inside a package never wins over the real root
  // above it because it is not in the list at all.
  auto best_rel = std::filesystem::path();
  auto best_len = 0uz;
  auto best_is_tst = false;
  for (auto const &root : m_source_roots) {
    const auto rel = module_path.lexically_relative(root);
    if (rel.empty() or *rel.begin() == "..") { continue; }
    const auto len = spp::utils::files::NativeString(root).length();
    if (len < best_len) { continue; }
    best_len = len;
    best_rel = rel;
    best_is_tst = root.filename() == "tst";
  }

  // Under no source root: an ffi stub, namespaced by the
  // package folder holding it.
  auto parts = Vec<Str>();
  if (best_rel.empty()) {
    const auto raw = Vec<Str>(module_path.begin(), module_path.end());
    return Vec<Str>{raw[raw.Len() - 2]};
  }

  for (auto const &part : best_rel) { parts.EmplaceBack(spp::utils::files::NativeString(part)); }
  parts.Back().erase(parts.Back().length() - 4);
  if (best_is_tst) { parts.Insert(parts.begin() + (parts.IsEmpty() ? 0z : 1z), Str("tst")); }
  return parts;
}

auto spp::compiler::ModuleTree::ForCppGoogleTest(
  std::filesystem::path path,
  Str &&main_code)
  -> Unique<ModuleTree> {
  // Create a new ModuleTree with a single module containing the
  // main_code.
  auto c = MakeUnique<ModuleTree>(std::move(path));
  c->m_modules[0]->code = std::move(main_code);
  return c;
}

auto spp::compiler::ModuleTree::Lock() -> void {
  m_lock.LockShared(".lock");
}

auto spp::compiler::ModuleTree::Unlock() -> void {
  m_lock.Unlock();
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

auto spp::compiler::ModuleTree::LlvmOutPathFor(
  std::filesystem::path const &module_path) const
  -> std::filesystem::path {
  const auto out_root = m_root / "out" / "llvm";

  // The project's own sources lose their "src" prefix, so
  // "<root>/src/a/b.spp" mirrors to "<root>/out/llvm/a/b.ll".
  const auto roots = Vec<Pair<std::filesystem::path, std::filesystem::path>>{
    {m_src_path, std::filesystem::path()},
    {m_vcs_path, std::filesystem::path("vcs")},
    {m_ffi_path, std::filesystem::path("ffi")},
    {m_tst_path, std::filesystem::path("tst")}
  };

  for (auto const &[root, prefix] : roots) {
    const auto relative_path = module_path.lexically_relative(root);
    if (relative_path.empty() or *relative_path.begin() == "..") { continue; }
    auto out_file = out_root / prefix / relative_path;
    out_file.replace_extension(".ll");
    return out_file;
  }

  auto out_file = out_root / module_path.filename();
  out_file.replace_extension(".ll");
  return out_file;
}

SPP_MOD_END
