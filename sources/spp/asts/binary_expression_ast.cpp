module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.binary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.drop_utils;
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
import spp.codegen.llvm_drop;
import spp.lex.tokens;
import spp.utils.functions;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import llvm;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeBlockName);
use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts, TokenAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

namespace spp::asts {
  /// Check if the binary operation is a logical "and" or a logical "or", as
  /// these two have special behaviour; they use intrinsics, not function
  /// mappings, allowing for short-circuiting.
  static auto IsLogicalToken(TokenAst const *tok) -> bool;
}

auto spp::asts::IsLogicalToken(TokenAst const *tok) -> bool {
  // Simple token type check. The token can be absent: rewriting a comparison
  // chain moves the operator out of the node it replaces, and a clone of that
  // node is built with no token at all.
  using spp::lex::SppTokenType;
  return tok != nullptr
    and (tok->TokenType == SppTokenType::KW_AND
      or tok->TokenType == SppTokenType::KW_OR);
}

SPP_MOD_BEGIN
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
  ast->_DropsOnShortCircuit = _DropsOnShortCircuit;
  ast->_DropsAfterRhs = _DropsAfterRhs;
  ast->_DropsRhsTemporaries = _DropsRhsTemporaries;
  ast->_IsCondition = _IsCondition;
  ast->_HeldWhenTrue = _HeldWhenTrue;
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
  const auto _pack_guard = spp::utils::functions::ScopedAssign(
    meta->AllowPackWhole, true);
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
      _NestChainRight();

      // An owned "Bool" each (a borrow is not one); the type is only spelled out for the error.
      // Held, not passed through: the error's arguments are bound as references, and these types are nobody
      // else's - the temporary they came back in would be gone before the error was built.
      const auto what = Str("\"") + TokOp->TokenData + "\" expression";
      Lhs->Stage7_AnalyseSemantics(sm, meta);
      if (not type_predicates::IsTypeBool(Lhs->InferTypeRef(sm, meta), *sm->CurrentScope)) {
        const auto lhs_ty = Lhs->InferType(sm, meta);
        Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Lhs, *lhs_ty, what));
      }

      // Where an "and"'s left operand is true, what it bound holds: its
      // right operand is then analysed in a scope of its own, holding
      // those bindings. Nothing else has one (a comp argument's operator
      // is analysed off the walk, which never enters a scope it makes).
      const auto scoped = TokOp->TokenType == lex::SppTokenType::KW_AND
        and not analyse::utils::expr_utils::BoundWhenTrue(*Lhs).empty();
      if (scoped) {
        auto scope_name = ScopeBlockName::FromParts("and-rhs", {}, Rhs->PosStart());
        sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
        sm->CurrentScope->IsOperandScope = true;
        _RhsScope = sm->CurrentScope;
        _DeclareLhsBound(*sm->CurrentScope);
      }
      Rhs->Stage7_AnalyseSemantics(sm, meta);
      if (not type_predicates::IsTypeBool(Rhs->InferTypeRef(sm, meta), *sm->CurrentScope)) {
        const auto rhs_ty = Rhs->InferType(sm, meta);
        Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Rhs, *rhs_ty, what));
      }
      if (scoped) { sm->MoveOutOfCurrentScope(); }

      _LogicalAnalysed = true;
      return;
    }

    // Standard non-folding binary expression.
    _MappedFn = operator_desugaring::ConvertBinExprToFnCall(*this, sm, meta);
    _MappedFn->Stage7_AnalyseSemantics(sm, meta);
  }
}

auto spp::asts::BinaryExpressionAst::_DeclareLhsBound(
  analyse::scopes::Scope &rhs_scope) const -> void {
  if (TokOp->TokenType != lex::SppTokenType::KW_AND) { return; }
  rhs_scope.DeclareBoundWhenTrue(analyse::utils::expr_utils::BoundWhenTrue(*Lhs));
}

auto spp::asts::BinaryExpressionAst::_InRhsScope(
  ScopeManager &sm, auto &&walk) const -> void {
  if (_RhsScope == nullptr) {
    walk();
    return;
  }
  sm.MoveToNextScope();
  SPP_ASSERT(sm.CurrentScope == _RhsScope);
  _DeclareLhsBound(*sm.CurrentScope);
  walk();
  sm.MoveOutOfCurrentScope();
}

auto spp::asts::BinaryExpressionAst::MarkCondition(
  ExpressionAst &cond) -> void {
  if (auto *const binary = cond.To<BinaryExpressionAst>(); binary != nullptr) { binary->_IsCondition = true; }
}

