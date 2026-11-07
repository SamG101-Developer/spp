module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
  decltype(Name) name,
  decltype(Val) &&val) :
  Name(std::move(name)),
  Val(std::move(val)) {
}

spp::asts::ObjectInitializerArgumentAst::~ObjectInitializerArgumentAst() = default;

auto spp::asts::ObjectInitializerArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Forward analysis into the value expression.
  Val->Stage7_AnalyseSemantics(sm, meta);
  common_errors::RequirePrimary(*Val, *sm);
}

auto spp::asts::ObjectInitializerArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory of the value expression.
  IMPORT_UTILS;
  Val->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*Val, *this, *sm, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Comptime resolve the value expression.
  Val->EvaluateCompTime(sm, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Infer the type of the value expression.
  return Val->InferType(sm, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Val->InferTypeRef(sm, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::IsAllowedInDefault() const -> bool {
  // Check the internal value of the argument.
  // Todo: Remove the nullptr guard?
  return Val == nullptr or Val->IsAllowedInDefault();
}

SPP_MOD_END
