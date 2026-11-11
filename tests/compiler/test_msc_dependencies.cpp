#include "test_macros.hpp"

import spp.asts.module_prototype_ast;
import spp.cli;
import spp.compiler.dependencies;
import spp.compiler.ffi;
import spp.compiler.module_tree;
import spp.compiler.out_layout;
import spp.lsp.resolution_index;
import spp.utils.files;
import spp.utils.types;
import genex;
import std;

namespace {
  constexpr auto kProject = "[project]\nname = \"p\"\nversion = \"0.1.0\"\nbuild = \"lib\"\n";
  constexpr auto kDepRecord = "[vcs]\na = { git = \"https://example.com/a\", branch = \"dev\" }\n";
  constexpr auto kStdRecord = "std = { git = \"https://example.com/std\" }\n";
  constexpr auto kSoundRecords =
    "[vcs]\nstd = { git = \"https://example.com/std\" }\na = { git = \"https://example.com/a\", branch = \"dev\" }\n";

  /// A project of its own under the test's temporary root, which is the current
  /// directory for as long as the test runs. "name" lets a test choose a folder
  /// with something to say about it ("vcs").
  struct ScratchProject {
    spp::Path Base = std::filesystem::weakly_canonical(std::filesystem::temp_directory_path())
      / ("spp dependencies " + std::to_string(std::random_device()()));
    spp::Path Root;
    spp::Path Previous = std::filesystem::current_path();

    explicit ScratchProject(spp::Path const &name = "p") : Root(Base / name) {
      std::filesystem::create_directories(Root / "src");
      std::filesystem::current_path(Root);
    }

    ~ScratchProject() {
      std::filesystem::current_path(Previous);
      std::filesystem::remove_all(Base);
    }

    auto Write(spp::Path const &relative, spp::Str const &content = "") const -> void {
      std::filesystem::create_directories((Root / relative).parent_path());
      std::ofstream(Root / relative) << content;
    }

    auto Config(spp::Str const &vcs) const -> void {
      Write("spp.toml", spp::Str(kProject) + vcs);
    }

    /// A fetched dependency, sound as far as validation asks: everything but
    /// std itself declares std.
    auto Fetched(spp::Str const &name) const -> void {
      const auto vcs = name == "std" ? spp::Str() : spp::Str("[vcs]\n") + kStdRecord;
      Write(spp::Path("vcs") / name / "spp.toml", kProject + vcs);
      std::filesystem::create_directories(Root / "vcs" / name / "src");
    }

    /// Each module's path under the root, as the tree reads them.
    auto Modules(spp::compiler::TestScope const &tests = {}) const -> spp::Vec<spp::Str> {
      auto tree = spp::compiler::ModuleTree(Root, spp::compiler::BuildMode::DEV, tests);
      auto paths = spp::Vec<spp::Str>();
      for (auto const *const m : tree.GetModules()) {
        paths.emplace_back(m->Path.lexically_relative(Root).generic_string());
      }
      return paths;
    }
  };

  auto IsRejected(spp::Str const &vcs) -> bool {
    const auto project = ScratchProject();
    project.Config(vcs);
    return not spp::compiler::dependencies::Declared(project.Root).has_value();
  }
}

TEST(Dependencies, test_declared_reads_a_record) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  ASSERT_EQ(declared->size(), 1uz);
  EXPECT_EQ((*declared)[0].Name, "a");
  EXPECT_EQ((*declared)[0].Git, "https://example.com/a");
  EXPECT_EQ((*declared)[0].Branch, "dev");
  EXPECT_EQ((*declared)[0].Folder, project.Root / "vcs" / "a");
}

TEST(Dependencies, test_declared_defaults_the_branch) {
  const auto project = ScratchProject();
  project.Config("[vcs]\na = { git = \"https://example.com/a\" }\n");
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  EXPECT_EQ((*declared)[0].Branch, "master");
}

