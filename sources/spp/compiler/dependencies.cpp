module;
#include <spp/macros.hpp>

module spp.compiler.dependencies;
import spp.utils.files;
import genex;
import std;
import tomlpp;

namespace spp::compiler::dependencies {
  namespace {
    constexpr auto kConfigFile = "spp.toml";
    constexpr auto kVcsFolder = "vcs";
    constexpr auto kDefaultBranch = "master";
    constexpr auto kStdName = "std";

    /// A dependency, and who asked for it first, for an error.
    struct Request {
      Dependency Record;
      Str By;
    };

    /// Reports against the file as seen from here: every "spp.toml" of a walk
    /// has the same name, so the name alone would not say which one is wrong.
    auto Report(Path const &config, Str const &problem) -> void {
      std::cerr
        << "Error in " << utils::files::DisplayString(config.lexically_proximate(std::filesystem::current_path()))
        << ": " << problem << "\n";
    }

    /// The name is a folder under "vcs", so it cannot be allowed to name
    /// anywhere else ("../src").
    auto IsFolderName(StrView const name) -> bool {
      return not name.empty() and name.front() != '-' and not genex::any_of(name, [](const char c) {
        return not std::isalnum(static_cast<unsigned char>(c)) and c != '_' and c != '-';
      });
    }

    /// A value handed to git as an argument: one starting with "-" would be
    /// read as an option of git's ("--upload-pack=...") and run a command.
    auto IsGitArgument(StrView const value) -> bool {
      return not value.empty() and value.front() != '-';
    }

    /// One record of the "[vcs]" table, or nothing once each problem with it
    /// has been reported. Unknown keys are rejected, as they are everywhere
    /// else in the file: a mistyped "branch" would fetch the wrong code.
    auto ReadRecord(
      Str const &name, toml::node const &info, Path const &root, Path const &config) -> Opt<Dependency> {
      const auto record = info.as_table();
      if (record == nullptr) {
        Report(config, "[vcs] '" + name + "' must be a table of 'git' and an optional 'branch'");
        return Nullopt;
      }

      auto ok = true;
      if (not IsFolderName(name)) {
        Report(config, "[vcs] '" + name + "' is not a valid name: it can only hold letters, digits, '_' and '-'");
        ok = false;
      }

      for (auto [key, _] : *record) {
        if (key.str() == "git" or key.str() == "branch") { continue; }
        Report(config, "unknown key '" + Str(key.str()) + "' in [vcs] '" + name + "'; it accepts 'git' and 'branch'");
        ok = false;
      }

      const auto git = (*record)["git"].value<Str>();
      if (not git.has_value() or not IsGitArgument(*git)) {
        Report(config, "[vcs] '" + name + "' needs 'git', the repository to fetch it from");
        ok = false;
      }

      const auto has_branch = record->contains("branch");
      const auto branch = has_branch ? (*record)["branch"].value<Str>() : Opt<Str>(kDefaultBranch);
      if (not branch.has_value() or not IsGitArgument(*branch)) {
        Report(config, "[vcs] '" + name + "' has an invalid 'branch'");
        ok = false;
      }

      if (not ok) { return Nullopt; }
      return Dependency{.Name = name, .Git = *git, .Branch = *branch, .Folder = root / kVcsFolder / name};
    }

    /// [CHECKED]
    /// Walk what "root" declares, then what each of those declares, handing
    /// each dependency to "visit" once. Its own declarations are only read when
    /// "visit" answers true, as it may be what makes them exist.
    auto Walk(
      Path const &root,
      FuncRef<bool(Request const&)> visit) -> Opt<Vec<Dependency>> {
      // The "seen" map stores the name against the request. The "pending" list
      // contains ___.
      auto seen = Map<Str, Request>();
      auto pending = Vec<Request>();

      const auto read = [&](Path const &folder, Str const &by) -> bool {
        auto declared = Declared(folder);
        if (not declared.has_value()) { return false; }

        for (auto &record : *declared) {
          // Everything lands in the one "vcs" folder, whoever asked for it.
          // This flattened folder provides the single point of dependencies.
          record.Folder = root / kVcsFolder / record.Name;
          const auto [it, fresh] = seen.try_emplace(
            record.Name, Request{.Record = record, .By = by});

          // Once the new request has been "seen", if it was fresh, ie there
          // wasn't a record with the same name pre-existing, then mark it as
          // "pending" too.
          if (fresh) {
            pending.emplace_back(it->second);
            continue;
          }

          // If the pre-existing record has the same url and git branch, then
          // this is an identical dependency, so there is no issue.
          auto const &[first, first_by] = it->second;
          if (first.Git == record.Git and first.Branch == record.Branch) {
            continue;
          }

          // Otherwise, there is a same-name conflict, so log the error, and
          // return false; the entire walk fails if there is one error.
          std::cerr
            << "Error: the dependency '" << record.Name
            << "' is declared two ways:\n" << "  " << first_by << " declares "
            << first.Git << " (" << first.Branch << ")\n" << "  " << by
            << " declares " << record.Git << " (" << record.Branch << ")\n"
            << "  A dependency is one repository and branch across a whole "
               "project.\n";
          return false;
        }
        return true;
      };

      if (not read(root, "this project")) { return Nullopt; }

      // For each pending request, run the visitation helper first, followed by
      // the "read" (recursive search). The visitation helper can also filter
      // out pending modules, so visited may not equal pending.
      auto visited = Vec<Dependency>();
      for (auto i = 0uz; i < pending.size(); ++i) {
        const auto request = pending[i];
        if (not visit(request)) { continue; }

        const auto read_by = "the dependency '" + request.Record.Name + "'";
        if (not read(request.Record.Folder, read_by)) { return Nullopt; }
        visited.emplace_back(request.Record);
      }

      // Sort the dependencies for normalisation per different systems, and
      // return the visited list.
      genex::actions::sort(visited, {}, [](auto const &d) { return d.Name; });
      return visited;
    }