auto spp::asts::BinaryExpressionAst::HeldWhenTrue(
  ExpressionAst const &cond) -> Vec<analyse::scopes::VariableSymbol*> {
  auto const *const binary = cond.To<BinaryExpressionAst>();
  return binary != nullptr ? binary->_HeldWhenTrue : Vec<analyse::scopes::VariableSymbol*>();
}

auto spp::asts::BinaryExpressionAst::_NestChainRight() -> void {
  // "(a and b) and c" is "a and (b and c)": the same answer, each
  // operand run or skipped alike. Nested to the right, "c" runs on
  // "b"'s path, where what "b" made is still held: the "$is" its
  // "is" bindings view lives to the end of the operand it is made
  // in ("_DropsRhsTemporaries"), and "c" can read them.
  while (true) {
    auto *const inner = Lhs->To<BinaryExpressionAst>();
    if (inner == nullptr or inner->TokOp->TokenType != TokOp->TokenType) { return; }
    auto rhs = MakeUnique<BinaryExpressionAst>(std::move(inner->Rhs), std::move(TokOp), std::move(Rhs));
    TokOp = std::move(inner->TokOp);
    Lhs = std::move(inner->Lhs);
    Rhs = std::move(rhs);
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
  // The right operand runs only when the left does not decide the
  // answer, so the two paths are joined after it.
  if (IsLogicalOperator()) {
    Lhs->Stage8_CheckMemory(sm, meta);
    mem_utils::ValidateSymbolMemory(*Lhs, *this, *sm, meta, {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
    // An "and" held where it is true is so through its right
    // operand: in "a and (b and c)", "b and c" is true just where
    // the whole is (given "a").
    const auto held_when_true = _IsCondition and TokOp->TokenType == lex::SppTokenType::KW_AND;
    if (held_when_true) { MarkCondition(*Rhs); }

    const auto short_circuit = memory_state::SnapshotFlow(*sm->CurrentScope);
    const auto rhs_region = drop_utils::TemporaryRegion(*sm->CurrentScope->TemporaryHome());
    _InRhsScope(*sm, [&] {
      Rhs->Stage8_CheckMemory(sm, meta);
      mem_utils::ValidateSymbolMemory(*Rhs, *this, *sm, meta, {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
    });

    // A temporary the right operand declared was never made on the
    // short-circuit path, so the join never sees it. It ends with the
    // right operand, or, held where the "and" is true, is dropped only
    // where the right operand answers false (but not what a nested
    // "and" there already dropped where it answered false).
    _HeldWhenTrue.clear();
    if (held_when_true) {
      _HeldWhenTrue = rhs_region.Declared();
      _DropsRhsTemporaries = rhs_region.Held(*Rhs, *sm, meta, HeldWhenTrue(*Rhs));
    }
    else {
      _DropsRhsTemporaries = rhs_region.End(*Rhs, *sm, meta);
    }
    _JoinShortCircuit(short_circuit, rhs_region.Declared(), *sm, meta);
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

auto spp::asts::BinaryExpressionAst::_JoinShortCircuit(
  analyse::utils::memory_state::ScopeSnapshot const &short_circuit,
  Vec<analyse::scopes::VariableSymbol*> const &rhs_temporaries, ScopeManager &sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  _DropsOnShortCircuit.clear();
  _DropsAfterRhs.clear();
  auto const *const flow = memory_state::FlowOf(*sm.CurrentScope);
  if (flow == nullptr) { return; }

  // The values of this block and the ones around it may be held on
  // one path and not the other; only those the right operand changed
  // are joined.
  auto owners = drop_utils::OwnersAround(*sm.CurrentScope, meta);
  for (auto const *const owner : sm.CurrentScope->GetOwners()) { owners.emplace(owner, sm.CurrentScope); }
  auto syms = Vec<analyse::scopes::VariableSymbol*>();
  for (auto const *const sym : flow->States | genex::views::keys) {
    const auto on_rhs = memory_state::StateOf(*sm.CurrentScope, *sym);
    const auto on_short = memory_state::StateIn(short_circuit, *sym);
    if (genex::contains(rhs_temporaries, sym) or memory_state::SameShape(on_short, on_rhs)) { continue; }
    syms.emplace_back(const_cast<analyse::scopes::VariableSymbol*>(sym));
  }
  auto drops = drop_utils::JoinPaths(
    syms, {{&short_circuit, Lhs.get(), Lhs.get()}, {nullptr, Rhs.get(), Rhs.get()}}, owners, sm, meta);
  _DropsOnShortCircuit = std::move(drops[0]);
  _DropsAfterRhs = std::move(drops[1]);
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

  // The short-circuit edge drops what only the right operand would
  // have taken ("_DropsOnShortCircuit"), in a block of its own. The
  // edge into the join is taken from where the builder is, as the
  // left operand may have left it in a block of its own making (a
  // "case" on the left of an "and").
  auto short_end_bb = ctx->Builder.GetInsertBlock();
  auto short_bb = join_bb;
  if (not _DropsOnShortCircuit.empty()) {
    short_bb = llvm::BasicBlock::Create(*ctx->Context, "logical.short" + uid, func);
  }
  ctx->Builder.CreateCondBr(
    llvm_lhs, is_and ? rhs_bb : short_bb, is_and ? short_bb : rhs_bb);
  if (short_bb != join_bb) {
    ctx->Builder.SetInsertPoint(short_bb);
    codegen::EmitOuterDrops(_DropsOnShortCircuit, *this, sm, meta, ctx);
    short_end_bb = ctx->Builder.GetInsertBlock();
    ctx->Builder.CreateBr(join_bb);
  }

  // Do the right-hand-side codegen (doesn't execute the LLVM,
  // just creates it), and hook it into the short-circuiting
  // mechanism.
  // The temporaries the right operand makes exist only on its
  // path, not the short-circuit one: they are dropped at its end,
  // its value being a "Bool" that borrows none of them.
  ctx->Builder.SetInsertPoint(rhs_bb);
  const auto temporaries = codegen::TemporaryDepth(ctx);
  auto llvm_rhs = static_cast<llvm::Value*>(nullptr);
  _InRhsScope(*sm, [&] { llvm_rhs = Rhs->Stage11_CodeGen(sm, meta, ctx); });
  SPP_ASSERT(llvm_rhs->getType()->isIntegerTy(1));
  auto &home = *sm->CurrentScope->TemporaryHome();
  if (_HeldWhenTrue.empty()) {
    codegen::EndTemporaryRegion(temporaries, home, _DropsRhsTemporaries, Rhs.get(), sm, meta, ctx);
  }
  else {
    // Held where the "and" is true: dropped on the right operand's
    // false edge only.
    codegen::EndTemporaryRegion(temporaries, home, {}, Rhs.get(), sm, meta, ctx);
    if (not _DropsRhsTemporaries.empty() and not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      const auto false_bb = llvm::BasicBlock::Create(*ctx->Context, "logical.rhs_false" + uid, func);
      const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "logical.rhs_done" + uid, func);
      ctx->Builder.CreateCondBr(llvm_rhs, done_bb, false_bb);
      ctx->Builder.SetInsertPoint(false_bb);
      codegen::EmitOwnedDrops(home, _DropsRhsTemporaries, Rhs.get(), sm, meta, ctx);
      ctx->Builder.CreateBr(done_bb);
      ctx->Builder.SetInsertPoint(done_bb);
    }
  }
  codegen::EmitOuterDrops(_DropsAfterRhs, *this, sm, meta, ctx);
  const auto rhs_end_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(join_bb);

  // The short-circuit edge carries the answer the left operand
  // already gave: "false" for an "and", "true" for an "or". The
  // other edge carries whatever the right operand evaluated to.
  ctx->Builder.SetInsertPoint(join_bb);
  const auto llvm_res = ctx->Builder.CreatePHI(llvm_bool_ty, 2, "logical" + uid);
  llvm_res->addIncoming(llvm::ConstantInt::getBool(*ctx->Context, not is_and), short_end_bb);
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
    const auto runs_rhs = lhs_is_true == (TokOp->TokenType == lex::SppTokenType::KW_AND);

    // Evaluated in walk order, the walk goes into the right operand's
    // scope (or past it, when the left operand decided). Evaluated
    // again out of order (a boolean "case"'s condition, re-read in its
    // branch), it is evaluated by a walk of its own, standing there.
    auto next = sm->Position().It;
    const auto in_order = _RhsScope != nullptr and next != analyse::scopes::ScopeIterator()
      and *++next == _RhsScope;
    if (_RhsScope == nullptr) {
      if (runs_rhs) { Rhs->EvaluateCompTime(sm, meta); }
    }
    else if (in_order and not runs_rhs) {
      sm->MoveToNextScope();
      ignore = sm->SkipSubtree(sm->CurrentScope);
      sm->MoveOutOfCurrentScope();
    }
    else if (in_order) {
      _InRhsScope(*sm, [&] { Rhs->EvaluateCompTime(sm, meta); });
    }
    else if (runs_rhs) {
      auto walk = sm->NewManagerAt(_RhsScope);
      _DeclareLhsBound(*_RhsScope);
      Rhs->EvaluateCompTime(&walk, meta);
    }
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
