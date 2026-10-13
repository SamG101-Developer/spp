module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.type_resolution;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import genex;
import llvm;

namespace {
  /// Resolve the types written on "pattern" and the destructures in
  /// it: a nested destructure is matched through the mapped "let"
  /// rather than analysed as a pattern of its own, and would otherwise
  /// keep naming its type as written.
  auto AnalyseNestedWrittenTypes(
    CasePatternVariantAst &pattern, ScopeManager &sm,
    CompilerMetaData &meta) -> void {
    using namespace spp::asts;
    if (auto obj = pattern.To<CasePatternVariantDestructureObjectAst>()) {
      obj->Type = spp::analyse::utils::type_resolution::AnalyseWrittenType(*obj->Type, sm, meta);
      for (auto const &elem : obj->Elems) { AnalyseNestedWrittenTypes(*elem, sm, meta); }
    }
    else if (auto tup = pattern.To<CasePatternVariantDestructureTupleAst>()) {
      for (auto const &elem : tup->Elems) { AnalyseNestedWrittenTypes(*elem, sm, meta); }
    }
    else if (auto arr = pattern.To<CasePatternVariantDestructureArrayAst>()) {
      for (auto const &elem : arr->Elems) { AnalyseNestedWrittenTypes(*elem, sm, meta); }
    }
    else if (auto binding = pattern.To<CasePatternVariantDestructureAttributeBindingAst>();
      binding != nullptr and binding->Val != nullptr) {
      AnalyseNestedWrittenTypes(*binding->Val, sm, meta);
    }
  }
}

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

auto CasePatternVariantAst::NarrowsTo() const -> TypeAst const* {
  // Only a destructure naming a type selects one.
  return nullptr;
}

auto CasePatternVariantAst::ConvToVar(
  CompilerMetaData *) -> Unique<LocalVariableAst> {
  // Default implementation for case pattern variants
  // that do not create variables.
  return nullptr;
}

auto CasePatternVariantAst::AnalyseDestructure(
  ExpressionAst const *cond, Vec<CasePatternVariantAst*> const &elems,
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // The nested destructures name their types resolved, as the
  // mapped "let" reads them.
  for (auto *const elem : elems) { AnalyseNestedWrittenTypes(*elem, *sm, *meta); }

  // Create the new variable from the pattern, in the
  // pattern's scope.
  auto var = ConvToVar(meta);
  _MappedLet = MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(var), nullptr, nullptr, AstClone(cond));
  _MappedLet->Stage7_AnalyseSemantics(sm, meta);
  case_utils::CreateAndAnalysePatternEqFnsDummyCore(
    elems, sm, meta);
}

auto CasePatternVariantAst::CompTimeResolveDestructure(
  Vec<CasePatternVariantAst*> const &elems,
  ScopeManager *sm, CompilerMetaData *meta) const -> void {
  IMPORT_UTILS;
  // Transform the pattern into comptime values: all of them
  // must be true for it to match.
  const auto comptime_transforms = case_utils::CreateAndAnalysePatternEqCompTime(
    elems, sm, meta);
  const auto all_true = genex::all_of(
    comptime_transforms, [](auto const &x) { return x->template To<BooleanLiteralAst>()->IsTrue(); });

  // Resolve the "let" statement introducing the symbols,
  // then give the result.
  _MappedLet->Stage9_CompTimeResolve(sm, meta);
  const auto p = PosStart();
  meta->CompTimeResult = all_true ? BooleanLiteralAst::True(p) : BooleanLiteralAst::False(p);
}

auto CasePatternVariantAst::CodeGenDestructure(
  Vec<CasePatternVariantAst*> const &elems, ScopeManager *sm,
  CompilerMetaData *meta, codegen::LlvmCtx *ctx) const -> llvm::Value* {
  IMPORT_UTILS;

  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function.
  if (_MappedLet != nullptr) {
    const auto _meta_guard = MetaGuard(meta);
    meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
    _MappedLet->Stage11_CodeGen(sm, meta, ctx);
  }

  // Combine all the generated transforms into a single "AND"ed
  // expression.
  auto llvm_transforms = case_utils::CreateAndAnalysePatternEqFnsLlvm(
    elems, sm, meta, ctx);

  const auto AND = [&ctx](auto a, auto b) { return ctx->Builder.CreateAnd(a, b); };
  const auto llvm_master_transform = llvm_transforms.IsEmpty()
    ? llvm::cast<llvm::Value>(llvm::ConstantInt::getTrue(*ctx->Context))
    : genex::fold_left_first(llvm_transforms, std::move(AND));

  // Return the combined expression back to the branch who owns
  // this pattern.
  return llvm_master_transform;
}

SPP_MOD_END
