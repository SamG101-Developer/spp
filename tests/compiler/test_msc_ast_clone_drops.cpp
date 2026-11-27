#include "test_macros.hpp"

import spp.analyse.utils.drop_utils;
import spp.asts._all;
import spp.asts.utils.ast_utils;
import spp.lex.lexer;
import spp.parse.parser_spp;

// Each ast holding drops keeps them in its own "_Drops", and its "Clone" copies them: some clones are generated
// without being analysed again (a "case" passed on as a method's receiver, a fold's calls), so a list the clone left
// behind is a value never dropped. Each test fills every list of one ast with values of its own and clones it.

namespace {
  using spp::analyse::utils::drop_utils::OwnedValue;

  /// A list of one value, told apart from the others by where it is declared.
  auto Owned(const std::size_t at) -> spp::Vec<OwnedValue> {
    auto out = spp::Vec<OwnedValue>();
    out.emplace_back(OwnedValue{.Name = spp::MakeShared<spp::asts::IdentifierAst>(at, "v"), .DeclaredAt = at});
    return out;
  }

  auto DeclaredAt(spp::Vec<OwnedValue> const &owned) -> spp::Vec<std::size_t> {
    auto out = spp::Vec<std::size_t>();
    for (auto const &v : owned) { out.emplace_back(v.DeclaredAt.value_or(0uz)); }
    return out;
  }

  auto Parse(spp::Str const &code, auto &&method) {
    auto parser = spp::parse::ParserSpp(spp::lex::Lexer(code).Lex());
    auto ast = method(parser);
    EXPECT_NE(ast, nullptr) << "\"" << code << "\" did not parse";
    return ast;
  }
}

#define EXPECT_SAME_DROPS(clone, ast, field) \
  EXPECT_EQ(DeclaredAt((clone)->_Drops.field), DeclaredAt((ast)->_Drops.field)) << #field " was not cloned"

TEST(TestAstCloneDrops, test_loop_conditional) {
  const auto ast = Parse("loop true { }", [](auto &p) { return p.ParseLoopConditionalExpression(); });
  ast->_Drops.Entry = Owned(1);
  ast->_Drops.BackEdge = Owned(2);
  ast->_Drops.CondTemporaries = Owned(3);
  ast->_Drops.BadCond = Owned(4);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, Entry);
  EXPECT_SAME_DROPS(clone, ast, BackEdge);
  EXPECT_SAME_DROPS(clone, ast, CondTemporaries);
  EXPECT_SAME_DROPS(clone, ast, BadCond);
}

TEST(TestAstCloneDrops, test_loop_control_flow) {
  const auto ast = Parse("skip", [](auto &p) { return p.ParseSkipStatement(); });
  ast->_Drops.AfterJump = Owned(1);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, AfterJump);
}

TEST(TestAstCloneDrops, test_binary_expression) {
  const auto parsed = Parse("a and b", [](auto &p) { return p.ParseBinaryExpression(); });
  auto *const ast = parsed->To<spp::asts::BinaryExpressionAst>();
  ASSERT_NE(ast, nullptr);
  ast->_Drops.ShortCircuit = Owned(1);
  ast->_Drops.AfterRhs = Owned(2);
  ast->_Drops.RhsTemporaries = Owned(3);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, ShortCircuit);
  EXPECT_SAME_DROPS(clone, ast, AfterRhs);
  EXPECT_SAME_DROPS(clone, ast, RhsTemporaries);
}

TEST(TestAstCloneDrops, test_case_and_branch) {
  const auto ast = Parse("case x { 1 } else { 2 }", [](auto &p) { return p.ParseCaseExpression(); });
  ASSERT_FALSE(ast->Branches.empty());
  ast->_Drops.FallThrough = Owned(1);
  auto &branch = *ast->Branches.front();
  branch._Drops.End = Owned(2);
  branch._Drops.BadMatch = Owned(3);
  branch._Drops.BadGuard = Owned(4);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, FallThrough);
  EXPECT_SAME_DROPS(clone->Branches.front(), &branch, End);
  EXPECT_SAME_DROPS(clone->Branches.front(), &branch, BadMatch);
  EXPECT_SAME_DROPS(clone->Branches.front(), &branch, BadGuard);
}

TEST(TestAstCloneDrops, test_function_call) {
  const auto parsed = Parse("f(1)", [](auto &p) { return p.ParsePostfixExpression(); });
  auto *const postfix = parsed->To<spp::asts::PostfixExpressionAst>();
  ASSERT_NE(postfix, nullptr);
  auto *const ast = postfix->Op->To<spp::asts::PostfixExpressionOperatorFunctionCallAst>();
  ASSERT_NE(ast, nullptr);
  ast->_Drops.Fold = Owned(1);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, Fold);
}

TEST(TestAstCloneDrops, test_inner_scope) {
  const auto loop = Parse("loop true { 1\n 2 }", [](auto &p) { return p.ParseLoopConditionalExpression(); });
  auto *const ast = loop->Body.get();
  ast->_Drops.End = Owned(1);
  ast->_Drops.StatementTemporaries = {Owned(2), Owned(3)};
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, End);
  ASSERT_EQ(clone->_Drops.StatementTemporaries.size(), 2uz);
  EXPECT_EQ(DeclaredAt(clone->_Drops.StatementTemporaries[0]), DeclaredAt(ast->_Drops.StatementTemporaries[0]));
  EXPECT_EQ(DeclaredAt(clone->_Drops.StatementTemporaries[1]), DeclaredAt(ast->_Drops.StatementTemporaries[1]));
}

TEST(TestAstCloneDrops, test_closure) {
  const auto ast = Parse("|| 1", [](auto &p) { return p.ParseClosureExpression(); });
  ast->_Drops.End = Owned(1);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, End);
}

TEST(TestAstCloneDrops, test_subroutine_prototype) {
  const auto ast = Parse("fun f() -> Void { }", [](auto &p) { return p.ParseSubroutinePrototype(); });
  ast->_Drops.End = Owned(1);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, End);
}

TEST(TestAstCloneDrops, test_coroutine_prototype) {
  const auto ast = Parse("cor g() -> Gen[U32] { }", [](auto &p) { return p.ParseCoroutinePrototype(); });
  ast->_Drops.End = Owned(1);
  const auto clone = spp::asts::AstClone(ast);
  EXPECT_SAME_DROPS(clone, ast, End);
}

TEST(TestAstCloneDrops, test_assignment) {
  const auto ast = Parse("a, b = 1, 2", [](auto &p) { return p.ParseAssignmentStatement(); });
  ast->_Drops.Overwrite = {spp::Nullopt, Owned(1).front()};
  const auto clone = spp::asts::AstClone(ast);
  ASSERT_EQ(clone->_Drops.Overwrite.size(), 2uz);
  EXPECT_FALSE(clone->_Drops.Overwrite[0].has_value());
  ASSERT_TRUE(clone->_Drops.Overwrite[1].has_value());
  EXPECT_EQ(clone->_Drops.Overwrite[1]->DeclaredAt, 1uz);
}
