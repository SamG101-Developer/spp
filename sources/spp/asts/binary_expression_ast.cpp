module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.binary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.operator_desugaring;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.boolean_literal_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import llvm;

use_alias(spp::asts::meta, AllowPackWholeGuard);

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
namespace {
  /// Check if the binary operation is a logical "and" or
  /// a logical "or", as these two have special behaviour;
  /// they use intrinsics, not function mappings, allowing
  /// for short-circuiting.
  auto IsLogicalToken(
    TokenAst const *tok)
    -> bool {
    // Simple token type check. The token can be absent: rewriting a comparison chain moves the operator out of the
    // node it replaces ("CombineComparisonChain"), and a clone of that node is built with no token at all.
    using spp::lex::SppTokenType;
    return tok != nullptr
      and (tok->TokenType == SppTokenType::KW_AND
        or tok->TokenType == SppTokenType::KW_OR);
  }
}

spp::asts::BinaryExpressionAst::BinaryExpressionAst(
  decltype(Lhs) &&lhs,
  decltype(TokOp) &&tok_op,
  decltype(Rhs) &&rhs) :
  Lhs(std::move(lhs)),
  TokOp(std::move(tok_op)),
  Rhs(std::move(rhs)),
  _MappedFn(nullptr),
  _LogicalAnalysed(false),
  _IsLogical(IsLogicalToken(TokOp.get())) {
  Source.OriginalPosStart = Lhs ? Lhs->PosStart() : 0;
  Source.OriginalPosEnd = Rhs ? Rhs->PosEnd() : 0;
}

spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;

auto spp::asts::BinaryExpressionAst::PosStart() const -> std::size_t {
  // Use the left hand side operand.
  return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto spp::asts::BinaryExpressionAst::PosEnd() const -> std::size_t {
  // Use the right hand side operand.
  return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto spp::asts::BinaryExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<BinaryExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp),
    AstClone(Rhs));
  ast->_MappedFn = _MappedFn;
  ast->_ChainTemps = AstCloneVec(_ChainTemps);
  ast->_OpenPack = AstClone(_OpenPack);
  ast->_OpenPackMoves = _OpenPackMoves;
  ast->Source = Source;
  return ast;
}

auto spp::asts::BinaryExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  if (Lhs != nullptr) {
    raw_string.append("(");
    SPP_STRING_APPEND(Lhs).append(" ");
    SPP_STRING_APPEND(TokOp).append(" ");
    SPP_STRING_APPEND(Rhs).append(")");
  }
  else {
    SPP_STRING_APPEND(_MappedFn);
  }
  SPP_STRING_END;
}

auto spp::asts::BinaryExpressionAst::IsLogicalOperator() const -> bool {
  // Simple getter on the flag determining if this is a
  // logic operation.
  return _IsLogical;
}

