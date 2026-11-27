module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.case_pattern_variant_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.boolean_literal_ast;
import spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import genex;
import llvm;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts, CasePatternVariantAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

namespace spp::asts {
  /// Resolve the types written on "pattern" and the destructures in
  /// it: a nested destructure is matched through the mapped "let"
  /// rather than analysed as a pattern of its own, and would otherwise
  /// keep naming its type as written. "subject" is the type of what
  /// "pattern" takes apart (null when unknown), which an object
  /// pattern's unwritten generic arguments are taken from.
  static auto AnalyseNestedWrittenTypes(
    CasePatternVariantAst &pattern, Shared<TypeAst> const &subject,
    ScopeManager &sm, CompilerMetaData &meta) -> void;

  /// "AnalyseNestedWrittenTypes" for each of the elements of "parent",
  /// which takes apart a "parent_type".
  static auto AnalyseNestedElems(
    CasePatternVariantAst const &parent, Shared<TypeAst> const &parent_type,
    Span<Unique<CasePatternVariantAst> const> elems, ScopeManager &sm, CompilerMetaData &meta) -> void;

  /// The type of what element "index" of "parent" (taking apart a
  /// "parent_type") takes apart: an attribute binding's attribute, a
  /// tuple's element at its position (unknown past a ".."), an array's
  /// element. Null when unknown.
  static auto ElemSubject(
    CasePatternVariantAst const &parent, Shared<TypeAst> const &parent_type,
    CasePatternVariantAst const &elem, std::size_t index, bool after_rest, ScopeManager &sm) -> Shared<TypeAst>;
}

auto spp::asts::AnalyseNestedWrittenTypes(
  CasePatternVariantAst &pattern, Shared<TypeAst> const &subject,
  ScopeManager &sm, CompilerMetaData &meta) -> void {
  IMPORT_UTILS;
  using namespace spp::asts;
  if (const auto obj = pattern.To<CasePatternVariantDestructureObjectAst>()) {
    obj->Type = type_resolution::AnalyseInferredType(*obj->Type, {.Subject = subject}, sm, meta);
    AnalyseNestedElems(*obj, obj->Type, obj->Elems, sm, meta);
  }
  else if (const auto tup = pattern.To<CasePatternVariantDestructureTupleAst>()) {
    AnalyseNestedElems(*tup, subject, tup->Elems, sm, meta);
  }
  else if (const auto arr = pattern.To<CasePatternVariantDestructureArrayAst>()) {
    AnalyseNestedElems(*arr, subject, arr->Elems, sm, meta);
  }
  else if (const auto binding = pattern.To<CasePatternVariantDestructureAttributeBindingAst>();
    binding != nullptr and binding->Val != nullptr) {
    AnalyseNestedWrittenTypes(*binding->Val, subject, sm, meta);
  }
}

auto spp::asts::AnalyseNestedElems(
  CasePatternVariantAst const &parent, Shared<TypeAst> const &parent_type,
  const Span<Unique<CasePatternVariantAst> const> elems, ScopeManager &sm,
  CompilerMetaData &meta) -> void {

  // For each binding within the destructure pattern, get the binding element's
  // type and recursively analyse the elements (allowing for nested tuples,
  // arrays, object/variant destructuring).
  auto after_rest = false;
  for (auto i = 0uz; i < elems.size(); ++i) {
    after_rest = after_rest or elems[i]->To<CasePatternVariantDestructureSkipMultipleArgumentsAst>() != nullptr;
    AnalyseNestedWrittenTypes(*elems[i], ElemSubject(parent, parent_type, *elems[i], i, after_rest, sm), sm, meta);
  }
}

