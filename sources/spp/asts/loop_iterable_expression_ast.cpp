module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.asts.loop_iterable_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_resolution;
import spp.asts.assignment_statement_ast;
import spp.asts.boolean_literal_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.defer_statement_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.loop_conditional_expression_ast;
import spp.asts.loop_else_statement_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
LoopIterableExpressionAst::LoopIterableExpressionAst(
  decltype(TokLoop) &&tok_loop,
  decltype(Var) &&var,
  decltype(TokIn) &&tok_in,
  decltype(Iterable) &&iterable,
  decltype(Body) &&body,
  decltype(ElseBlock) &&else_block) :
  LoopExpressionAst(std::move(tok_loop), std::move(body), std::move(else_block)),
  Var(std::move(var)),
  TokIn(std::move(tok_in)),
  Iterable(std::move(iterable)),
  _TransformedLet(nullptr),
  _TransformedFlagLet(nullptr),
  _TransformedLoop(nullptr),
  _IterDefer(nullptr),
  _IterDrop(nullptr),
  _IterableName(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokIn, lex::SppTokenType::KW_IN, "in");
}

LoopIterableExpressionAst::~LoopIterableExpressionAst() = default;

auto LoopIterableExpressionAst::PosStart() const -> std::size_t {
  // Use the "loop" token or the transformed loop.
  return TokLoop != nullptr ? TokLoop->PosStart() : _TransformedLoop->PosStart();
}

auto LoopIterableExpressionAst::PosEnd() const -> std::size_t {
  // Use the iterable or the transformed loop.
  return Iterable != nullptr ? Iterable->PosEnd() : _TransformedLoop->PosEnd();
}

auto LoopIterableExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast, and the lowering Stage 7
  // built (which moved the written members into it), so a clone
  // taken after Stage 7 is as complete as the original.
  auto ast = MakeUnique<LoopIterableExpressionAst>(
    AstClone(TokLoop),
    AstClone(Var),
    AstClone(TokIn),
    AstClone(Iterable),
    AstClone(Body),
    AstClone(ElseBlock));
  ast->_TransformedLet = AstClone(_TransformedLet);
  ast->_TransformedFlagLet = AstClone(_TransformedFlagLet);
  ast->_TransformedLoop = AstClone(_TransformedLoop);
  ast->_IterDefer = AstClone(_IterDefer);
  ast->_IterDrop = AstClone(_IterDrop);
  ast->_IterableName = AstCloneShared(_IterableName);
  return ast;
}

auto LoopIterableExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  if (_TransformedLoop != nullptr) {
    SPP_STRING_APPEND(_TransformedLoop);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND(TokLoop).append(" ");
  SPP_STRING_APPEND(Var).append(" ");
  SPP_STRING_APPEND(TokIn).append(" ");
  SPP_STRING_APPEND(Iterable).append(" ");
  SPP_STRING_APPEND(Body).append("\n");
  SPP_STRING_APPEND(ElseBlock);
  SPP_STRING_END;
}

