#include "test_macros.hpp"

import spp.compiler.out_layout;
import spp.utils.types;
import magic_enum;
import std;

// The names of the build modes and types are derived from the enums, and everything that takes one by name depends on
// them.

TEST(TestBuildMode, test_a_mode_is_named_in_lower_case) {
  EXPECT_EQ(spp::compiler::ModeName(spp::compiler::BuildMode::DEV), "dev");
  EXPECT_EQ(spp::compiler::ModeName(spp::compiler::BuildMode::REL), "rel");
}

TEST(TestBuildMode, test_every_mode_is_listed_in_declaration_order) {
  EXPECT_EQ(spp::compiler::ModeNames(), (spp::Vec<spp::Str>{"dev", "rel"}));
}

TEST(TestBuildMode, test_a_name_reads_back_as_its_mode) {
  for (const auto mode : magic_enum::enum_values<spp::compiler::BuildMode>()) {
    EXPECT_EQ(spp::compiler::ModeFromName(spp::compiler::ModeName(mode)), mode);
  }
}

TEST(TestBuildMode, test_a_mode_name_is_read_in_any_case) {
  EXPECT_EQ(spp::compiler::ModeFromName("REL"), spp::compiler::BuildMode::REL);
}

// "spp clean" relies on "all" naming no mode, which is how it comes to mean every mode.
TEST(TestBuildMode, test_a_name_that_is_not_a_mode_reads_as_nothing) {
  EXPECT_EQ(spp::compiler::ModeFromName("all"), spp::Nullopt);
  EXPECT_EQ(spp::compiler::ModeFromName("debug"), spp::Nullopt);
  EXPECT_EQ(spp::compiler::ModeFromName(""), spp::Nullopt);
}

TEST(TestBuildMode, test_the_out_folder_is_the_name_of_the_mode) {
  const auto out = spp::compiler::OutLayout{.Root = "root", .Target = "target", .Mode = spp::compiler::BuildMode::REL};
  EXPECT_EQ(out.OutRoot(), spp::Path("root") / "out" / "target" / "rel");
}

TEST(TestBuildType, test_a_type_is_named_in_lower_case) {
  EXPECT_EQ(spp::compiler::TypeName(spp::compiler::BuildType::EXE), "exe");
  EXPECT_EQ(spp::compiler::TypeName(spp::compiler::BuildType::LIB), "lib");
}

TEST(TestBuildType, test_every_type_is_listed_in_declaration_order) {
  EXPECT_EQ(spp::compiler::TypeNames(), (spp::Vec<spp::Str>{"exe", "lib"}));
}

TEST(TestBuildType, test_a_name_reads_back_as_its_type) {
  for (const auto type : magic_enum::enum_values<spp::compiler::BuildType>()) {
    EXPECT_EQ(spp::compiler::TypeFromName(spp::compiler::TypeName(type)), type);
  }
}

TEST(TestBuildType, test_a_type_name_is_read_in_any_case) {
  EXPECT_EQ(spp::compiler::TypeFromName("LIB"), spp::compiler::BuildType::LIB);
}

TEST(TestBuildType, test_a_name_that_is_not_a_type_reads_as_nothing) {
  EXPECT_EQ(spp::compiler::TypeFromName("nope"), spp::Nullopt);
  EXPECT_EQ(spp::compiler::TypeFromName(""), spp::Nullopt);
}
