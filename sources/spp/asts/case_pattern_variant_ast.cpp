module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_resolution;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
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

CasePatternVariantAst::~CasePatternVariantAst() = default;

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
  ExpressionAst const *cond, Vec<CasePatternVariantAst*> const &elems,
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // The nested destructures name their types resolved, as the
  // mapped "let" reads them.
  for (auto *const elem : elems) { AnalyseNestedWrittenTypes(*elem, *sm, *meta); }

  // Create the new variable from the pattern, in the
  // pattern's scope. The literals it tests are the "case"'s
  // plan's ("PatternSolver").
  auto var = ConvToVar(meta);
  _MappedLet = MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(var), nullptr, nullptr, AstClone(cond));
  _MappedLet->Stage7_AnalyseSemantics(sm, meta);
}

auto CasePatternVariantAst::CompTimeResolveDestructure(
  ScopeManager *sm, CompilerMetaData *meta) const -> void {
  // Resolve the "let" statement introducing the symbols. The
  // literals the pattern tests are the "case"'s plan's.
  _MappedLet->Stage9_CompTimeResolve(sm, meta);
  meta->CompTimeResult = BooleanLiteralAst::True(PosStart());
}

auto CasePatternVariantAst::CodeGenDestructure(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) const -> llvm::Value* {
  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function. The literals the pattern
  // tests are the "case"'s plan's, so the shape always matches.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
  _MappedLet->Stage11_CodeGen(sm, meta, ctx);
  return llvm::ConstantInt::getTrue(*ctx->Context);
}

SPP_MOD_END