TEST(Dependencies, test_declared_is_sorted_by_name) {
  const auto project = ScratchProject();
  project.Config("[vcs]\nb = { git = \"https://example.com/b\" }\na = { git = \"https://example.com/a\" }\n");
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  ASSERT_EQ(declared->size(), 2uz);
  EXPECT_EQ((*declared)[0].Name, "a");
  EXPECT_EQ((*declared)[1].Name, "b");
}

TEST(Dependencies, test_declared_is_empty_without_a_vcs_table) {
  const auto project = ScratchProject();
  project.Config("");
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  EXPECT_TRUE(declared->empty());
}

TEST(Dependencies, test_invalid_record_without_git) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { branch = \"dev\" }\n"));
}

TEST(Dependencies, test_invalid_record_with_an_unknown_key) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"https://example.com/a\", brnach = \"dev\" }\n"));  // spellchecker:disable-line
}

TEST(Dependencies, test_invalid_record_with_an_ns_key) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"https://example.com/a\", ns = \"b\" }\n"));
}

TEST(Dependencies, test_invalid_record_that_is_not_a_table) {
  EXPECT_TRUE(IsRejected("[vcs]\na = \"https://example.com/a\"\n"));
}

TEST(Dependencies, test_invalid_vcs_section_that_is_not_a_table) {
  const auto project = ScratchProject();
  project.Write("spp.toml", spp::Str("vcs = 1\n") + kProject);
  EXPECT_FALSE(spp::compiler::dependencies::Declared(project.Root).has_value());
}

TEST(Dependencies, test_invalid_name_that_leaves_the_vcs_folder) {
  EXPECT_TRUE(IsRejected("[vcs]\n\"../src\" = { git = \"https://example.com/a\" }\n"));
}

TEST(Dependencies, test_invalid_git_that_reads_as_a_git_option) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"--upload-pack=touch x\" }\n"));
}

TEST(Dependencies, test_invalid_branch_that_reads_as_a_git_option) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"https://example.com/a\", branch = \"--force\" }\n"));
}

TEST(Dependencies, test_invalid_branch_that_is_not_a_string) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"https://example.com/a\", branch = 1 }\n"));
}

namespace {
  constexpr auto kRev = "0123456789abcdef0123456789abcdef01234567";
  constexpr auto kOtherRev = "89abcdef0123456789abcdef0123456789abcdef";

  auto PinnedRecord(spp::Str const &name, spp::Str const &rev = kRev) -> spp::Str {
    return name + " = { git = \"https://example.com/" + name + "\", rev = \"" + rev + "\" }\n";
  }
}

TEST(Dependencies, test_declared_reads_a_pinned_record) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + PinnedRecord("a"));
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  ASSERT_EQ(declared->size(), 1uz);
  EXPECT_EQ((*declared)[0].Rev, spp::Opt<spp::Str>(kRev));
  EXPECT_EQ((*declared)[0].Branch, "");
}

TEST(Dependencies, test_declared_lowers_a_rev) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + PinnedRecord("a", "0123456789ABCDEF0123456789ABCDEF01234567"));
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  EXPECT_EQ((*declared)[0].Rev, spp::Opt<spp::Str>(kRev));
}

TEST(Dependencies, test_declared_reads_a_sha256_rev) {
  const auto project = ScratchProject();
  const auto rev = spp::Str(kRev) + "0123456789abcdef01234567";
  project.Config("[vcs]\n" + PinnedRecord("a", rev));
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  EXPECT_EQ((*declared)[0].Rev, spp::Opt<spp::Str>(rev));
}

TEST(Dependencies, test_declared_leaves_a_branch_record_unpinned) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  const auto declared = spp::compiler::dependencies::Declared(project.Root);
  ASSERT_TRUE(declared.has_value());
  EXPECT_FALSE((*declared)[0].Rev.has_value());
}

TEST(Dependencies, test_invalid_record_with_both_branch_and_rev) {
  EXPECT_TRUE(IsRejected(
    spp::Str("[vcs]\na = { git = \"https://example.com/a\", branch = \"dev\", rev = \"") + kRev + "\" }\n"));
}

