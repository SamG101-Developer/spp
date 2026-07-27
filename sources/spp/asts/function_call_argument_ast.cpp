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
import spp.codegen.llvm_materialize;

SPP_MOD_BEGIN
spp::asts::FunctionCallArgumentAst::FunctionCallArgumentAst(
  decltype(Conv) &&conv,
  decltype(Val) &&val,
  const utils::OrderableTag order_tag) :
  OrderableAst(order_tag),
  Conv(std::move(conv)),
  Val(std::move(val)),
  _InjectedSelfType(nullptr) {
}

auto spp::asts::FunctionCallArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
  using analyse::errors::SppInvalidPrimaryExpressionError;

  // Analyse the semantics of the value expression.
  Val->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*Val, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Val));
}

auto spp::asts::FunctionCallArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Check the memory status of the value expression.
  Val->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::FunctionCallArgumentAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Delegate comptime resolution to the value expression.
  Val->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::FunctionCallArgumentAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // An argument passed by value is generated as a value; a borrowed one lowers to the address of what it borrows.
  if (Conv == nullptr) { return Val->Stage11_CodeGen(sm, meta, ctx); }
  return codegen::llvm_addr_of(*Val, sm, meta, ctx);
}

auto spp::asts::FunctionCallArgumentAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Infer the type from the value expression, unless an explicit "self" type has been given.
  if (_InjectedSelfType != nullptr) { return _InjectedSelfType; }

  auto type = Val->InferType(sm, meta);
  if (Conv) { type = type->WithConvention(AstClone(Conv)); }
  return type;
}

auto spp::asts::FunctionCallArgumentAst::SetSelfType(
  Shared<TypeAst> self_type)
  -> void {
  // Set the self type to the given type.
  _InjectedSelfType = std::move(self_type);
}

auto spp::asts::FunctionCallArgumentAst::GetSelfType()
  -> Shared<TypeAst> {
  // Get the self type.
  return _InjectedSelfType;
}

SPP_MOD_END