auto LoopIterableExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS_AND_UID;

  // Simple statements to move from.
  const auto uid = "_" + Uid();
  auto iterable_name = MakeShared<IdentifierAst>(PosStart(), "$_iter" + uid);
  auto resume_name = MakeShared<IdentifierAst>(PosStart(), "$_res" + uid);
  auto flag_name = MakeShared<IdentifierAst>(PosStart(), "$_ok" + uid);
  _IterableName = iterable_name;

  // Create the initial let statement to materialize the
  // condition being iterated, and analyse it first: the
  // generator's type is read off its value.
  // Translated: "let $_iter = <iterable>".
  auto iterable_let = [&] {
    auto mut = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_MUT, "mut");
    auto iterable_var = MakeUnique<LocalVariableSingleIdentifierAst>(std::move(mut), iterable_name, nullptr);
    auto iterable_val = std::move(Iterable);
    return MakeUnique<LetStatementInitializedAst>(
      nullptr, std::move(iterable_var), nullptr, nullptr, std::move(iterable_val));
  }();
  iterable_let->Stage7_AnalyseSemantics(sm, meta);

  // Grab the generator's inner type.
  auto &iterable = *iterable_let->Val;
  const auto gen = marker_sups::FindGenSup(
    iterable.InferTypeRef(sm, meta), *sm->CurrentScope, iterable,
    [&] { return iterable.InferType(sm, meta); }, "loop iterable");
  marker_sups::EnforceYieldTypeWithoutGenDone(
    gen, *sm->CurrentScope, iterable, "loop iterable");
  const auto yield_type = marker_sups::GenYieldOf(gen).AstIn(*sm->CurrentScope);

  // Create the let statement holding the loop's continuation
  // flag, to control entering the else block.
  // Translated: "let mut $_ok = true".
  auto flag_let = [&] {
    auto mut = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_MUT, "mut");
    auto flag_var = MakeUnique<LocalVariableSingleIdentifierAst>(std::move(mut), flag_name, nullptr);
    auto flag_val = BooleanLiteralAst::True(PosStart());
    return MakeUnique<LetStatementInitializedAst>(
      nullptr, std::move(flag_var), nullptr, nullptr, std::move(flag_val));
  }();

  // Create the "let" statement that increments the iterator.
  // Translated: "let $_res = $_iter.resume()".
  auto resume_let = [&] {
    auto resume_fn_call = MakeUnique<PostfixExpressionOperatorKeywordResAst>(nullptr, nullptr, nullptr);
    auto resume_val = MakeUnique<PostfixExpressionAst>(AstClone(iterable_name), std::move(resume_fn_call));
    auto resume_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, resume_name, nullptr);
    return MakeUnique<LetStatementInitializedAst>(
      nullptr, std::move(resume_var), nullptr, nullptr, std::move(resume_val));
  }();

  // If the value is the iterable type, then use the inner body.
  // Translated: "<case-of-expr> is &Str::Str(..) { ... }".
  auto case_valid_branch = [&] {
    auto ignore_fields = MakeUnique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(nullptr, nullptr);
    auto case_type_pattern = CasePatternVariantDestructureObjectAst::FromType(yield_type);
    case_type_pattern->Elems.emplace_back(std::move(ignore_fields));

    auto is_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_IS, "is");
    auto patterns = Vec<Unique<CasePatternVariantAst>>();
    patterns.emplace_back(std::move(case_type_pattern));

    // Destructure the "$_res" variable into parts defined in
    // "loop (v1, v2) in ..."
    auto let_stmt = MakeUnique<LetStatementInitializedAst>(
      nullptr, std::move(Var), nullptr, nullptr, AstClone(resume_name));
    Body->Members.insert(Body->Members.begin(), std::move(let_stmt));
    return MakeUnique<CaseExpressionBranchAst>(
      std::move(is_tok), std::move(patterns), nullptr, std::move(Body));
  }();

  // Reaching the branch above is what counts as the loop
  // having taken an iteration.
  case_valid_branch->MarkForIterLoopYield();

  // Otherwise, the generator is exhausted, so clear the
  // continuation flag.
  // Translated: "<case-of-expr> else { $_ok = false }".
  auto case_else_branch = [&] {
    auto case_else_pattern = MakeUnique<CasePatternVariantElseAst>(nullptr);
    auto case_else_body = InnerScopeExpressionAst::NewEmpty();
    case_else_pattern->MarkForIterLoopExit();

    auto flag_clear_lhs = Vec<Unique<ExpressionAst>>();
    auto flag_clear_rhs = Vec<Unique<ExpressionAst>>();
    flag_clear_lhs.emplace_back(AstClone(flag_name));
    flag_clear_rhs.emplace_back(BooleanLiteralAst::False(PosStart()));
    auto flag_clear = MakeUnique<AssignmentStatementAst>(
      std::move(flag_clear_lhs), nullptr, std::move(flag_clear_rhs));

    auto is_tok = MakeUnique<TokenAst>(PosStart(), lex::SppTokenType::KW_IS, "is");
    auto patterns = Vec<Unique<CasePatternVariantAst>>();
    patterns.emplace_back(std::move(case_else_pattern));
    case_else_body->Members.emplace_back(std::move(flag_clear));
    return MakeUnique<CaseExpressionBranchAst>(
      std::move(is_tok), std::move(patterns), nullptr, std::move(case_else_body));
  }();

  // Case block to handle the resume value.
  auto case_expr = [&] {
    auto case_branches = Vec<Unique<CaseExpressionBranchAst>>();
    case_branches.emplace_back(std::move(case_valid_branch));
    case_branches.emplace_back(std::move(case_else_branch));
    return MakeUnique<CaseExpressionAst>(
      nullptr, AstClone(resume_name), nullptr, std::move(case_branches));
  }();

  // New boolean loop that manually iterates the iterable.
  // Translated: "loop $_ok { ... } else { ... }".
  auto loop_new = [&] {
    auto cond = AstClone(flag_name);
    return MakeUnique<LoopConditionalExpressionAst>(
      nullptr, std::move(cond), nullptr, std::move(ElseBlock));
  }();
  loop_new->Body->Members.emplace_back(std::move(resume_let));
  loop_new->Body->Members.emplace_back(std::move(case_expr));
  loop_new->MarkAsIterDesugar();
  _TransformedLoop = std::move(loop_new);

  // The iterator is consumed on every way out of the loop:
  // by a "defer" while the loop runs (the ways that also
  // leave this scope), then by a drop after it (the rest).
  // Translated: "defer std::mem::ops::drop($_iter)" /
  // "std::mem::ops::drop($_iter)".
  const auto drop_iter = [&]() -> Unique<ExpressionAst> {
    auto args = Vec<Unique<FunctionCallArgumentAst>>();
    args.emplace_back(MakeUnique<FunctionCallArgumentPositionalAst>(
      nullptr, nullptr, AstClone(iterable_name)));
    auto call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
      nullptr, MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(args), nullptr), nullptr);
    return MakeUnique<PostfixExpressionAst>(
      INJECT_CODE("std::mem::ops::drop", ParseExpression), std::move(call));
  };
  _IterDefer = MakeUnique<DeferStatementAst>(nullptr, drop_iter());
  _IterDrop = drop_iter();

  // Analyse the other initial let statement.
  _TransformedLet = std::move(iterable_let);
  _IterDefer->Stage7_AnalyseSemantics(sm, meta);
  flag_let->Stage7_AnalyseSemantics(sm, meta);
  _TransformedFlagLet = std::move(flag_let);
  _TransformedLoop->Stage7_AnalyseSemantics(sm, meta);
  _IterDrop->Stage7_AnalyseSemantics(sm, meta);
}