TEST(Dependencies, test_invalid_rev_that_is_short) {
  EXPECT_TRUE(IsRejected("[vcs]\n" + PinnedRecord("a", "0123456")));
}

TEST(Dependencies, test_invalid_rev_that_is_not_hex) {
  EXPECT_TRUE(IsRejected("[vcs]\n" + PinnedRecord("a", "g123456789abcdef0123456789abcdef01234567")));
}

TEST(Dependencies, test_invalid_rev_that_reads_as_a_git_option) {
  EXPECT_TRUE(IsRejected("[vcs]\n" + PinnedRecord("a", "--upload-pack=touch-x-0123456789abcdef0")));
}

TEST(Dependencies, test_invalid_rev_that_is_not_a_string) {
  EXPECT_TRUE(IsRejected("[vcs]\na = { git = \"https://example.com/a\", rev = 1 }\n"));
}

TEST(Dependencies, test_find_checked_out_rev_reads_a_detached_head) {
  const auto project = ScratchProject();
  project.Write("vcs/a/.git/HEAD", "0123456789ABCDEF0123456789ABCDEF01234567\n");
  EXPECT_EQ(spp::compiler::dependencies::FindCheckedOutRev(project.Root / "vcs" / "a"), spp::Opt<spp::Str>(kRev));
}

TEST(Dependencies, test_find_checked_out_rev_is_empty_on_a_branch) {
  const auto project = ScratchProject();
  project.Write("vcs/a/.git/HEAD", "ref: refs/heads/master\n");
  EXPECT_FALSE(spp::compiler::dependencies::FindCheckedOutRev(project.Root / "vcs" / "a").has_value());
}

TEST(Dependencies, test_find_checked_out_rev_is_empty_without_a_checkout) {
  const auto project = ScratchProject();
  project.Fetched("a");
  EXPECT_FALSE(spp::compiler::dependencies::FindCheckedOutRev(project.Root / "vcs" / "a").has_value());
}

TEST(Dependencies, test_validate_accepts_a_dependency_at_its_pin) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + spp::Str(kStdRecord) + PinnedRecord("a"));
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/.git/HEAD", spp::Str(kRev) + "\n");
  EXPECT_TRUE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_validate_reports_a_dependency_away_from_its_pin) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + spp::Str(kStdRecord) + PinnedRecord("a"));
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/.git/HEAD", spp::Str(kOtherRev) + "\n");
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
  project.Write("vcs/a/.git/HEAD", "ref: refs/heads/master\n");
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_resolve_ignores_a_folder_that_is_not_declared) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Fetched("removed");
  const auto resolved = spp::compiler::dependencies::Resolve(project.Root);
  ASSERT_EQ(resolved.size(), 1uz);
  EXPECT_EQ(resolved[0].Name, "a");
}

TEST(Dependencies, test_resolve_skips_what_has_not_been_fetched) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  EXPECT_TRUE(spp::compiler::dependencies::Resolve(project.Root).empty());
}

TEST(Dependencies, test_validate_reports_what_has_not_been_fetched) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
  project.Fetched("a");
  EXPECT_TRUE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_module_tree_reads_only_a_dependencies_src) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("vcs/a/src/a/kept.spp");
  project.Write("vcs/a/src/main.spp");
  project.Write("vcs/a/tst/a/not_asked_for.spp");
  project.Write("vcs/a/examples/stray.spp");
  project.Write("vcs/a/vcs/b/src/b/nested.spp");
  EXPECT_EQ(project.Modules(), spp::Vec<spp::Str>{"vcs/a/src/a/kept.spp"});
}

TEST(Dependencies, test_module_tree_reads_a_dependencies_tst_when_asked) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("vcs/a/tst/a/kept.spp");
  project.Write("vcs/a/tst/main.spp");
  const auto modules = project.Modules(spp::compiler::TestScope{.Libs = {"a"}});
  EXPECT_TRUE(genex::contains(modules, spp::Str("vcs/a/tst/a/kept.spp")));
  EXPECT_FALSE(genex::contains(modules, spp::Str("vcs/a/tst/main.spp")));
}

