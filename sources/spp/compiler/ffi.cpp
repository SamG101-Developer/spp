module;
#include <spp/macros.hpp>

module spp.compiler.ffi;
import spp.codegen.llvm_artefact;
import spp.codegen.llvm_target;
import spp.utils.files;
import genex;
import std;

namespace {
  constexpr auto kStubFile = "stub.spp";

  /// The entries of "dir", sorted so every walk sees the same
  /// order; none when it does not exist.
  auto Entries(spp::Path const &dir) -> spp::Vec<std::filesystem::directory_entry> {
    if (not std::filesystem::is_directory(dir)) { return {}; }
    auto entries = spp::Vec<std::filesystem::directory_entry>(
      std::filesystem::directory_iterator(dir), std::filesystem::directory_iterator());
    entries |= genex::actions::sort;
    return entries;
  }

  /// The folders packages live in: the project's own "ffi", then
  /// each dependency's.
  auto FfiFolders(spp::Path const &root) -> spp::Vec<spp::Path> {
    auto folders = spp::Vec{root / "ffi"};
    for (auto const &dep : Entries(root / "vcs")) {
      if (dep.is_directory()) { folders.emplace_back(dep.path() / "ffi"); }
    }
    return folders;
  }

  auto Shown(spp::Path const &root, spp::Path const &path) -> spp::Str {
    return "'" + spp::utils::files::DisplayString(path.lexically_relative(root)) + "'";
  }

  auto Name(spp::Path const &package) -> spp::Str {
    return spp::utils::files::NativeString(package.filename());
  }

  /// The one file name "package"'s library has in "target".
  auto LibraryName(spp::Path const &package, spp::Str const &target) -> spp::Str {
    return spp::utils::files::SharedLibraryName(Name(package), spp::codegen::SharedLibraryExtension(target.c_str()));
  }

  /// Check one package's stub and target folders.
  auto ValidatePackage(spp::Path const &root, spp::Path const &package) -> bool {
    const auto example = Shown(root, package / spp::codegen::HostFolderName());
    if (not std::filesystem::exists(package / kStubFile)) {
      std::cerr << "Error: " << Shown(root, package) << " has no '" << kStubFile << "'.\n";
      return false;
    }

    auto targets = 0uz;
    for (auto const &entry : Entries(package)) {
      const auto found = spp::utils::files::NativeString(entry.path().filename());

      // Loose files (a SOURCE, README, LICENSE) are the package's
      // own business; only its library left outside a target
      // folder, the old flat layout, is an error.
      if (not entry.is_directory()) {
        const auto misplaced = genex::any_of(spp::Arr<spp::StrView, 3>{"so", "dylib", "dll"}, [&](auto const ext) {
          return found == spp::utils::files::SharedLibraryName(Name(package), ext);
        });
        if (not misplaced) { continue; }
        std::cerr
          << "Error: " << Shown(root, entry.path()) << " is not in a target folder.\n"
          << "  A library lives in the folder for its target, such as " << example << ".\n";
        return false;
      }

      if (not spp::codegen::IsTargetFolderName(found.c_str())) {
        std::cerr
          << "Error: " << Shown(root, entry.path()) << " does not name a target.\n"
          << "  A target folder is '<arch>-<os>[-<env>]', spelled as the compiler spells it, such as "
          << example << ".\n";
        return false;
      }

      if (not std::filesystem::exists(entry.path() / LibraryName(package, found))) {
        std::cerr << "Error: " << Shown(root, entry.path()) << " has no '" << LibraryName(package, found) << "'.\n";
        return false;
      }
      targets += 1;
    }

    if (targets == 0) {
      std::cerr
        << "Error: " << Shown(root, package) << " ships no library for any target.\n"
        << "  A library lives in the folder for its target, such as " << example << ".\n";
      return false;
    }
    return true;
  }
}

auto spp::compiler::ffi::Packages(
  Path const &root) -> Vec<Path> {
  auto packages = Vec<Path>();
  for (auto const &folder : FfiFolders(root)) {
    for (auto const &entry : Entries(folder)) {
      if (entry.is_directory()) { packages.emplace_back(entry.path()); }
    }
  }
  return packages;
}

auto spp::compiler::ffi::Validate(
  Path const &root) -> bool {
  auto seen = Map<Str, Path>();
  for (auto const &folder : FfiFolders(root)) {
    for (auto const &entry : Entries(folder)) {
      if (not entry.is_directory()) {
        std::cerr << "Error: " << Shown(root, entry.path()) << " is not an ffi package folder.\n";
        return false;
      }

      // A copy left in the project's own "ffi" by an older spp
      // would shadow the dependency's, so neither wins.
      if (const auto [it, fresh] = seen.try_emplace(Name(entry.path()), entry.path()); not fresh) {
        std::cerr
          << "Error: " << Shown(root, it->second) << " and " << Shown(root, entry.path())
          << " are both the ffi package '" << it->first << "'; remove one.\n";
        return false;
      }
      if (not ValidatePackage(root, entry.path())) { return false; }
    }
  }
  return true;
}

auto spp::compiler::ffi::Libraries(
  Path const &root) -> Opt<Vec<Path>> {
  const auto target = Str(spp::codegen::TargetFolderName());
  auto libraries = Vec<Path>();
  for (auto const &package : Packages(root)) {
    const auto library = package / target / LibraryName(package, target);
    if (std::filesystem::exists(library)) {
      libraries.emplace_back(library);
      continue;
    }

    auto shipped = Str();
    for (auto const &entry : Entries(package)) {
      if (not entry.is_directory()) { continue; }
      shipped += (shipped.empty() ? "" : ", ") + utils::files::NativeString(entry.path().filename());
    }
    std::cerr
      << "Error: " << Shown(root, package) << " has no library for " << target << ".\n"
      << "  It ships for " << (shipped.empty() ? Str("no target") : shipped) << ".\n";
    return Nullopt;
  }
  return libraries;
}

auto spp::compiler::ffi::CheckFortified(
  Path const &root, Vec<Path> const &libraries) -> bool {
  for (auto const &library : libraries) {
    const auto plain = codegen::UnfortifiedImport(utils::files::NativeString(library).c_str());
    if (plain == nullptr) { continue; }
    std::cerr
      << "Error: " << Shown(root, library) << " calls '" << plain << "' unchecked: it was built without "
      << "_FORTIFY_SOURCE.\n"
      << "  Rebuild it with -O2 -D_FORTIFY_SOURCE=3, or set 'fortified_ffi = false' under [binary.link] in "
      << "spp.toml.\n";
    return false;
  }
  return true;
}
