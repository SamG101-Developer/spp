module;
#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

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
  Path(std::move(path)),
  Code(std::move(code)),
  Tokens(std::move(tokens)),
  ModuleAst(std::move(module_ast)),
  Formatter(std::move(error_formatter)) {}

auto spp::compiler::Module::FromPath(std::filesystem::path const &path) {
  return MakeUnique<Module>(path, "", Vec<lex::RawToken>{}, nullptr, nullptr);
}

auto spp::compiler::Module::TestHarness(
  std::filesystem::path const &tst_root)
  -> Unique<Module> {
  auto mod = MakeUnique<Module>(tst_root / "main.spp", "", Vec<lex::RawToken>{}, nullptr, nullptr);
  mod->IsTestHarness = true;
  return mod;
}

spp::compiler::ModuleTree::ModuleTree(
  std::filesystem::path path,
  Str mode,
  TestScope const &tests) {
  // Get all the spp module files from the src path.
  _Root = std::move(path);

  // The target is a property of the whole build, set by the cli
  // before anything is compiled, so it is asked for here rather
  // than threaded through every constructor between the two.
  _Out = OutLayout{
    .Root = _Root, .Target = codegen::TargetFolderName(), .Mode = std::move(mode)};
  _SrcPath = _Root / "src";
  _VcsPath = _Root / "vcs";
  _FfiPath = _Root / "ffi";
  _TstPath = _Root / "tst";

  // The libraries under "vcs", by folder name, and the source roots every module is measured against.
  auto vcs_libs = Vec<Pair<Str, std::filesystem::path>>();
  if (std::filesystem::exists(_VcsPath)) {
    for (auto const &entry : std::filesystem::directory_iterator(_VcsPath)) {
      if (not entry.is_directory()) { continue; }
      vcs_libs.EmplaceBack(spp::utils::files::NativeString(entry.path().filename()), entry.path());
    }
  }

  _SourceRoots = Vec{_SrcPath, _TstPath};
  for (auto const &[_, lib_path] : vcs_libs) {
    _SourceRoots.EmplaceBack(lib_path / "src");
    _SourceRoots.EmplaceBack(lib_path / "tst");
  }

  // Get all the modules from the src and vcs path.
  auto src_modules = spp::utils::files::GlobSpp(_SrcPath)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  auto vcs_modules = spp::utils::files::GlobSpp(_VcsPath)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  auto ffi_modules = spp::utils::files::GlobSpp(_FfiPath)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  // The project's own tests, and only when they were asked for - see "TestScope".
  auto tst_modules = tests.Project
    ? spp::utils::files::GlobSpp(_TstPath)
    | genex::views::filter([this](auto const &p) { return p != _TstPath / "main.spp"; })
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>()
    : decltype(ffi_modules)();

  // Remove the "main.spp" files from the vcs modules.
  auto filtered_vcs_modules = decltype(vcs_modules)();
  for (auto &&m : vcs_modules) {
    auto relative_path = std::filesystem::relative(m->Path, _VcsPath);
    const auto lib_name = spp::utils::files::NativeString(*relative_path.begin());
    auto inner_path = std::filesystem::path();

    relative_path = relative_path.lexically_relative(*relative_path.begin()); // strip first component.
    for (auto const &part : relative_path) {
      inner_path /= part;
    }

    // Compare the leading component rather than a string prefix: the native
    // separator is a wchar_t on Windows, so it cannot be spliced onto a Str.
    const auto top = inner_path.empty() ? std::filesystem::path() : *inner_path.begin();
    const auto is_ffi = top == "ffi";
    const auto is_tst = top == "tst";
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
      [this](auto const &m) { return m->Path == _SrcPath / "main.spp"; });
    tst_modules.EmplaceBack(Module::TestHarness(_TstPath));
  }

  // Merge the src, vcs and ffi modules together.
  auto all_modules = std::move(src_modules);
  all_modules.AppendRange(std::move(vcs_modules));
  all_modules.AppendRange(std::move(ffi_modules));
  all_modules.AppendRange(std::move(tst_modules));
  _Modules = std::move(all_modules);

  // Measure every module's namespace now that all the roots
  // are known.
  for (auto const &m : _Modules) {
    m->NsParts = m->IsTestHarness ? Vec{Str("main")} : NamespaceOf(m->Path);
  }

  Lock();
  for (auto &&m : _Modules) {
    if (m->IsTestHarness) { continue; }
    m->Code = utils::files::ReadFile(std::filesystem::current_path() / m->Path);
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
  for (auto const &root : _SourceRoots) {
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
    return Vec<Str>{spp::utils::files::NativeString(module_path.parent_path().filename())};
  }

  for (auto const &part : best_rel) { parts.EmplaceBack(spp::utils::files::NativeString(part)); }
  parts.Back().erase(parts.Back().length() - 4);
  if (best_is_tst) { parts.Insert(parts.begin() + (parts.IsEmpty() ? 0z : 1z), Str("tst")); }
  return parts;
}

auto spp::compiler::ModuleTree::ForCppGoogleTest(
  std::filesystem::path path,
  Str mode,
  Str &&main_code)
  -> Unique<ModuleTree> {
  // Create a new ModuleTree with a single module containing the
  // main_code.
  auto c = MakeUnique<ModuleTree>(std::move(path), std::move(mode));
  c->_Modules[0]->Code = std::move(main_code);
  return c;
}

auto spp::compiler::ModuleTree::Lock() -> void {
  _Lock.LockShared(".lock");
}

auto spp::compiler::ModuleTree::Unlock() -> void {
  _Lock.Unlock();
}

auto spp::compiler::ModuleTree::begin()
  -> Vec<Unique<Module>>::iterator {
  return _Modules.begin();
}

auto spp::compiler::ModuleTree::end()
  -> Vec<Unique<Module>>::iterator {
  return _Modules.end();
}

auto spp::compiler::ModuleTree::GetModules()
  -> Vec<Module*> {
  return _Modules | genex::views::ptr | genex::to<Vec>();
}

auto spp::compiler::ModuleTree::RootPath() const
  -> std::filesystem::path {
  return _Root;
}

auto spp::compiler::ModuleTree::Out() const
  -> OutLayout const& {
  return _Out;
}

auto spp::compiler::ModuleTree::LlvmOutPathFor(
  std::filesystem::path const &module_path) const
  -> std::filesystem::path {
  const auto out_root = _Out.LlvmRoot();

  // The project's own sources lose their "src" prefix, so
  // "<root>/src/a/b.spp" mirrors to "<root>/out/<mode>/llvm/a/b.ll".
  const auto roots = Vec<Pair<std::filesystem::path, std::filesystem::path>>{
    {_SrcPath, std::filesystem::path()},
    {_VcsPath, std::filesystem::path("vcs")},
    {_FfiPath, std::filesystem::path("ffi")},
    {_TstPath, std::filesystem::path("tst")}
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
