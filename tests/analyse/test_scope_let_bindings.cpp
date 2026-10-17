#include "../test_macros.hpp"

import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.asts.identifier_ast;
import spp.utils.types;

// A walk entering a scope hides its "let" bindings, and each "let" declares its own again where it is written; leaving
// puts them all back, so the scope reads as Stage 7 left it. These drive "Scope" directly, because what a scope reads
// as after a walk is not visible to a program: every walk hides the bindings again on entry.

namespace {
  auto MakeLet(spp::Str const &name, spp::analyse::scopes::Scope &scope)
    -> spp::Shared<spp::analyse::scopes::VariableSymbol> {
    auto ident = spp::MakeShared<spp::asts::IdentifierAst>(0uz, name);
    auto sym = spp::MakeShared<spp::analyse::scopes::VariableSymbol>(
      ident, nullptr, &scope, spp::analyse::scopes::VariableKind::Local);
    scope.RecordLetBinding(sym);
    return sym;
  }
}

TEST(TestScopeLetBindings, test_reveal_after_full_walk_reads_final_shadow) {
  using spp::analyse::scopes::Scope;
  using spp::analyse::scopes::ScopeBlockName;
  auto scope = Scope(ScopeBlockName::FromParts("block", {}, 0uz), nullptr);

  // Stage 7: "let x = 1; let x = 2", the second filed over the first.
  const auto first = MakeLet("x", scope);
  const auto second = MakeLet("x", scope);
  scope.AddVarSymbol(first);
  scope.AddVarSymbol(second);

  // A walk that reaches the end declares both again, in order.
  scope.HideLetBindings();
  EXPECT_EQ(scope.InternalTable.VarTable.Find(first->Name.get()), nullptr);
  scope.AddVarSymbol(first);
  scope.AddVarSymbol(second);
  scope.RevealLetBindings();
  EXPECT_EQ(scope.InternalTable.VarTable.Find(first->Name.get()), second.get());
}

// Red: a walk leaving part-way between two shadows of a name (a comp-time "ret" in Stage 9 stops the walk there) leaves
// the earlier one filed, and "RevealLetBindings" only fills names that are empty, so the scope goes on reading the
// earlier binding instead of the last one declared. See docs/next-steps.md N6.15.
TEST(TestScopeLetBindings, test_reveal_after_partial_walk_reads_final_shadow) {
  using spp::analyse::scopes::Scope;
  using spp::analyse::scopes::ScopeBlockName;
  auto scope = Scope(ScopeBlockName::FromParts("block", {}, 0uz), nullptr);

  const auto first = MakeLet("x", scope);
  const auto second = MakeLet("x", scope);
  scope.AddVarSymbol(first);
  scope.AddVarSymbol(second);

  // The walk declares the first "x" and leaves before the second.
  scope.HideLetBindings();
  scope.AddVarSymbol(first);
  scope.RevealLetBindings();
  EXPECT_EQ(scope.InternalTable.VarTable.Find(first->Name.get()), second.get());
}