TEST(Dependencies, test_module_tree_leaves_out_a_removed_dependency) {
  const auto project = ScratchProject();
  project.Config("");
  project.Fetched("removed");
  project.Write("vcs/removed/src/removed/gone.spp");
  EXPECT_TRUE(project.Modules().empty());
}

TEST(Dependencies, test_module_tree_accepts_a_dependency_with_no_tst_or_src) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  std::filesystem::create_directories(project.Root / "vcs" / "a");
  // A test build still has the harness it generates, and nothing else.
  EXPECT_EQ(project.Modules(spp::compiler::TestScope{.AllLibs = true}), spp::Vec<spp::Str>{"tst/main.spp"});
}

TEST(Dependencies, test_ffi_packages_leave_out_a_removed_dependency) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Fetched("removed");
  project.Write("vcs/a/ffi/kept/stub.spp");
  project.Write("vcs/removed/ffi/gone/stub.spp");
  const auto packages = spp::compiler::ffi::Packages(project.Root);
  ASSERT_EQ(packages.size(), 1uz);
  EXPECT_EQ(packages[0], project.Root / "vcs" / "a" / "ffi" / "kept");
}

TEST(Dependencies, test_validate_accepts_sound_dependencies) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  project.Fetched("a");
  EXPECT_TRUE(spp::cli::HandleValidate(false));
}

TEST(Dependencies, test_validate_fails_on_an_invalid_dependency) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  std::filesystem::create_directories(project.Root / "vcs" / "a" / "src");
  EXPECT_FALSE(spp::cli::HandleValidate(false));
  EXPECT_EQ(std::filesystem::current_path(), project.Root);
}

TEST(Dependencies, test_validate_fails_on_a_dependency_with_a_bad_record) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\nb = { branch = \"dev\" }\n");
  EXPECT_FALSE(spp::cli::HandleValidate(false));
}

TEST(Dependencies, test_validate_leaves_a_dependencies_checkout_clean) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  project.Fetched("a");
  EXPECT_TRUE(spp::cli::HandleValidate(false));
  EXPECT_FALSE(std::filesystem::exists(project.Root / "vcs" / "a" / "out"));
  EXPECT_FALSE(std::filesystem::exists(project.Root / "vcs" / "a" / "vcs"));
}

namespace {
  /// What each repository's "[vcs]" holds, written out as it is "fetched", and
  /// the order they were. Stands in for git, so the walk needs no network.
  struct FakeRemotes {
    ScratchProject const &Project;
    spp::Map<spp::Str, spp::Str> Vcs;
    spp::Vec<spp::Str> Fetched = {};
    spp::Set<spp::Str> Unreachable = {};

    auto Fetch() -> bool {
      return spp::compiler::dependencies::Fetch(Project.Root, [this](auto const &dependency) {
        Fetched.emplace_back(dependency.Name);
        if (Unreachable.contains(dependency.Name)) { return false; }
        Project.Write(spp::Path("vcs") / dependency.Name / "spp.toml", spp::Str(kProject) + Vcs[dependency.Name]);
        std::filesystem::create_directories(dependency.Folder / "src");
        return true;
      });
    }
  };

  auto Record(spp::Str const &name, spp::Str const &branch = "master") -> spp::Str {
    return name + " = { git = \"https://example.com/" + name + "\", branch = \"" + branch + "\" }\n";
  }

  auto Names(spp::Vec<spp::compiler::dependencies::Dependency> const &dependencies) -> spp::Vec<spp::Str> {
    auto names = spp::Vec<spp::Str>();
    for (auto const &dependency : dependencies) { names.emplace_back(dependency.Name); }
    return names;
  }
}

