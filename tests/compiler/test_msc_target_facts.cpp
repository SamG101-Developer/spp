#include "test_macros.hpp"

import spp.utils.features;

// A hardening measure the target cannot have: left out where it is on by default, refused where spp.toml sets it.

namespace {
  using spp::utils::features::ConfigKey;

  /// A spp.toml of its own under the test's temporary root, loaded, and removed with the features when the test ends.
  struct LoadedConfig {
    spp::Path Root = std::filesystem::temp_directory_path()
      / ("spp_target_facts_" + std::to_string(std::random_device()()));

    explicit LoadedConfig(const spp::StrView extra) {
      std::filesystem::create_directories(Root);
      std::ofstream(Root / "spp.toml") << "[project]\nname = \"p\"\nversion = \"0.1.0\"\nbuild = \"exe\"\n\n" << extra;
      auto errors = spp::Vec<spp::Str>();
      EXPECT_TRUE(spp::utils::features::Load(Root / "spp.toml", errors)) << (errors.empty() ? "" : errors.front());
    }

    ~LoadedConfig() {
      spp::utils::features::Reset();
      std::filesystem::remove_all(Root);
    }
  };

  constexpr auto WINDOWS = spp::utils::features::TargetFacts{
    .SafeStack = false, .SafeStackCanary = false, .PacRet = false, .IsLinux = false, .IsElf = false,
    .IsMachO = false, .IsGlibc = false, .Folder = "x86_64-windows-msvc"};

  constexpr auto MACOS = spp::utils::features::TargetFacts{
    .SafeStack = true, .SafeStackCanary = true, .PacRet = false, .IsLinux = false, .IsElf = false,
    .IsMachO = true, .IsGlibc = false, .Folder = "aarch64-macos"};

  constexpr auto LINUX_MUSL = spp::utils::features::TargetFacts{
    .SafeStack = true, .SafeStackCanary = true, .PacRet = false, .IsLinux = true, .IsElf = true,
    .IsMachO = false, .IsGlibc = false, .Folder = "x86_64-linux-musl"};

  constexpr auto LINUX_GNU = spp::utils::features::TargetFacts{
    .SafeStack = true, .SafeStackCanary = true, .PacRet = false, .IsLinux = true, .IsElf = true,
    .IsMachO = false, .IsGlibc = true, .Folder = "x86_64-linux-gnu"};
}

TEST(TestTargetFacts, test_default_split_and_pac_ret_left_out_without_an_unsafe_stack) {
  const auto config = LoadedConfig("");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(WINDOWS, errors));
  EXPECT_TRUE(errors.empty());
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackSplit));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryCodegenPacRet));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackProtect));
  const auto record = spp::utils::features::EffectiveSettings(false);
  EXPECT_NE(record.find("memory.stack.split = left out (no unsafe stack pointer for x86_64-windows-msvc)"),
    spp::Str::npos) << record;
}

TEST(TestTargetFacts, test_explicit_split_refused_without_an_unsafe_stack) {
  const auto config = LoadedConfig("[memory.stack]\nsplit = true\n");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_FALSE(spp::utils::features::CheckTargetSupport(WINDOWS, errors));
  ASSERT_EQ(errors.size(), 1U);
  EXPECT_NE(errors.front().find("set 'split = false'"), spp::Str::npos) << errors.front();
}

TEST(TestTargetFacts, test_explicit_pac_ret_kept_on_when_split_left_out) {
  const auto config = LoadedConfig("[binary.codegen]\npac_ret = true\n");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(WINDOWS, errors));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackSplit));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryCodegenPacRet)); // the signing pass refuses it
}

TEST(TestTargetFacts, test_default_protect_left_out_for_explicit_split_without_its_canary) {
  const auto config = LoadedConfig("[memory.stack]\nsplit = true\n");
  auto facts = WINDOWS;
  facts.SafeStack = true;
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(facts, errors));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackProtect));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackSplit));
}

TEST(TestTargetFacts, test_explicit_protect_and_split_refused_without_its_canary) {
  const auto config = LoadedConfig("[memory.stack]\nprotect = true\nsplit = true\n");
  auto facts = WINDOWS;
  facts.SafeStack = true;
  auto errors = spp::Vec<spp::Str>();
  EXPECT_FALSE(spp::utils::features::CheckTargetSupport(facts, errors));
  ASSERT_EQ(errors.size(), 1U);
  EXPECT_NE(errors.front().find("Set one to false"), spp::Str::npos) << errors.front();
}

TEST(TestTargetFacts, test_nothing_left_out_where_the_target_has_everything) {
  const auto config = LoadedConfig("");
  auto facts = WINDOWS;
  facts.SafeStack = facts.SafeStackCanary = true;
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(facts, errors));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackSplit));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryCodegenPacRet));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::MemoryStackProtect));
}

TEST(TestTargetFacts, test_default_ffi_checks_left_out_off_elf_but_dll_segments_kept) {
  const auto config = LoadedConfig("");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(WINDOWS, errors));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkFortifiedFfi));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkRelroFfi));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkSegmentsFfi));
  const auto record = spp::utils::features::EffectiveSettings(false);
  EXPECT_NE(record.find("binary.link.relro_ffi = left out (RELRO is ELF's, and x86_64-windows-msvc is not ELF)"),
    spp::Str::npos) << record;
}

TEST(TestTargetFacts, test_default_segments_ffi_left_out_on_macho) {
  const auto config = LoadedConfig("");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(MACOS, errors));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkSegmentsFfi));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkRelroFfi));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkFortifiedFfi));
}

TEST(TestTargetFacts, test_default_fortified_ffi_left_out_on_musl_only) {
  const auto config = LoadedConfig("");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(LINUX_MUSL, errors));
  EXPECT_FALSE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkFortifiedFfi));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkRelroFfi));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkSegmentsFfi));
}

TEST(TestTargetFacts, test_no_ffi_check_left_out_on_glibc) {
  const auto config = LoadedConfig("");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_TRUE(spp::utils::features::CheckTargetSupport(LINUX_GNU, errors));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkFortifiedFfi));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkRelroFfi));
  EXPECT_TRUE(spp::utils::features::IsEnabled(ConfigKey::BinaryLinkSegmentsFfi));
}

TEST(TestTargetFacts, test_explicit_ffi_check_refused_where_the_target_cannot_have_it) {
  const auto config = LoadedConfig("[binary.link]\nrelro_ffi = true\n");
  auto errors = spp::Vec<spp::Str>();
  EXPECT_FALSE(spp::utils::features::CheckTargetSupport(WINDOWS, errors));
  ASSERT_EQ(errors.size(), 1U);
  EXPECT_NE(errors.front().find("set 'relro_ffi = false'"), spp::Str::npos) << errors.front();
}
