module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_parameter_optional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::FunctionParameterOptionalAst::FunctionParameterOptionalAst(
  decltype(Var) &&var,
  decltype(TokColon) &&tok_colon,
  decltype(Type) type,
  decltype(TokAssign) &&tok_assign,
  decltype(DefaultVal) &&default_val) :
  FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type), utils::OrderableTag::kOptionalParam),
  TokAssign(std::move(tok_assign)),
  DefaultVal(std::move(default_val)) {
  Source.OriginalDefaultVal = AstClone(DefaultVal);
}

spp::asts::FunctionParameterOptionalAst::~FunctionParameterOptionalAst() = default;

auto spp::asts::FunctionParameterOptionalAst::PosStart() const
  -> std::size_t {
  // Use the variable.
  return Var->PosStart();
}

auto spp::asts::FunctionParameterOptionalAst::PosEnd() const
  -> std::size_t {
  // Use the default value.
  return DefaultVal->PosEnd();
}

auto spp::asts::FunctionParameterOptionalAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<FunctionParameterOptionalAst>(
    AstClone(Var),
    AstClone(TokColon),
    AstCloneShared(Type),
    AstClone(TokAssign),
    AstClone(DefaultVal));
  ast->Source.OriginalDefaultVal = AstClone(Source.OriginalDefaultVal);
  return ast;
}

auto spp::asts::FunctionParameterOptionalAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Var);
  SPP_STRING_APPEND(TokColon).append(" ");
  SPP_STRING_APPEND(Type).append(" ");
  SPP_STRING_APPEND(TokAssign).append(" ");
  SPP_STRING_APPEND(DefaultVal);
  SPP_STRING_END;
}

auto spp::asts::FunctionParameterOptionalAst::Stage6_PreAnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Perform default analysis steps.
  using analyse::errors::SppTypeMismatchError;
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::errors::SppInvalidDefaultValueError;
  using analyse::utils::type_compare::TypeEq;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

  if (_DefaultAnalysed) { return; }
  FunctionParameterAst::Stage7_AnalyseSemantics(sm, meta);

  // What a default may hold is limited, because it is
  // copied into every call that leaves it out. Ensure
  // it is valid.
  auto const &written = Source.OriginalDefaultVal != nullptr
    ? *Source.OriginalDefaultVal
    : *DefaultVal;

  RaiseIf<SppInvalidDefaultValueError>(
    not written.IsAllowedInDefault(),
    {sm->CurrentScope}, ERR_ARGS(written, "parameter", "call"));

  // Standard ast shape check on what's valid as a primary
  // expression.
  DefaultVal->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(*DefaultVal, *sm),
    {sm->CurrentScope}, ERR_ARGS(*DefaultVal));

  // Do a type check on the default value's type vs the type
  // given.
  const auto default_type = DefaultVal->InferType(sm, meta);
  RaiseIf<SppTypeMismatchError>(
    not TypeEq(*Type, *default_type, *sm->CurrentScope, *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *Type, *DefaultVal, *default_type));
  _DefaultAnalysed = true;
}

auto spp::asts::FunctionParameterOptionalAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Perform default memory checking steps.
  using analyse::utils::mem_utils::ValidateSymbolMemory;
  FunctionParameterAst::Stage8_CheckMemory(sm, meta);

  // Check the memory status of the default value expression.
  DefaultVal->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(*DefaultVal, *DefaultVal, *sm, true, true, true, true, meta);
}

SPP_MOD_END
