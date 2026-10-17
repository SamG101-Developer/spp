#include "../test_macros.hpp"

import spp.asts.convention_ast;
import spp.asts.gen_expression_ast;
import spp.asts.gen_with_expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.utils.ptr;

// An ast whose lowering moves a part out ("gen with" into a loop, "gen" into a return) still answers where it ends as
// written: an error can be raised at it, or a type positioned at it, after it is lowered.

namespace {
  using spp::asts::GenExpressionAst;
  using spp::asts::GenWithExpressionAst;
  using spp::asts::IdentifierAst;
}

TEST(TestAstPositions, test_gen_with_end_survives_its_expression_moving) {
  auto ast = GenWithExpressionAst(nullptr, nullptr, spp::MakeUnique<IdentifierAst>(10, "value"));
  const auto end = ast.PosEnd();
  const auto moved = std::move(ast.Expr);
  EXPECT_EQ(ast.PosEnd(), end);
}

TEST(TestAstPositions, test_gen_end_survives_its_expression_moving) {
  auto ast = GenExpressionAst(nullptr, nullptr, spp::MakeUnique<IdentifierAst>(10, "value"));
  const auto end = ast.PosEnd();
  const auto moved = std::move(ast.Expr);
  EXPECT_EQ(ast.PosEnd(), end);
}

TEST(TestAstPositions, test_gen_without_an_expression_ends_at_its_keyword) {
  const auto ast = GenExpressionAst(nullptr, nullptr, nullptr);
  EXPECT_EQ(ast.PosEnd(), ast.TokGen->PosEnd());
}