auto spp::asts::ElemSubject(
  CasePatternVariantAst const &parent, Shared<TypeAst> const &parent_type,
  CasePatternVariantAst const &elem, const std::size_t index,
  const bool after_rest, ScopeManager &sm) -> Shared<TypeAst> {
  IMPORT_UTILS;
  using namespace spp::asts;
  if (parent_type == nullptr) { return nullptr; }
  auto const &scope = *sm.CurrentScope;
  const auto ref = analyse::scopes::TypeRef::Of(*parent_type, scope).WithoutConvention();

  // For objects, where the binding being checked is an attribute binding, loop
  // through the field to find the match (by name), and return the field's type.
  if (parent.To<CasePatternVariantDestructureObjectAst>() != nullptr) {
    const auto binding = elem.To<CasePatternVariantDestructureAttributeBindingAst>();
    if (binding == nullptr or ref.Symbol == nullptr) { return nullptr; }
    for (auto const &[name, attr_ref, attr_scope] : type_members::GetAllAttrs(*ref.Symbol)) {
      if (*name == *binding->Name) { return attr_ref.AstIn(scope); }
    }
    return nullptr;
  }

  // For tuples, just get the nth type of the tuple element set, which is the
  // condition type in scope.
  if (parent.To<CasePatternVariantDestructureTupleAst>() != nullptr) {
    if (after_rest or not type_predicates::IsTypeTuple(ref, scope)) { return nullptr; }
    return type_members::GetNthTypeOfIndexableType(index, ref, scope).AstIn(scope);
  }

  // For arrays, just get the nth type of the tuple element set, which is the
  // condition type in scope.
  if (parent.To<CasePatternVariantDestructureArrayAst>() != nullptr) {
    if (not type_predicates::IsTypeArray(ref, scope)) { return nullptr; }
    return type_members::GetNthTypeOfIndexableType(0uz, ref, scope).AstIn(scope);
  }
  return nullptr;
}

SPP_MOD_BEGIN
spp::asts::CasePatternVariantAst::CasePatternVariantAst() :
  _MappedLet(nullptr) {
}

spp::asts::CasePatternVariantAst::~CasePatternVariantAst() = default;

auto spp::asts::CasePatternVariantAst::EvaluateCompTime(
  ScopeManager *, CompilerMetaData *) -> void {
  // No behaviour but c++ module issues require this be
  // defined here (maybe GCC bug).
}

auto spp::asts::CasePatternVariantAst::BindsByMove() const -> bool {
  // Only the patterns that name something take anything;
  // every other kind is a test.
  return false;
}

auto spp::asts::CasePatternVariantAst::ConvToVar(
  CompilerMetaData *) -> Unique<LocalVariableAst> {
  // Default implementation for case pattern variants
  // that do not create variables.
  return nullptr;
}

auto spp::asts::CasePatternVariantAst::AnalyseDestructure(
  ExpressionAst const *cond, const Span<Unique<CasePatternVariantAst> const> elems,
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // The nested destructures name their types resolved, as the mapped "let"
  // reads them: each from what it takes apart, an object pattern's attributes
  // or the condition's elements (a tuple or array pattern destructures the
  // condition itself).
  const auto obj = To<CasePatternVariantDestructureObjectAst>();
  AnalyseNestedElems(
    *this,
    obj != nullptr ? obj->Type : meta->CaseCondition->InferType(sm, meta),
    elems, *sm, *meta);

  // Create the new variable from the pattern, in the
  // pattern's scope. The literals it tests are the "case"'s
  // plan's ("PatternSolver").
  auto var = ConvToVar(meta);
  _MappedLet = MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(var), nullptr, nullptr, AstClone(cond));
  _MappedLet->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::CasePatternVariantAst::EvaluateCompTimeDestructure(
  ScopeManager *sm, CompilerMetaData *meta) const -> void {
  // Resolve the "let" statement introducing the symbols. The
  // literals the pattern tests are the "case"'s plan's.
  _MappedLet->EvaluateCompTime(sm, meta);
  meta->CompTimeResult = BooleanLiteralAst::True(PosStart());
}

auto spp::asts::CasePatternVariantAst::CodeGenDestructure(
  ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) const -> llvm::Value* {
  // Run the codegen on the transformed "let" ast to introduce
  // symbols into the llvm function. The literals the pattern
  // tests are the "case"'s plan's, so the shape always matches.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementPrecomputedValue = meta->LlvmCaseCondition;
  _MappedLet->Stage11_CodeGen(sm, meta, ctx);
  return llvm::ConstantInt::getTrue(*ctx->Context);
}

SPP_MOD_END
