module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_call_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.LlvmMaterialize;
import spp.codegen.llvm_type;
import spp.utils.types;

SPP_MOD_BEGIN
namespace {
  /// An argument is its parameter's value: what the call's own
  /// result goes into ("let y = k(..)") is no target of the
  /// argument's, neither a type to check it against nor a slot to
  /// store it into. Cleared while this lives, and restored after;
  /// nothing is saved when no target is set.
  struct WithoutCallersTarget {
    explicit WithoutCallersTarget(CompilerMetaData *meta) {
      if (meta == nullptr or (
        meta->AssignmentTarget == nullptr
        and meta->AssignmentTargetType == nullptr
        and meta->LlvmAssignmentTarget == nullptr)) { return; }
      _Guard.emplace(meta);
      meta->AssignmentTarget = nullptr;
      meta->AssignmentTargetType = nullptr;
      meta->LlvmAssignmentTarget = nullptr;
    }

  private:
    spp::Opt<MetaGuard> _Guard;
  };
}

FunctionCallArgumentAst::FunctionCallArgumentAst(
  decltype(Conv) &&conv,
  decltype(Val) &&val,
  const utils::OrderableTag order_tag) :
  OrderableAst(order_tag),
  Conv(std::move(conv)),
  Val(std::move(val)) {
}

auto FunctionCallArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Analyse the semantics of the value expression.
  const auto _target_guard = WithoutCallersTarget(meta);
  Val->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Val, *sm),
    ERR_ARGS(*Val));
}

auto FunctionCallArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory status of the value expression.
  Val->Stage8_CheckMemory(sm, meta);
}

auto FunctionCallArgumentAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Delegate comptime resolution to the value expression.
  Val->Stage9_CompTimeResolve(sm, meta);
}

auto FunctionCallArgumentAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // An argument passed by value is generated as a value; a borrowed one lowers to the address of what it borrows.
  const auto _target_guard = WithoutCallersTarget(meta);
  if (Conv == nullptr) { return Val->Stage11_CodeGen(sm, meta, ctx); }
  return codegen::LlvmAddrOf(*Val, sm, meta, ctx);
}

auto FunctionCallArgumentAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Infer the type from the value expression.
  const auto _target_guard = WithoutCallersTarget(meta);
  auto type = Val->InferType(sm, meta);
  if (Conv) { type = type->WithConvention(AstClone(Conv)); }
  return type;
}

auto FunctionCallArgumentAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // Held as the argument says: a borrow of "!" is a real value.
  const auto _target_guard = WithoutCallersTarget(meta);
  auto ref = Val->InferTypeRef(sm, meta);
  if (Conv) {
    ref.Conv = Conv->Tag();
    ref.IsNever = false;
  }
  return ref;
}

auto FunctionCallArgumentAst::IsAllowedInDefault() const -> bool {
  // Check the internal value of the argument.
  // Todo: Remove the nullptr guard?
  return Val == nullptr or Val->IsAllowedInDefault();
}

SPP_MOD_END
