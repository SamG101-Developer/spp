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

  // Get the loop's exit type (or Void if there are no
  // exits from inside the loop).
  auto [exit_expr, loop_type, exit_scope] = _LoopExitTypeInfo.has_value()
    ? *_LoopExitTypeInfo
    : Tup(static_cast<ExpressionAst*>(nullptr), VoidAt(PosStart()), static_cast<Scope*>(nullptr));
  exit_expr = exit_expr ? exit_expr : this;

  // Check the else block's type is the same as the loop exit
  // type. An exit that diverges ("!") has no value for the else
  // block to agree with, so the loop takes the else block's type
  // instead; it is only "!" when that diverges too. The exit type
  // is read in the scope its "exit" was written in.
  if (ElseBlock != nullptr and not meta->IgnoreMissingElseBranchForInference) {
    const auto else_type = ElseBlock->InferType(sm, meta);
    const auto final_member = ElseBlock->Body->FinalMember();
    if (loop_type->IsNeverType()) {
      loop_type = else_type;
      exit_scope = nullptr;
    }

    const auto loop_scope = exit_scope != nullptr
      ? *exit_scope
      : *sm->CurrentScope;

    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        TypeRef::Of(*loop_type, loop_scope),
        TypeRef::Of(*else_type, *sm->CurrentScope),
        loop_scope, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*exit_expr, *loop_type, *final_member, *else_type));
  }

  // Return the loop type.
  return loop_type;
}

auto LoopExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The exit type, checked against the else block as written; resolved where it is read.
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef();
}

SPP_MOD_END
