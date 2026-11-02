module;
#include <spp/macros.hpp>

module spp.compiler.module_tree;
import spp.asts.module_prototype_ast;
import spp.codegen.llvm_target;
import spp.compiler.dependencies;
import spp.compiler.ffi;
import spp.utils.files;
import spp.utils.ice;
import genex;
import std;

SPP_MOD_BEGIN
/// [CHECKED]
spp::compiler::Module::Module(
  spp::Path path, Str code, Vec<lex::RawToken> tokens,
  Unique<asts::ModulePrototypeAst> module_ast,
  Shared<utils::errors::ErrorFormatter> error_formatter) :
  Path(std::move(path)),
  Code(std::move(code)),
  Tokens(std::move(tokens)),
  ModuleAst(std::move(module_ast)),
  Formatter(std::move(error_formatter)) {}

/// [CHECKED]
auto spp::compiler::Module::FromPath(spp::Path const &path) {
  // Build an empty module for a path. The lexing and parsing stages will then
  // fill the tokens and module asts.
  return MakeUnique<Module>(
    path, "", Vec<lex::RawToken>{}, nullptr, nullptr);
}

/// [CHECKED]
auto spp::compiler::Module::TestHarness(
  spp::Path const &tst_root) -> Unique<Module> {
  // Build an empty module for a test hardness. This is the entry for the test
  // harness and has a custom "main.spp" to call the test functions.
  auto mod = MakeUnique<Module>(
    tst_root / "main.spp", "", Vec<lex::RawToken>{}, nullptr, nullptr);
  mod->IsTestHarness = true;
  return mod;
}

/// [CHECKED]
spp::compiler::ModuleTree::ModuleTree(
  spp::Path path, const BuildMode mode, TestScope const &tests) {
  // The target is a property of the whole build, set by the cli before anything
  // is compiled, so it is asked for here rather than threaded through every
  // constructor between the two.
  _Root = std::move(path);
  _Out = OutLayout{
    .Root = _Root, .Target = codegen::TargetFolderName(), .Mode = mode};
  _SrcPath = _Root / "src";
  _VcsPath = _Root / "vcs";
  _FfiPath = _Root / "ffi";
  _TstPath = _Root / "tst";

  // The dependencies, and the source roots every module is measured against.
  const auto deps = dependencies::Resolve(_Root);
  _SourceRoots = Vec{_SrcPath, _TstPath};
  for (auto const &dependency : deps) {
    _SourceRoots.emplace_back(dependency.Folder / "src");
    _SourceRoots.emplace_back(dependency.Folder / "tst");
  }

  // Get all the modules from the src path.
  auto src_modules = spp::utils::files::GlobSpp(_SrcPath)
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>();

  // Each dependency's "src", without its "main.spp", and its "tst" only when
  // asked for. Nothing else in a dependency is compiled: its "ffi" stubs come
  // through "ffi::Packages", and any other folder is its own business.
  auto vcs_modules = decltype(src_modules)();
  for (auto const &dependency : deps) {
    const auto src_path = dependency.Folder / "src";
    if (std::filesystem::is_directory(src_path)) {
      for (auto const &p : spp::utils::files::GlobSpp(src_path)) {
        if (p == src_path / "main.spp") { continue; }
        vcs_modules.emplace_back(Module::FromPath(p))->IsStd = dependency.Name == "std";
      }
    }

    const auto tst_path = dependency.Folder / "tst";
    if (tests.WantsLib(dependency.Name) and std::filesystem::is_directory(tst_path)) {
      for (auto const &p : spp::utils::files::GlobSpp(tst_path)) {
        if (p != tst_path / "main.spp") { vcs_modules.emplace_back(Module::FromPath(p)); }
      }
    }
  }

  // The ffi packages, the project's and its dependencies', read where they are.
  auto ffi_modules = decltype(src_modules)();
  for (auto const &package : ffi::Packages(_Root)) {
    for (auto const &p : spp::utils::files::GlobSpp(package)) {
      ffi_modules.emplace_back(Module::FromPath(p))->IsFfi = true;
    }
  }

  // The project's own tests, and only when they were asked for - see "TestScope".
  auto tst_modules = tests.Project
    ? spp::utils::files::GlobSpp(_TstPath)
    | genex::views::filter([this](auto const &p) { return p != _TstPath / "main.spp"; })
    | genex::views::transform([](auto const &p) { return Module::FromPath(p); })
    | genex::to<Vec>()
    : decltype(ffi_modules)();

  // A test build is entered through the generated harness,
  // not through the project's own "main", so the latter is
  // left out entirely - two "main" functions in the one
  // namespace would be a duplicate definition.
  if (tests.Any()) {
    src_modules |= genex::actions::remove_if(
      [this](auto const &m) { return m->Path == _SrcPath / "main.spp"; });
    tst_modules.emplace_back(Module::TestHarness(_TstPath));
  }

  // Merge the src, vcs and ffi modules together.
  auto all_modules = std::move(src_modules);
  all_modules.append_range(genex::views::move(vcs_modules));
  all_modules.append_range(genex::views::move(ffi_modules));
  all_modules.append_range(genex::views::move(tst_modules));
  _Modules = std::move(all_modules);

  // What each package declares, which the modules it owns can name.
  const auto names_declared_by = [](Path const &package) {
    auto names = Vec<Str>();
    for (auto const &d : dependencies::Declared(package).value_or(Vec<dependencies::Dependency>())) {
      names.emplace_back(d.Name);
    }
    return names;
  };
  const auto project_declares = names_declared_by(_Root);
  auto dep_declares = Map<Str, Vec<Str>>();
  for (auto const &dependency : deps) { dep_declares[dependency.Name] = names_declared_by(dependency.Folder); }

  // Measure every module's namespace now that all the roots
  // are known, and note whose it is.
  for (auto const &m : _Modules) {
    m->NsParts = m->IsTestHarness ? Vec{Str("main")} : NamespaceOf(m->Path);
    m->Owner = m->IsTestHarness ? Nullopt : OwnerOf(m->Path);
    m->Declares = m->Owner.has_value() ? dep_declares[*m->Owner] : project_declares;
  }

  Lock();
  for (auto &&m : _Modules) {
    if (m->IsTestHarness) { continue; }
    m->Code = utils::files::ReadFile(std::filesystem::current_path() / m->Path);
  }
  Unlock();
}