auto spp::asts::BinaryExpressionAst::_BindFoldOperand(
  Unique<ExpressionAst> &operand, ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Each element is read off it, so it is evaluated once ("HoistOnce"): a place where it is, anything else into a
  // temporary.
  if (auto hoisted = analyse::utils::expr_utils::HoistOnce(*operand, "$fold", sm, meta); hoisted.Let != nullptr) {
    _ChainTemps.emplace_back(std::move(hoisted.Let));
    operand = AstClone(hoisted.Name);
  }

  // A name may be a pack, which a fold names whole ("AllowPackWhole").
  const auto _pack_guard = AllowPackWholeGuard(meta, true);
  operand->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::BinaryExpressionAst::_FoldElements(
  Unique<ExpressionAst> &tuple, Ast const &fold, ScopeManager *sm, CompilerMetaData *meta)
  -> Vec<Unique<ExpressionAst>> {
  IMPORT_UTILS;
  _BindFoldOperand(tuple, sm, meta);
  common_errors::RequirePrimary(*tuple, *sm);

  // The elements, read off as a spread reads them ("ExpandTupleSpread": a closed pack's parameters "xs#0", ..., or a
  // tuple's "t.0", ...), each analysed. The operand is a place by now, so nothing more is hoisted.
  const auto elements_of = [&] {
    auto spread = expr_utils::ExpandTupleSpread(*tuple, *tuple, sm, meta);
    for (auto const &elem : spread->Elems) { elem->Stage7_AnalyseSemantics(sm, meta); }
    return std::move(spread->Elems);
  };

  // A closed pack has at least two elements.
  auto const *const pack_name = tuple->To<IdentifierAst>();
  auto const *const pack = pack_name != nullptr ? sm->CurrentScope->FindVarSymbol(pack_name) : nullptr;
  if (pack != nullptr and pack->Pack.has_value() and not pack->Pack->Open) {
    if (pack->Pack->Elements.size() < 2) {
      const auto tuple_type = tuple->InferType(sm, meta);
      Raise<SppInvalidBinaryFoldExpressionError>(ERR_ARGS(*tuple, *tuple_type, pack->Pack->Elements.size()));
    }
    return elements_of();
  }

  // An open pack, or a value of an open tuple (a fold's results over an open pack, "(..xs)"): of unknown length, so
  // three elements are read off it ("xs.0", "xs.1", "xs.2"), and "Stage7" checks a fold of two and of three agree on
  // their type: then each element more keeps it, whatever the length.
  const auto open_ref = tuple->InferTypeRef(sm, meta);
  if (type_compare::IsOpenTuple(open_ref.WithoutConvention(), *sm->CurrentScope) and tuple->To<IdentifierAst>() != nullptr) {
    _OpenPack = AstClone(tuple->To<IdentifierAst>());
    auto elems = Vec<Unique<ExpressionAst>>();
    for (auto i = 0uz; i < 3; ++i) {
      auto elem = expr_utils::ElementAccess(*tuple, i);
      elem->Stage7_AnalyseSemantics(sm, meta);
      elems.emplace_back(std::move(elem));
    }
    const auto element = elems.front()->InferTypeRef(sm, meta);
    _OpenPackMoves = not element.IsBorrowed() and not open_ref.IsBorrowed()
      and (element.Symbol == nullptr or not element.Symbol->IsCopyable());
    return elems;
  }

  // The operand is a tuple (Todo: maybe allow arrays too?) of at
  // least two elements.
  const auto tuple_ref = tuple->InferTypeRef(sm, meta);
  if (not type_predicates::IsTypeTuple(tuple_ref, *sm->CurrentScope)) {
    const auto tuple_type = tuple->InferType(sm, meta);
    Raise<SppMemberAccessNonIndexableError>(ERR_ARGS(*tuple, *tuple_type, fold));
  }
  const auto num_elems = tuple_ref.Symbol->TypeArgRefs().size();
  if (num_elems < 2) {
    const auto tuple_type = tuple->InferType(sm, meta);
    Raise<SppInvalidBinaryFoldExpressionError>(ERR_ARGS(*tuple, *tuple_type, num_elems));
  }
  return elements_of();
}

auto spp::asts::BinaryExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Analysed already: a clone shares the mapped call ("Clone"),
  // and a fold has rewritten its operands, so analysing again
  // would desugar what is no longer written.
  if (_MappedFn or _LogicalAnalysed) { return; }

  // A fold reads its tuple's elements off it and combines them, for "t = (0, 1, 2, 3)": ".. + t" from the left,
  // "(((t.0 + t.1) + t.2) + t.3)", and "t + .." from the right, "(t.0 + (t.1 + (t.2 + t.3)))".
  if (Lhs->To<FoldExpressionAst>() != nullptr or Rhs->To<FoldExpressionAst>() != nullptr) {
    const auto from_left = Lhs->To<FoldExpressionAst>() != nullptr;
    auto elems = from_left ? _FoldElements(Rhs, *Lhs, sm, meta) : _FoldElements(Lhs, *Rhs, sm, meta);
    if (from_left) {
      Lhs = std::move(elems[0]);
      Rhs = std::move(elems[1]);
      for (auto &&elem : elems | genex::views::move | genex::views::drop(2)) {
        Lhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
        Rhs = std::move(elem);
      }
    }
    else {
      Lhs = std::move(elems[elems.size() - 2]);
      Rhs = std::move(elems[elems.size() - 1]);
      for (auto &&elem : elems | genex::views::move_reverse | genex::views::drop(2)) {
        Rhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
        Lhs = std::move(elem);
      }
    }
    _MappedFn = operator_desugaring::ConvertBinExprToFnCall(*this, sm, meta);
    _MappedFn->Stage7_AnalyseSemantics(sm, meta);

    // An open pack's fold of three elements is only the type of every fold of three or more if the fold of two has
    // it too: each element more is then the same step again. A fold of fewer than two is the call's to reject.
    if (_OpenPack != nullptr) {
      const auto element = [&](const std::size_t i) { return expr_utils::ElementAccess(*_OpenPack, i); };
      auto two = from_left
        ? MakeUnique<BinaryExpressionAst>(element(0), AstClone(TokOp), element(1))
        : MakeUnique<BinaryExpressionAst>(element(1), AstClone(TokOp), element(2));
      two->Stage7_AnalyseSemantics(sm, meta);
      const auto two_type = two->InferType(sm, meta);
      const auto all_type = _MappedFn->InferType(sm, meta);
      if (not type_compare::TypeEq(
        TypeRef::Of(*two_type, *sm->CurrentScope), TypeRef::Of(*all_type, *sm->CurrentScope),
        *sm->CurrentScope, *sm->CurrentScope)) {
        Raise<SppTypeMismatchError>(ERR_ARGS(*two, *two_type, *this, *all_type));
      }
    }
  }

  else {
    // A chain of comparisons is rewritten into an "and" of
    // its pairs first, so that the "and" it produces is
    // analysed as one - conditional right operand and all -
    // rather than being turned straight into a call.
    const auto combined = operator_desugaring::CombineComparisonChain(*this, sm, meta, _ChainTemps);
    Lhs = std::move(combined->Lhs);
    TokOp = std::move(combined->TokOp);
    Rhs = std::move(combined->Rhs);
    _IsLogical = IsLogicalToken(TokOp.get());

    common_errors::RequirePrimary(*Lhs, *sm);
    common_errors::RequirePrimary(*Rhs, *sm);

    // "and" and "or" are not mapped to a method; they are built
    // in, like "not", for short-circuiting. Both operands must
    // be "Bool", again like "not".
    if (IsLogicalOperator()) {
      // What an "is" on the left binds holds on the right of an
      // "and" (the match succeeded), but not of an "or" - the right
      // only runs when it failed.
      const auto bound_before = meta->AddedIsBindings.size();
      Lhs->Stage7_AnalyseSemantics(sm, meta);
      if (TokOp->TokenType == lex::SppTokenType::KW_OR) {
        for (auto i = bound_before; i < meta->AddedIsBindings.size(); ++i) {
          meta->ExpiredIsBindings.emplace_back(meta->AddedIsBindings[i]);
        }
      }
      Rhs->Stage7_AnalyseSemantics(sm, meta);

      const auto what = Str("\"") + TokOp->TokenData + "\" expression";
      // An owned "Bool" each (a borrow is not one); the type is only spelled out for the error.
      // Held, not passed through: the error's arguments are bound as references, and these types are nobody
      // else's - the temporary they came back in would be gone before the error was built.
      if (not type_predicates::IsTypeBool(Lhs->InferTypeRef(sm, meta), *sm->CurrentScope)) {
        const auto lhs_ty = Lhs->InferType(sm, meta);
        Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Lhs, *lhs_ty, what));
      }
      if (not type_predicates::IsTypeBool(Rhs->InferTypeRef(sm, meta), *sm->CurrentScope)) {
        const auto rhs_ty = Rhs->InferType(sm, meta);
        Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Rhs, *rhs_ty, what));
      }

      _LogicalAnalysed = true;
      return;
    }

    // Standard non-folding binary expression.
    _MappedFn = operator_desugaring::ConvertBinExprToFnCall(*this, sm, meta);
    _MappedFn->Stage7_AnalyseSemantics(sm, meta);
  }
}

