module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.types;
import genex;

SPP_MOD_BEGIN
CasePatternVariantAst::CasePatternVariantAst() :
  _MappedLet(nullptr) {
}

auto CasePatternVariantAst::Stage9_CompTimeResolve(
  ScopeManager *, CompilerMetaData *) -> void {
  // No behaviour but c++ module issues require this be
  // defined here (maybe GCC bug).
}

auto CasePatternVariantAst::BindsByMove() const -> bool {
  // Only the patterns that name something take anything;
  // every other kind is a test.
  return false;
}

auto CasePatternVariantAst::ConvToVar(
  CompilerMetaData *) -> Unique<LocalVariableAst> {
  // Default implementation for case pattern variants
  // that do not create variables.
  return nullptr;
}

auto CasePatternVariantAst::AnalyseDestructure(
  ExpressionAst const *cond, Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta) -> void {
  using analyse::utils::case_utils::CreateAndAnalysePatternEqFuncsDummyCore;
  // Create the new variable from the pattern, in the
  // pattern's scope.
  auto var = ConvToVar(meta);
  _MappedLet = MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(var), nullptr, nullptr, AstClone(cond));
  _MappedLet->Stage7_AnalyseSemantics(sm, meta);
  CreateAndAnalysePatternEqFuncsDummyCore(
    elems, sm, meta);
}

auto CasePatternVariantAst::ResolveDestructure(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm, CompilerMetaData *meta) const -> void {
  using analyse::utils::case_utils::CreateAndAnalysePatternEqCompTime;
  // Transform the pattern into comptime values: all of them
  // must be true for it to match.
  const auto comptime_transforms = CreateAndAnalysePatternEqCompTime(
    elems, sm, meta);
  const auto all_true = genex::all_of(
    comptime_transforms, [](auto const &x) { return x->template To<BooleanLiteralAst>()->IsTrue(); });

  // Resolve the "let" statement introducing the symbols,
  // then give the result.
  _MappedLet->Stage9_CompTimeResolve(sm, meta);
  const auto p = PosStart();
  meta->CmpResult = all_true ? BooleanLiteralAst::True(p) : BooleanLiteralAst::False(p);
}

SPP_MOD_END