auto LoopIterableExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Call the memory check on the transformed loop.
  _TransformedLet->Stage8_CheckMemory(sm, meta);
  _IterDefer->Stage8_CheckMemory(sm, meta);
  _TransformedFlagLet->Stage8_CheckMemory(sm, meta);
  _TransformedLoop->Stage8_CheckMemory(sm, meta);

  // The "defer" covers the loop only; after it, the drop
  // ends the iterator and its loans (the "&mut v" of
  // "v.iter_mut()").
  auto &deferred = sm->CurrentScope->Deferred;
  deferred |= genex::actions::remove_if([this](auto const *d) { return d == _IterDefer.get(); });
  _IterDrop->Stage8_CheckMemory(sm, meta);
}

auto LoopIterableExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the code for the transformed loop.
  _TransformedLet->Stage11_CodeGen(sm, meta, ctx);
  _IterDefer->Stage11_CodeGen(sm, meta, ctx);
  _TransformedFlagLet->Stage11_CodeGen(sm, meta, ctx);
  const auto loop_val = _TransformedLoop->Stage11_CodeGen(sm, meta, ctx);

  // As in stage 8: the "defer" covers the loop only, and the drop
  // follows it, where the loop can be left at all.
  auto &reached = sm->CurrentScope->DeferredReached;
  reached |= genex::actions::remove_if([this](auto const *d) { return d == _IterDefer.get(); });
  if (not ctx->Builder.GetInsertBlock()->hasTerminator()) { _IterDrop->Stage11_CodeGen(sm, meta, ctx); }
  return loop_val;
}

auto LoopIterableExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Before the desugaring in stage 7 there is no transformed
  // loop to ask, so fall back to the base implementation.
  return _TransformedLoop == nullptr
    ? LoopExpressionAst::InferType(sm, meta)
    : _TransformedLoop->InferType(sm, meta);
}

auto LoopIterableExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return _TransformedLoop == nullptr
    ? LoopExpressionAst::InferTypeRef(sm, meta)
    : _TransformedLoop->InferTypeRef(sm, meta);
}

SPP_MOD_END
