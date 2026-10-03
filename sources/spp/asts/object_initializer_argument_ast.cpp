module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
  decltype(Name) name,
  decltype(Val) &&val) :
  Name(std::move(name)),
  Val(std::move(val)) {
}

ObjectInitializerArgumentAst::~ObjectInitializerArgumentAst() = default;

auto ObjectInitializerArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Forward analysis into the value expression.
  Val->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Val, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Val));
}

auto ObjectInitializerArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory of the value expression.
  IMPORT_UTILS;
  Val->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*Val, *this, *sm, meta);
}

auto ObjectInitializerArgumentAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Comptime resolve the value expression.
  Val->Stage9_CompTimeResolve(sm, meta);
}

auto ObjectInitializerArgumentAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Infer the type of the value expression.
  return Val->InferType(sm, meta);
}

auto ObjectInitializerArgumentAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Val->InferTypeRef(sm, meta);
}

auto ObjectInitializerArgumentAst::IsAllowedInDefault() const -> bool {
  // Check the internal value of the argument.
  // Todo: Remove the nullptr guard?
  return Val == nullptr or Val->IsAllowedInDefault();
}

SPP_MOD_END