auto spp::compiler::ModuleTree::NamespaceOf(
  Path const &module_path) const
  -> Vec<Str> {
  // The longest root the module sits under is the one that
  // produced it: "vcs/std/src" beats nothing, and a nested
  // "src" inside a package never wins over the real root
  // above it because it is not in the list at all.
  auto best_rel = Path();
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

  for (auto const &part : best_rel) { parts.emplace_back(spp::utils::files::NativeString(part)); }
  parts.back().erase(parts.back().length() - 4);
  if (best_is_tst) { parts.insert(parts.begin() + (parts.empty() ? 0z : 1z), Str("tst")); }
  return parts;
}

/// [CHECKED]
auto spp::compiler::ModuleTree::CheckNamespaces() const -> bool {
  // Modules are in a fixed order, so the first of a namespace's is reported
  // first, and each namespace once however many modules it has.
  struct Claim {
    Opt<Str> Owner;
    bool IsFfi;
    bool Reported = false;
  };

  // Helper for error messages concerning "who" owns a namespace (root, ffi
  // package, etc).
  const auto who = [](Opt<Str> const &owner, const bool is_ffi) {
    const auto package = owner.has_value()
      ? "the dependency '" + *owner + "'"
      : Str("this project");
    return (is_ffi ? "an ffi package of " : "") + package;
  };

  auto claims = Map<Str, Claim>();
  auto ok = true;
  for (auto const &m : _Modules) {
    auto const &root = m->NsParts.front();
    // Try to inject the new claim into the "claims" list, with the known owner
    // and ffi flag.
    auto &[owner, is_ffi, reported] = claims.try_emplace(
      root, Claim{.Owner = m->Owner, .IsFfi = m->IsFfi}).first->second;

    // If we have already reported an error on this namespace, or there is an
    // existing entry with the same owner and ffi flag, skip.
    if (reported or (owner == m->Owner and is_ffi == m->IsFfi)) { continue; }

    // Otherwise, we have a conflict, so report to the error console, and mark
    // the namespace as "reported".
    std::cerr
      << "Error: the top-level namespace '" << root << "' is both "
      << who(owner, is_ffi) << " and "
      << who(m->Owner, m->IsFfi) << ".\n"
      << "  A top-level namespace belongs to one package.\n";
    reported = true;
    ok = false;
  }
  return ok;
}

auto spp::compiler::ModuleTree::OwnerOf(
  Path const &module_path) const
  -> Opt<Str> {
  const auto rel = module_path.lexically_relative(_VcsPath);
  if (rel.empty() or *rel.begin() == "..") { return Nullopt; }
  return spp::utils::files::NativeString(*rel.begin());
}

auto spp::compiler::ModuleTree::ForCppGoogleTest(
  spp::Path path,
  const BuildMode mode,
  Str &&main_code)
  -> Unique<ModuleTree> {
  // Replace the source of the project's "main.spp" with the test's code, found
  // by its path as the modules are sorted, so it need not be the first.
  auto c = MakeUnique<ModuleTree>(std::move(path), mode);
  const auto main_path = c->_SrcPath / "main.spp";
  const auto main_module = genex::find_if(
    c->_Modules, [&](auto const &m) { return m->Path == main_path; });
  if (main_module == c->_Modules.end()) {
    Ice("a google test project has no '" + utils::files::DisplayString(main_path) + "'");
  }
  (*main_module)->Code = std::move(main_code);
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
  -> Path {
  return _Root;
}

auto spp::compiler::ModuleTree::Out() const
  -> OutLayout const& {
  return _Out;
}

auto spp::compiler::ModuleTree::LlvmOutPathFor(
  Path const &module_path) const
  -> Path {
  const auto out_root = _Out.LlvmRoot();

  // The project's own sources lose their "src" prefix, so
  // "<root>/src/a/b.spp" mirrors to "<root>/out/<mode>/llvm/a/b.ll".
  const auto roots = Vec<Pair<Path, Path>>{
    {_SrcPath, Path()},
    {_VcsPath, Path("vcs")},
    {_FfiPath, Path("ffi")},
    {_TstPath, Path("tst")}
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