    /// [CHECKED]
    /// The name the "spp.toml" at "root" gives its own project.
    auto ProjectName(Path const &root) -> Str {
      try {
        const auto parsed = toml::parse_file(
          utils::files::NativeString(root / kConfigFile));
        return parsed["project"]["name"].value<Str>().value_or("");
      }
      catch (std::exception const &) { return ""; }
    }

    /// Check "package" declares the standard library. The prelude is added to
    /// every module but std's own, and it names std, so anything else would be
    /// reported at a prelude line nobody wrote, or as every std symbol missing.
    auto DeclaresStd(Path const &folder, Str const &package) -> bool {
      const auto declared = Declared(folder);
      if (not declared.has_value()) { return false; }
      if (genex::any_of(*declared, [](auto const &d) { return d.Name == kStdName; })) { return true; }
      std::cerr
        << "Error: " << package << " does not declare '" << kStdName << "', which the prelude needs.\n"
        << "  Add it under [vcs] in its 'spp.toml', on the same repository and branch as the rest of the project.\n";
      return false;
    }
  }
}

/// [CHECKED]
auto spp::compiler::dependencies::Declared(
  Path const &root) -> Opt<Vec<Dependency>> {
  // Determine the path for the "spp.toml" config path. If it doesn't exist
  // (failsafe), return an empty list.
  const auto config = root / kConfigFile;
  if (not std::filesystem::exists(config)) { return Vec<Dependency>(); }

  try {
    // Open the toml file, and read the [vcs] section. If it doesn't exist,
    // return the empty list.
    const auto parsed = toml::parse_file(utils::files::NativeString(config));
    if (not parsed.contains("vcs")) { return Vec<Dependency>(); }

    const auto vcs = parsed["vcs"].as_table();
    if (vcs == nullptr) {
      Report(config, "[vcs] must be a table");
      return Nullopt;
    }

    auto dependencies = Vec<Dependency>();
    auto ok = true;

    // For each entry in the [vcs] section, read the record. This creates an
    // optional "dependency" object (nullopt for invalid entries).
    for (auto [key, info] : *vcs) {
      auto record = ReadRecord(Str(key.str()), info, root, config);
      if (record.has_value()) { dependencies.emplace_back(std::move(*record)); }
      else { ok = false; }
    }
    if (not ok) { return Nullopt; }
    return dependencies;
  }
  catch (std::exception const &e) {
    Report(config, Str("could not read it: ") + e.what());
    return Nullopt;
  }
}

/// [CHECKED]
auto spp::compiler::dependencies::Resolve(
  Path const &root) -> Vec<Dependency> {
  // Walk the dependencies, with the visitation filter requiring the request's
  // folder name to be an actual folder.
  const auto visited = Walk(root, [](Request const &request) {
    return std::filesystem::is_directory(request.Record.Folder);
  });
  return visited.value_or(Vec<Dependency>());
}

/// [CHECKED]
auto spp::compiler::dependencies::Fetch(
  Path const &root, const FuncRef<bool(Dependency const&)> fetch_one) -> bool {
  // A dependency that cannot be fetched does not stop the others, so one run
  // reports every one that is unreachable.
  auto ok = true;
  const auto visited = Walk(root, [&](Request const &request) {
    const auto fetched = fetch_one(request.Record);
    ok = ok and fetched;
    return fetched;
  });
  return visited.has_value() and ok;
}

auto spp::compiler::dependencies::Validate(
  Path const &root) -> bool {
  auto ok = true;
  const auto visited = Walk(root, [&](Request const &request) {
    if (std::filesystem::is_directory(request.Record.Folder)) { return true; }
    std::cerr
      << "Error: the dependency '" << request.Record.Name << "', declared by " << request.By
      << ", has not been fetched into '" << kVcsFolder << "/" << request.Record.Name << "'.\n"
      << "  Run 'spp vcs' to fetch it.\n";
    ok = false;
    return false;
  });
  if (not visited.has_value()) { return false; }

  // Every package but std itself, which may be the project being built.
  if (ProjectName(root) != kStdName) { ok = DeclaresStd(root, "this project") and ok; }
  for (auto const &dependency : *visited) {
    if (dependency.Name == kStdName) { continue; }
    ok = DeclaresStd(dependency.Folder, "the dependency '" + dependency.Name + "'") and ok;
  }
  return ok;
}
