#include "test_macros.hpp"

import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

// "AstCloneShared" gives a clone whose "enable_shared_from_this" base is set, whether the ast allocates its shared
// clone itself ("CloneShared", the type asts) or is cloned and wrapped as the type asked for (an identifier). A clone
// wrapped as a "Shared<Ast>" leaves a base below "Ast" unset, and "shared_from_this" on it throws "bad_weak_ptr".

TEST(TestAstClone, test_shared_clone_of_an_identifier_shares_from_this) {
  const auto ident = spp::MakeShared<spp::asts::IdentifierAst>(0uz, "x");
  const auto clone = spp::asts::AstCloneShared(ident);
  ASSERT_NE(clone, nullptr);
  EXPECT_NE(clone.get(), ident.get());
  EXPECT_EQ(clone->Val, "x");
  EXPECT_EQ(clone->shared_from_this().get(), clone.get());
}

TEST(TestAstClone, test_shared_clone_of_a_type_shares_from_this) {
  const spp::Shared<spp::asts::TypeAst> type = spp::asts::TypeIdentifierAst::FromString("Foo");
  const auto clone = spp::asts::AstCloneShared(type);
  ASSERT_NE(clone, nullptr);
  EXPECT_NE(clone.get(), type.get());
  EXPECT_EQ(clone->ToString(), "Foo");
  EXPECT_EQ(clone->shared_from_this().get(), clone.get());
}