auto spp::asts::BinaryExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Run memory analysis on the temporary materialized
  // comparison chain values.
  for (auto const &temp : _ChainTemps) { temp->Stage8_CheckMemory(sm, meta); }

  // A logical operator has no mapped function to forward
  // to. Both operands are checked as at worst, they both
  // evaluate, so must both be valid. Maintains consistency
  // in all code.
  if (IsLogicalOperator()) {
    Lhs->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*Lhs, *this, *sm, meta, {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
    Rhs->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*Rhs, *this, *sm, meta, {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
    return;
  }

  // A fold over a template's open pack uses the whole pack, of whatever length, not just the three elements it is
  // checked on: it must hold every element first, and has moved them all after, unless each is copied.
  if (_OpenPack != nullptr) {
    mem_utils::ValidateSymbolMemory(*_OpenPack, *this, *sm, meta, {.MarkMoves = false});
    _MappedFn->Stage8_CheckMemory(sm, meta);
    if (_OpenPackMoves) {
      auto *const held = sm->CurrentScope->FindVarSymbol(_OpenPack.get());
      memory_state::StateOf(*sm->CurrentScope, *held).MovedBy(*this);
    }
    return;
  }

  // Forward the memory checking to the mapped function.
  _MappedFn->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // Forward the code generation to the mapped function. The common
  // expressions like "1 + 2" follow these steps:
  // |- 1 + 2
  // |- 1.add(2)
  // |- S32::add(self=1, that=2) [inlined]
  //    |- std::intrinsics::add(1, 2) [inlined]
  //    |- ctx->Builder.CreateAdd(a, b)
  //    |- add #1 #2
  // This allows the optimal instruction to be used, whilst maintaining
  // the uniform function processing for all operations in s++.

  // Run codegen on the temporary materialized comparison
  // chain values.
  for (auto const &temp : _ChainTemps) { temp->Stage11_CodeGen(sm, meta, ctx); }

  // Codegen via the mapped function if there is one. For
  // logical operators, move to the next steps below.
  if (not IsLogicalOperator()) { return _MappedFn->Stage11_CodeGen(sm, meta, ctx); }

  // The "and" and "or" operations cannot map from the function
  // as there is no function to map from. Instead, they have
  // manual codegen, like the "not" operator.
  const auto uid = "." + Uid();
  const auto is_and = TokOp->TokenType == lex::SppTokenType::KW_AND;
  const auto llvm_bool_ty = llvm::Type::getInt1Ty(*ctx->Context);

  // Both operands are owned booleans, so each generates as an
  // "i1"
  const auto llvm_lhs = Lhs->Stage11_CodeGen(sm, meta, ctx);
  SPP_ASSERT(llvm_lhs->getType()->isIntegerTy(1));
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto rhs_bb = llvm::BasicBlock::Create(
    *ctx->Context, "logical.rhs" + uid, func);
  const auto join_bb = llvm::BasicBlock::Create(
    *ctx->Context, "logical.join" + uid, func);

  // Taken from here rather than from before the operand was
  // generated, because generating it may have left the builder
  // in a block of its own making, like from a "case" on the left
  // of an "and" expression.
  const auto lhs_end_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateCondBr(
    llvm_lhs, is_and ? rhs_bb : join_bb, is_and ? join_bb : rhs_bb);

  // Do the right-hand-side codegen (doesn't execute the LLVM,
  // just creates it), and hook it into the short-circuiting
  // mechanism.
  ctx->Builder.SetInsertPoint(rhs_bb);
  const auto llvm_rhs = Rhs->Stage11_CodeGen(sm, meta, ctx);
  SPP_ASSERT(llvm_rhs->getType()->isIntegerTy(1));
  const auto rhs_end_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(join_bb);

  // The short-circuit edge carries the answer the left operand
  // already gave: "false" for an "and", "true" for an "or". The
  // other edge carries whatever the right operand evaluated to.
  ctx->Builder.SetInsertPoint(join_bb);
  const auto llvm_res = ctx->Builder.CreatePHI(llvm_bool_ty, 2, "logical" + uid);
  llvm_res->addIncoming(llvm::ConstantInt::getBool(*ctx->Context, not is_and), lhs_end_bb);
  llvm_res->addIncoming(llvm_rhs, rhs_end_bb);
  return llvm_res;
}

auto spp::asts::BinaryExpressionAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Run comptime resolution on the temporary materialized
  // comparison chain values (checks validity).
  for (auto const &temp : _ChainTemps) { temp->EvaluateCompTime(sm, meta); }

  // Do the short-circuiting at compile time by evaluating
  // the left-hand-side, and if it is true, and we are not
  // doing an "and", then we can return true already.
  if (IsLogicalOperator()) {
    Lhs->EvaluateCompTime(sm, meta);
    const auto lhs_is_true = meta->CompTimeResult->As<BooleanLiteralAst>()->IsTrue();
    if (lhs_is_true != (TokOp->TokenType == lex::SppTokenType::KW_AND)) { return; }
    Rhs->EvaluateCompTime(sm, meta);
    return;
  }

  // Forward the compile-time resolution to the mapped function.
  _MappedFn->EvaluateCompTime(sm, meta);
}