TEST(Dependencies, test_fetch_follows_what_a_dependency_declares) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}, {"b", "[vcs]\n" + Record("c")}}};
  EXPECT_TRUE(remotes.Fetch());
  EXPECT_EQ(remotes.Fetched, (spp::Vec<spp::Str>{"a", "b", "c"}));
}

TEST(Dependencies, test_fetch_terminates_on_a_cycle) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}, {"b", "[vcs]\n" + Record("a")}}};
  EXPECT_TRUE(remotes.Fetch());
  EXPECT_EQ(remotes.Fetched, (spp::Vec<spp::Str>{"a", "b"}));
}

TEST(Dependencies, test_fetch_shares_one_copy_of_a_dependency_declared_twice_alike) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + Record("b"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("c")}, {"b", "[vcs]\n" + Record("c")}}};
  EXPECT_TRUE(remotes.Fetch());
  EXPECT_EQ(remotes.Fetched, (spp::Vec<spp::Str>{"a", "b", "c"}));
}

TEST(Dependencies, test_invalid_fetch_of_a_dependency_on_two_branches) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + Record("b"));
  auto remotes = FakeRemotes{
    .Project = project, .Vcs = {{"a", "[vcs]\n" + Record("c", "one")}, {"b", "[vcs]\n" + Record("c", "two")}}};
  EXPECT_FALSE(remotes.Fetch());
}

TEST(Dependencies, test_fetch_shares_one_copy_of_a_dependency_pinned_twice_alike) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + PinnedRecord("c"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + PinnedRecord("c")}}};
  EXPECT_TRUE(remotes.Fetch());
  EXPECT_EQ(remotes.Fetched, (spp::Vec<spp::Str>{"a", "c"}));
}

TEST(Dependencies, test_invalid_fetch_of_a_dependency_at_two_revs) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + PinnedRecord("c"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + PinnedRecord("c", kOtherRev)}}};
  EXPECT_FALSE(remotes.Fetch());
}

TEST(Dependencies, test_invalid_fetch_of_a_dependency_pinned_and_on_a_branch) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + PinnedRecord("c"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("c")}}};
  EXPECT_FALSE(remotes.Fetch());
}

TEST(Dependencies, test_invalid_fetch_of_a_dependency_from_two_repositories) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + Record("c"));
  auto remotes = FakeRemotes{
    .Project = project, .Vcs = {{"a", "[vcs]\nc = { git = \"https://example.com/elsewhere\" }\n"}}};
  EXPECT_FALSE(remotes.Fetch());
}

TEST(Dependencies, test_invalid_fetch_of_a_dependency_with_a_bad_record) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\nb = { branch = \"dev\" }\n"}}};
  EXPECT_FALSE(remotes.Fetch());
}

TEST(Dependencies, test_fetch_carries_on_past_an_unreachable_dependency) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + Record("b"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {}, .Unreachable = {"a"}};
  EXPECT_FALSE(remotes.Fetch());
  EXPECT_EQ(remotes.Fetched, (spp::Vec<spp::Str>{"a", "b"}));
}

TEST(Dependencies, test_resolve_is_flat_and_sorted_whoever_declared_it) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("b"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"b", "[vcs]\n" + Record("a")}}};
  ASSERT_TRUE(remotes.Fetch());
  const auto resolved = spp::compiler::dependencies::Resolve(project.Root);
  EXPECT_EQ(Names(resolved), (spp::Vec<spp::Str>{"a", "b"}));
  EXPECT_EQ(resolved[0].Folder, project.Root / "vcs" / "a");
}

TEST(Dependencies, test_resolve_leaves_out_what_only_a_removed_dependency_declared) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}}};
  ASSERT_TRUE(remotes.Fetch());
  project.Config("");
  EXPECT_TRUE(spp::compiler::dependencies::Resolve(project.Root).empty());
}

