module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.loop_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_join;
import spp.asts.boolean_literal_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
LoopExpressionAst::LoopExpressionAst(
  decltype(TokLoop) &&tok_loop,
  decltype(Body) &&body,
  decltype(ElseBlock) &&else_block) :
  TokLoop(std::move(tok_loop)),
  Body(std::move(body)),
  ElseBlock(std::move(else_block)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokLoop, lex::SppTokenType::KW_LOOP, "loop");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Body);
}

LoopExpressionAst::~LoopExpressionAst() = default;

auto LoopExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  //
  IMPORT_UTILS;
  using generate::common_types_precompiled::VoidAt;

  // The type the loop's "exit"s and "else" agreed on ("TypeJoin", at stage 7): "!" when every one diverges. A loop
  // with no "exit" is "Void", and its "else" must be too.
  if (_LoopExitTypeInfo.has_value()) {
    return _LoopExitTypeInfo->Diverges
      ? generate::common_types_precompiled::NeverAt(PosStart())
      : _LoopExitTypeInfo->Type;
  }
  if (ElseBlock != nullptr and not meta->IgnoreMissingElseBranchForInference) {
    const auto void_type = VoidAt(PosStart());
    const auto else_type = ElseBlock->InferType(sm, meta);
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        TypeRef::Of(*void_type, *sm->CurrentScope), TypeRef::Of(*else_type, *sm->CurrentScope),
        *sm->CurrentScope, *sm->CurrentScope),
      ERR_ARGS(*this, *void_type, *ElseBlock->Body->FinalMember(), *else_type));
  }
  return VoidAt(PosStart());
}

auto LoopExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The exit type, checked against the else block as written; resolved where it is read.
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef();
}

SPP_MOD_END