auto spp::asts::BinaryExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // A logical operator is boolean by construction - both
  // operands are required to be, and the result is one of
  // them.
  if (IsLogicalOperator()) {
    return generate::common_types_precompiled::BoolAt(PosStart());
  }

  // Not yet analysed - a comp argument waits for the sup
  // scopes its operator is resolved through: a comparison
  // is boolean, and comp-time arithmetic keeps its left
  // operand's type.
  if (_MappedFn == nullptr) {
    using lex::SppTokenType;
    const auto op = TokOp->TokenType;
    if (op == SppTokenType::TK_EQ or op == SppTokenType::TK_NE or op == SppTokenType::TK_LT
      or op == SppTokenType::TK_LE or op == SppTokenType::TK_GT or op == SppTokenType::TK_GE) {
      return generate::common_types_precompiled::BoolAt(PosStart());
    }
    return Lhs->InferType(sm, meta);
  }

  // Infer the type from the function mapping of the binary
  // expression.
  return _MappedFn->InferType(sm, meta);
}

auto spp::asts::BinaryExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // A logical operator is boolean by construction. Before
  // analysis maps it onto a function, a comparison is boolean
  // and comp-time arithmetic keeps its left operand's type.
  using generate::common_types_precompiled::BOOL;
  if (IsLogicalOperator()) { return TypeRef::Of(*BOOL, *sm->CurrentScope); }
  if (_MappedFn == nullptr) {
    using lex::SppTokenType;
    const auto op = TokOp->TokenType;
    if (op == SppTokenType::TK_EQ or op == SppTokenType::TK_NE or op == SppTokenType::TK_LT
      or op == SppTokenType::TK_LE or op == SppTokenType::TK_GT or op == SppTokenType::TK_GE) {
      return TypeRef::Of(*BOOL, *sm->CurrentScope);
    }
    return Lhs->InferTypeRef(sm, meta);
  }
  return _MappedFn->InferTypeRef(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Operands() const -> Pair<ExpressionAst const*, ExpressionAst const*> {
  if (Lhs != nullptr and Rhs != nullptr) { return {Lhs.get(), Rhs.get()}; }
  if (_MappedFn == nullptr) { return {nullptr, nullptr}; }
  // "lhs.add(rhs)" as converted, or "Type::add(self=lhs, that=rhs)" once the call is elaborated.
  auto const *const access = _MappedFn->Lhs->To<PostfixExpressionAst>();
  auto const *const call = _MappedFn->Op->To<PostfixExpressionOperatorFunctionCallAst>();
  if (call == nullptr) { return {nullptr, nullptr}; }
  auto const &args = call->FnArgGroup->Args;
  if (args.size() == 2) { return {args[0]->Val.get(), args[1]->Val.get()}; }
  if (args.size() == 1 and access != nullptr and access->Lhs != nullptr) { return {access->Lhs.get(), args[0]->Val.get()}; }
  return {nullptr, nullptr};
}

auto spp::asts::BinaryExpressionAst::ReadExpr(
  ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // Both operands are expressions, read where they are written or where analysis moved them.
  const auto [lhs, rhs] = Operands();
  if (lhs == nullptr or rhs == nullptr) { return AstCloneShared(this); }
  return MakeShared<BinaryExpressionAst>(
    AstClone(lhs->ReadExpr(sub)),
    AstClone(TokOp),
    AstClone(rhs->ReadExpr(sub)));
}

auto spp::asts::BinaryExpressionAst::IsAllowedInDefault() const -> bool {
  // Check the left-hand-side and right-hand-side elems.
  return
    (Lhs == nullptr or Lhs->IsAllowedInDefault()) and
    (Rhs == nullptr or Rhs->IsAllowedInDefault());
}

SPP_MOD_END
