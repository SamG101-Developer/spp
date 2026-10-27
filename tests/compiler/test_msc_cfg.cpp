#include <spp/parse/macros.hpp>
#include "test_macros.hpp"

import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.compiler.cfg;
import spp.lex.lexer;
import spp.parse.parser_spp;
import genex;

// "!cfg" drops members whose condition the target and build mode do not meet, before analysis. The fixtures use
// mode="dev|rel" (always met) and mode="!dev|rel" (never met), so they hold on every host and in either suite mode.

TEST(TestCfg, test_value_matches_one_of_its_alternatives) {
  using spp::compiler::cfg::CfgValueMatches;
  EXPECT_TRUE(CfgValueMatches("linux", "linux"));
  EXPECT_FALSE(CfgValueMatches("linux", "windows"));
  EXPECT_TRUE(CfgValueMatches("linux|macos", "macos"));
  EXPECT_FALSE(CfgValueMatches("linux|macos", "windows"));
  EXPECT_FALSE(CfgValueMatches("lin", "linux"));
}

TEST(TestCfg, test_negated_value_matches_none_of_its_alternatives) {
  using spp::compiler::cfg::CfgValueMatches;
  EXPECT_TRUE(CfgValueMatches("!windows", "linux"));
  EXPECT_FALSE(CfgValueMatches("!windows", "windows"));
  EXPECT_FALSE(CfgValueMatches("!linux|macos", "macos"));
  EXPECT_TRUE(CfgValueMatches("!linux|macos", "windows"));
}

// The keys a condition is checked against are the compiler's own list, as conditions are applied before std's "cfg"
// annotation can be resolved: they must be its parameters, exactly, or one would accept a key the other refuses.
TEST(TestCfg, test_keys_are_the_annotations_parameters) {
  const auto path = std::filesystem::current_path() / "../../tests/test_outputs/vcs/std/src/std/annotations.spp";
  auto file = std::ifstream(path);
  ASSERT_TRUE(file.is_open()) << path;
  const auto code = spp::Str(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  const auto mod = INJECT_CODE(code, Parse);

  auto params = spp::Vec<spp::Str>();
  auto found = false;
  for (auto const &member : mod->Impl->Members) {
    auto const *const fn = member->To<spp::asts::FunctionPrototypeAst>();
    if (fn == nullptr or fn->Name->Val != "cfg") { continue; }
    found = true;
    for (auto const *const param : fn->FnParamGroup->GetAllParams()) { params.emplace_back(param->ExtractName()->Val); }
  }
  ASSERT_TRUE(found) << "no \"cfg\" annotation in " << path;

  auto const &keys = spp::compiler::cfg::CfgKeys();
  for (auto const &param : params) {
    EXPECT_TRUE(genex::find(keys, spp::StrView(param)) != keys.end()) << "std's \"cfg\" takes \"" << param
      << "\", which the compiler does not check";
  }
  for (auto const key : keys) {
    EXPECT_TRUE(genex::find(params, spp::Str(key)) != params.end()) << "the compiler checks \"" << key
      << "\", which std's \"cfg\" does not take";
  }
}

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCfg,
  test_valid_unmet_function_is_dropped, R"(
    !cfg(mode="!dev|rel")
    fun f() -> Void { no_such_function() }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_met_function_is_kept,
  SppIdentifierUnknownError, R"(
    !cfg(mode="dev|rel")
    fun f() -> Void { no_such_function() }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_every_key_must_be_met,
  SppIdentifierUnknownError, R"(
    !cfg(mode="dev|rel", pointer_width="16|32|64")
    fun f() -> Void { no_such_function() }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCfg,
  test_valid_one_unmet_key_drops, R"(
    !cfg(mode="dev|rel", pointer_width="!16|32|64")
    fun f() -> Void { no_such_function() }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCfg,
  test_valid_alias_chosen_by_condition, R"(
    !cfg(mode="dev|rel") type T = S32
    !cfg(mode="!dev|rel") type T = Bool
    fun f() -> T { ret 1_s32 }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCfg,
  test_valid_unmet_attribute_is_dropped, R"(
    cls A {
        x: S32
        !cfg(mode="!dev|rel") y: NoSuchType
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestCfg,
  test_valid_unmet_sup_method_is_dropped, R"(
    cls A { }
    sup A {
        !cfg(mode="!dev|rel")
        fun g(&self) -> Void { no_such_function() }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_unknown_key,
  SppCfgUnknownKeyError, R"(
    !cfg(planet="earth")
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_unknown_value,
  SppCfgInvalidValueError, R"(
    !cfg(os="no_such_os")
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_unknown_alternative,
  SppCfgInvalidValueError, R"(
    !cfg(mode="!dev|debug")
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_value_not_a_string,
  SppCfgInvalidValueError, R"(
    !cfg(pointer_width=64)
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestCfg,
  test_invalid_unmet_condition_still_checked,
  SppCfgInvalidValueError, R"(
    !cfg(mode="!dev|rel", os="no_such_os")
    fun f() -> Void { }
)");