TEST(Dependencies, test_validate_reports_a_transitive_dependency_not_fetched) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("std") + Record("a"));
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\n" + Record("std") + Record("b"));
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
  project.Fetched("b");
  project.Write("vcs/b/spp.toml", spp::Str(kProject) + "[vcs]\n" + Record("std"));
  EXPECT_TRUE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_validate_reports_a_dependency_declared_two_ways) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("std") + Record("a") + Record("c", "one"));
  project.Fetched("std");
  project.Fetched("a");
  project.Fetched("c");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\n" + Record("std") + Record("c", "two"));
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
  EXPECT_FALSE(spp::cli::HandleValidate(false));
}

TEST(Dependencies, test_validate_fails_on_an_invalid_transitive_dependency) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("std") + Record("a"));
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\n" + Record("std") + Record("b"));
  std::filesystem::create_directories(project.Root / "vcs" / "b" / "src");
  EXPECT_FALSE(spp::cli::HandleValidate(false));
}

TEST(Dependencies, test_module_tree_reads_a_transitive_dependency) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}}};
  ASSERT_TRUE(remotes.Fetch());
  project.Write("vcs/b/src/b/kept.spp");
  auto tree = spp::compiler::ModuleTree(project.Root);
  const auto modules = tree.GetModules();
  ASSERT_EQ(modules.size(), 1uz);
  EXPECT_EQ(modules[0]->Owner, spp::Str("b"));
  EXPECT_EQ(modules[0]->NsParts, (spp::Vec<spp::Str>{"b", "kept"}));
}

TEST(Dependencies, test_module_tree_reads_a_transitive_dependencies_tst_when_asked) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}}};
  ASSERT_TRUE(remotes.Fetch());
  project.Write("vcs/b/tst/b/kept.spp");
  EXPECT_TRUE(genex::contains(project.Modules(spp::compiler::TestScope{.Libs = {"b"}}), spp::Str("vcs/b/tst/b/kept.spp")));
  EXPECT_TRUE(genex::contains(project.Modules(spp::compiler::TestScope{.AllLibs = true}), spp::Str("vcs/b/tst/b/kept.spp")));
  EXPECT_TRUE(project.Modules().empty());
}

TEST(Dependencies, test_ffi_packages_include_a_transitive_dependencies) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  auto remotes = FakeRemotes{.Project = project, .Vcs = {{"a", "[vcs]\n" + Record("b")}}};
  ASSERT_TRUE(remotes.Fetch());
  project.Write("vcs/b/ffi/kept/stub.spp");
  const auto packages = spp::compiler::ffi::Packages(project.Root);
  ASSERT_EQ(packages.size(), 1uz);
  EXPECT_EQ(packages[0], project.Root / "vcs" / "b" / "ffi" / "kept");
}

TEST(Dependencies, test_invalid_project_without_std) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_invalid_dependency_without_std) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  project.Fetched("a");
  project.Write("vcs/a/spp.toml", kProject);
  EXPECT_FALSE(spp::compiler::dependencies::Validate(project.Root));
}

TEST(Dependencies, test_std_itself_need_not_declare_std) {
  const auto project = ScratchProject();
  project.Write("spp.toml", "[project]\nname = \"std\"\nversion = \"0.1.0\"\nbuild = \"lib\"\n");
  EXPECT_TRUE(spp::compiler::dependencies::Validate(project.Root));
}

namespace {
  /// Whether a tree over the project as it stands passes "CheckNamespaces".
  auto NamespacesAreSound(ScratchProject const &project) -> bool {
    return spp::compiler::ModuleTree(project.Root).CheckNamespaces();
  }
}

TEST(Dependencies, test_namespaces_one_package_across_src_and_ffi_packages) {
  const auto project = ScratchProject();
  project.Config(kSoundRecords);
  project.Fetched("std");
  project.Fetched("a");
  project.Write("src/main.spp");
  project.Write("src/mine/x.spp");
  project.Write("vcs/a/src/a/x.spp");
  project.Write("vcs/a/src/a/y.spp");
  project.Write("vcs/a/ffi/a_ffi/stub.spp");
  EXPECT_TRUE(NamespacesAreSound(project));
}

TEST(Dependencies, test_invalid_namespace_shared_by_two_dependencies) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a") + Record("b"));
  project.Fetched("a");
  project.Fetched("b");
  project.Write("vcs/a/src/shared/x.spp");
  project.Write("vcs/b/src/shared/y.spp");
  EXPECT_FALSE(NamespacesAreSound(project));
}

TEST(Dependencies, test_invalid_namespace_shared_with_the_project) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("src/shared/x.spp");
  project.Write("vcs/a/src/shared/y.spp");
  EXPECT_FALSE(NamespacesAreSound(project));
}

TEST(Dependencies, test_invalid_namespace_shared_with_another_packages_ffi) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("src/shared/x.spp");
  project.Write("vcs/a/ffi/shared/stub.spp");
  EXPECT_FALSE(NamespacesAreSound(project));
}

TEST(Dependencies, test_invalid_namespace_shared_with_its_own_ffi) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("vcs/a/src/shared/x.spp");
  project.Write("vcs/a/ffi/shared/stub.spp");
  EXPECT_FALSE(NamespacesAreSound(project));
}

TEST(Dependencies, test_invalid_project_declared_by_its_own_dependency) {
  const auto project = ScratchProject();
  project.Config(kDepRecord);
  project.Fetched("a");
  project.Write("src/mine/x.spp");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\nmine = { git = \"https://example.com/mine\" }\n");
  project.Write("vcs/mine/src/mine/x.spp");
  EXPECT_FALSE(NamespacesAreSound(project));
}

TEST(Dependencies, test_modules_know_what_their_package_declares) {
  const auto project = ScratchProject();
  project.Config("[vcs]\n" + Record("a"));
  project.Fetched("a");
  project.Write("vcs/a/spp.toml", spp::Str(kProject) + "[vcs]\n" + Record("b"));
  project.Fetched("b");
  project.Write("src/mine/x.spp");
  project.Write("vcs/a/src/a/x.spp");
  project.Write("vcs/b/src/b/x.spp");
  auto tree = spp::compiler::ModuleTree(project.Root);
  const auto declares_of = [&](spp::Str const &root) {
    for (auto const *const m : tree.GetModules()) {
      if (m->NsParts.front() == root) { return m->Declares; }
    }
    return spp::Vec<spp::Str>{"none"};
  };
  EXPECT_EQ(declares_of("mine"), spp::Vec<spp::Str>{"a"});
  EXPECT_EQ(declares_of("a"), spp::Vec<spp::Str>{"b"});
  EXPECT_EQ(declares_of("b"), spp::Vec<spp::Str>{"std"});
}

TEST(Dependencies, test_google_test_code_replaces_main_when_it_is_not_the_first_module) {
  const auto project = ScratchProject();
  project.Config("");
  project.Write("src/a.spp", "first");
  project.Write("src/main.spp", "second");
  const auto tree = spp::compiler::ModuleTree::ForCppGoogleTest(
    project.Root, spp::compiler::BuildMode::DEV, "replaced");
  const auto modules = tree->GetModules();
  ASSERT_EQ(modules.size(), 2uz);
  EXPECT_EQ(modules[0]->Path, project.Root / "src" / "a.spp");
  EXPECT_EQ(modules[0]->Code, "first");
  EXPECT_EQ(modules[1]->Code, "replaced");
}

TEST(Dependencies, test_indexing_a_project_inside_a_folder_named_vcs) {
  const auto project = ScratchProject("vcs/p");
  const auto wants = [&](spp::Path const &path) {
    // Native separators throughout, as the paths the module tree globs have.
    return spp::lsp::resolution_index::Wants(spp::utils::files::DisplayString((project.Root / path).make_preferred()));
  };
  spp::lsp::resolution_index::EnableProject();
  EXPECT_TRUE(wants("src/main.spp"));
  EXPECT_FALSE(wants("vcs/std/src/std/number.spp"));
  spp::lsp::resolution_index::Disable();
}
