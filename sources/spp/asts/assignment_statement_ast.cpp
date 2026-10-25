module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.assignment_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.borrows;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_compare;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.ice;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
AssignmentStatementAst::AssignmentStatementAst(
  decltype(Lhs) &&lhs,
  decltype(TokAssign) &&tok_assign,
  decltype(Rhs) &&rhs) :
  Lhs(std::move(lhs)),
  TokAssign(std::move(tok_assign)),
  Rhs(std::move(rhs)) {
  // Default the assignment token.
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokAssign, SppTokenType::TK_ASSIGN, "=");
}

AssignmentStatementAst::~AssignmentStatementAst() = default;

auto AssignmentStatementAst::PosStart() const -> std::size_t {
  // Use the leftmost assignment target.
  return Lhs.front()->PosStart();
}

auto AssignmentStatementAst::PosEnd() const -> std::size_t {
  // Use the rightmost assignment value.
  return Rhs.back()->PosEnd();
}

auto AssignmentStatementAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<AssignmentStatementAst>(
    AstCloneVec(Lhs),
    AstClone(TokAssign),
    AstCloneVec(Rhs));
}

auto AssignmentStatementAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Lhs, ", ");
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_APPEND(TokAssign);
  SPP_STRING_APPEND_RAW(" ");
  SPP_STRING_EXTEND(Rhs, ", ");
  SPP_STRING_END;
}

auto AssignmentStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // For each part of the LHS, ensure it is semantically valid.
  // Use the deref helper to allow a "move"-looking ast on the
  // left side.
  for (auto const &lhs_expr : Lhs) {
    SPP_DEREF_ALLOW_MOVE_HELPER(lhs_expr) {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowMoveDeref = true;
      lhs_expr->Stage7_AnalyseSemantics(sm, meta);
    }
    else {
      lhs_expr->Stage7_AnalyseSemantics(sm, meta);
    }
  }

  // For each part of the RHS, ensure it is semantically valid.
  for (auto [i, rhs_expr] : Rhs | genex::views::ptr | genex::views::enumerate) {
    const auto _meta_guard = MetaGuard(meta);

    // Handle return type overloading matching from the lhs
    // elem type. Todo: RHS not analysed yet -> bad expr being
    // inferred? Maybe make failures on inference be nullptr.
    SPP_RETURN_TYPE_OVERLOAD_HELPER(rhs_expr) {
      meta->ReturnTypeOverloadResolverType = MakeShared<TypeRef>(
        Lhs[i]->InferTypeRef(sm, meta));
    }

    // Analyse the RHS expression. Bind the lhs target and
    // target type for each RHS element.
    meta->AssignmentTarget = AstCloneShared(Lhs[i]->To<IdentifierAst>());
    meta->AssignmentTargetType = Lhs[i]->InferType(sm, meta);
    rhs_expr->Stage7_AnalyseSemantics(sm, meta);
  }

  // For each assignment, get the outermost symbol of the
  // expression.
  auto lhs_syms = Lhs | genex::views::transform([sm](auto const &x) {
    return sm->CurrentScope->FindVarSymbolOutermost(*x);
  });

  // Full mutation checks, for identifiers, attribute
  // accesses, mutable/immutable values, mutable/immutable
  // borrows.
  for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(
         Lhs | genex::views::ptr,
         Rhs | genex::views::ptr,
         lhs_syms) | genex::to<Vec>()) {
    auto const &[lhs_sym, _] = lhs_sym_and_scope;
    const auto lhs_state = [&]() -> analyse::utils::memory_state::MemoryState& {
      return analyse::utils::memory_state::StateOf(*sm->CurrentScope, *lhs_sym);
    };
    const auto lhs_type = lhs_expr->InferType(sm, meta);
    const auto lhs_deref_ref = IsDeref(lhs_expr)
      ? lhs_expr->To<PostfixExpressionAst>()->Lhs->InferTypeRef(sm, meta)
      : TypeRef{};

    // Attribute assignment (ie "x.y = z"), for a non-borrowed
    // symbol, requires an outermost "mut" symbol.
    RaiseIf<SppInvalidMutationError>(
      regions::IsAttr(lhs_expr, sm) and not(lhs_sym->MemInfo->AstBorrowed or lhs_sym->IsMutable),
      ERR_ARGS(*lhs_sym->Name, *TokAssign, *lhs_state().AstInitialization, "immutable outer sym"));

    // Attribute assignment (ie "x.y = z"), for a borrowed symbol,
    // cannot be immutably borrowed.
    RaiseIf<SppInvalidMutationError>(
      regions::IsAttr(lhs_expr, sm) and lhs_sym->Type->GetConvention() and *lhs_sym->Type->GetConvention() ==
      ConventionTag::REF,
      ERR_ARGS(*lhs_sym->Name, *TokAssign, *lhs_state().AstInitialization, "immutable borrow"));

    // Dereference assignment (ie "x@ = y") writes through a
    // borrow, so the borrow being dereferenced must be &mut.
    RaiseIf<SppInvalidMutationError>(
      IsDeref(lhs_expr) and lhs_deref_ref.IsBorrowed() and lhs_deref_ref.Conv != ConventionTag::MUT,
      ERR_ARGS(*lhs_expr, *TokAssign, *lhs_expr, "immutable index or slice"));

    // Ensure the lhs and rhs have the same type.
    // Compared as resolved, each where it was inferred; the written forms are only for the message.
    const auto lhs_ref = lhs_expr->InferTypeRef(sm, meta);
    const auto rhs_ref = rhs_expr->InferTypeRef(sm, meta);
    const auto rhs_type = rhs_expr->InferType(sm, meta);
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(lhs_ref, rhs_ref, *sm->CurrentScope, *sm->CurrentScope),
      ERR_ARGS(*lhs_expr, *lhs_type, *rhs_expr, *rhs_type));

    // A function named as the value stands for the overload the
    // target's type asks for.
    fn_values::InstantiateFnValue(
      rhs_ref, lhs_ref, sm, meta);
  }
}

auto AssignmentStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using namespace spp::analyse::errors;
  using namespace spp::analyse::utils;

  // For each assignment, check the memory status and resolve
  // any (partial-)moves.
  auto lhs_syms = Lhs | genex::views::transform([sm](auto const &x) {
    return sm->CurrentScope->FindVarSymbolOutermost(*x);
  }) | genex::to<Vec>();

  for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(
         Lhs | genex::views::ptr, Rhs | genex::views::ptr, lhs_syms)) {
    auto const &[lhs_sym, _] = lhs_sym_and_scope;
    const auto lhs_state = [&]() -> memory_state::MemoryState& {
      return memory_state::StateOf(*sm->CurrentScope, *lhs_sym);
    };

    // Partially validate the memory of the right-hand-side
    // expression, if it is an attribute being set. Don't mark
    // the move, but do some checks before calling the internal
    // memory checker on the postfix expression.
    mem_utils::ValidateSymbolMemory(
      *rhs_expr, *TokAssign, *sm, meta,
      {.CheckMove = regions::IsAttr(lhs_expr, sm), .CheckPartialMove = false, .MarkMoves = false});

    // When we assign a replacement value, we need to release
    // the loans that were present on the symbol. Get the loans
    // in the scope, and remove them by comparing the holder
    // symbol.
    auto &loans = memory_state::LoansOf(*sm->CurrentScope);
    if (lhs_expr->To<IdentifierAst>() != nullptr and lhs_sym != nullptr) {
      loans |= genex::actions::remove_if([&](auto const &loan) {
        return loan.Holder == lhs_sym and not loan.Yielded;
      });
    }

    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = AstCloneShared(lhs_expr->To<IdentifierAst>());
      meta->AssignmentTargetType = lhs_expr->InferType(sm, meta);
      meta->AssignmentTargetSymbol = lhs_expr->To<IdentifierAst>() != nullptr ? lhs_sym : nullptr;
      rhs_expr->Stage8_CheckMemory(sm, meta);
    }

    // For a left-hand-side identifier (symbolic), activate any
    // loans (escaping borrows) on it.
    if (lhs_expr->To<IdentifierAst>() != nullptr and lhs_sym != nullptr) {
      memory_state::ActivateLoans(loans, *lhs_sym);
    }

    // Fully validate the memory of the right-hand-side
    // expression, marking the move. A value carrying loans is
    // let through here, because "EnforceBorrowsOutliveHolder"
    // below weighs the destination against those loans rather
    // than refusing the move on sight.
    mem_utils::ValidateSymbolMemory(
      *rhs_expr, *TokAssign, *sm, meta, {.CheckEscapingBorrowMove = false});

    // Writing over a value that a live coroutine, future or
    // iterator borrows changes what that borrow points at, just
    // as moving it out would, and is refused the same way.
    if (regions::IsAttr(lhs_expr, sm) or lhs_expr->To<IdentifierAst>() != nullptr) {
      for (auto &loan : loans) {
        if (loan.Pending or not loan.Borrowed.Of.has_value() or loan.Borrowed.Of->Root != lhs_sym) { continue; }
        borrows::ConflictWith(
          loan, memory_state::LoanConflict{.Moved = true, .First = loan.Holder->Name.get(), .Second = lhs_expr}, *sm);
      }
    }

    // For an attribute-based left-hand-side, we ensure that
    // the object is valid and mark it as being written in
    // place, to fine tune the memory error system. Resolve
    // the partial move. Check we are abiding by linear rules
    // by not overwriting a linear value (not discarded).
    if (regions::IsAttr(lhs_expr, sm)) {
      mem_utils::ValidateSymbolMemory(
        *lhs_expr, *TokAssign, *sm, meta,
        {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false, .IsPlaceWritten = true});
      linear_utils::CheckOverwritePart(*lhs_sym, *lhs_expr, *this, "Assignment", *sm, meta);
      lhs_state().RemovePartialMoves(*lhs_expr, sm->CurrentScope);
    }

    // Writing through a dereference ("x@ = ..") writes over
    // whatever the borrow points at, and uses the borrow.
    else if (IsDeref(lhs_expr)) {
      if (const auto borrow = lhs_expr->To<PostfixExpressionAst>()->Lhs->To<IdentifierAst>()) {
        if (const auto borrow_sym = sm->CurrentScope->FindVarSymbol(borrow)) {
          borrows::RaiseIfConflictedUse(*borrow_sym, *sm->CurrentScope);
        }
      }
      linear_utils::CheckOverwriteThroughBorrow(*lhs_expr, *sm, meta);
    }

    // Otherwise, resolve the moved identifier's memory status
    // to the "initialised" state.
    else if (lhs_expr->To<IdentifierAst>() != nullptr) {
      // An immutable may be given its value once: only if no path
      // here has given it one yet. A value held now, on some path
      // only, or since moved on, was a first one (a loop body's
      // second time round included).
      auto const &state = lhs_state();
      const auto never_given = not state.Given
        and state.AstInitialization == nullptr
        and not state.MaybeInit;

      RaiseIf<SppInvalidMutationError>(
        not lhs_sym->IsMutable and not never_given,
        ERR_ARGS(
          *lhs_sym->Name, *TokAssign,
          *(state.AstInitialization != nullptr ? state.AstInitialization : state.AstMoved), "immutable sym"));

      linear_utils::CheckOverwrite(*lhs_sym, *this, "Assignment", *sm);
      lhs_state().InitializedBy(*this);
    }

    // A holder moved in whole by name hands its loans to the name
    // it is moved into.
    const auto lhs_outermost = sm->CurrentScope->FindVarSymbolOutermost(*lhs_expr).first;
    const auto rhs_outermost = sm->CurrentScope->FindVarSymbolOutermost(*rhs_expr).first;
    if (lhs_expr->To<IdentifierAst>() != nullptr and rhs_expr->To<IdentifierAst>() != nullptr
      and lhs_sym != nullptr and rhs_outermost != nullptr) {
      memory_state::TransferLoans(loans, *rhs_outermost, *lhs_sym);
    }

    // What the value borrows has to outlive what now holds it.
    if (lhs_outermost != nullptr) {
      borrows::EnforceBorrowsOutliveHolder(*lhs_outermost, rhs_outermost, *this, *sm);
    }
  }
}

auto AssignmentStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Wrap the rhs value and move it into the value of the
  // variable symbol.
  for (auto i = 0uz; i < Lhs.size(); ++i) {
    Rhs[i]->Stage9_CompTimeResolve(sm, meta);
    const auto lhs_sym = sm->CurrentScope->FindVarSymbolOutermost(*Lhs[i]).first;

    // Assign to a full identifier.
    if (Lhs[i]->To<IdentifierAst>() != nullptr) {
      lhs_sym->CompTimeValue = std::move(meta->CompTimeResult);
    }

    // Assign to an attribute.
    else if (regions::IsAttr(Lhs[i].get(), sm)) {
      comp_time_intrinsics::SetCompTimeAttrValue(
        lhs_sym->CompTimeValue->To<ObjectInitializerAst>(),
        Lhs[i].get(), std::move(meta->CompTimeResult), sm);
    }

    // Otherwise, unsupported in the comptime context.
    else {
      Raise<SppInvalidCompTimeOperationError>(ERR_ARGS(*Lhs[i]));
    }
  }
}

auto AssignmentStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // Use a 2-pass system to ensure that "a, b = b, a" is supported
  // and doesn't clobber the values being reused. Firstly generate
  // all the right-hand-side values into llvm ir.
  auto llvm_rhs_vals = Vec<llvm::Value*>{};
  llvm_rhs_vals.reserve(Rhs.size());
  for (auto i = 0uz; i < Rhs.size(); ++i) {
    auto llvm_rhs = [&] {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = AstCloneShared(Lhs[i]->To<IdentifierAst>());
      meta->AssignmentTargetType = Lhs[i]->InferType(sm, meta);
      if (Lhs[i]->To<IdentifierAst>() != nullptr) {
        meta->LlvmAssignmentTarget = sm->CurrentScope->FindVarSymbol(
          Lhs[i]->To<IdentifierAst>())->LlvmInfo->Alloca;
      }

      auto value = Rhs[i]->Stage11_CodeGen(sm, meta, ctx);

      // Just like a "let" with a declared type: the target may be
      // a variant the value is only a member of, in which case it
      // is tagged and copied into the payload rather than written
      // raw over the slot (which would land the member on top of
      // the tag).
      const auto target_ref = Lhs[i]->InferTypeRef(sm, meta);
      if (target_ref.Symbol != nullptr) {
        value = codegen::CoerceToSlot(
          value, target_ref, Rhs[i]->InferTypeRef(sm, meta), *sm, "assign.variant." + Uid(), ctx);
      }

      return value;
    }();
    llvm_rhs_vals.emplace_back(llvm_rhs);
  }

  // Resolve every LHS store location, still before any stores happen.
  // This finalizes the expression swapping support.
  auto llvm_lhs_locs = Vec<llvm::Value*>{};
  llvm_lhs_locs.reserve(Lhs.size());
  for (auto i = 0uz; i < Lhs.size(); ++i) {
    auto llvm_lhs = static_cast<llvm::Value*>(nullptr);

    // The statement "x@ = v" writes through a borrow: the target is
    // the borrow pointer itself.
    if (IsDeref(Lhs[i].get())) {
      const auto inner = Lhs[i]->To<PostfixExpressionAst>()->Lhs.get();
      llvm_lhs = inner->Stage11_CodeGen(sm, meta, ctx);
    }

    // The statement "a = v" targets the variable's allocation directly
    // (loading it would yield the rvalue).
    else if (Lhs[i]->To<IdentifierAst>() != nullptr) {
      const auto var_sym = sm->CurrentScope->FindVarSymbol(
        Lhs[i]->To<IdentifierAst>());
      SPP_ASSERT(var_sym->LlvmInfo->Alloca != nullptr);
      llvm_lhs = var_sym->LlvmInfo->Alloca;
    }

    // The statement "x.y = v" (attribute): ask the runtime member
    // access for the field's address rather than its value.
    else {
      const auto _meta_guard = MetaGuard(meta);
      meta->LlvmWantAddress = true;
      llvm_lhs = Lhs[i]->Stage11_CodeGen(sm, meta, ctx);
    }

    llvm_lhs_locs.emplace_back(llvm_lhs);
  }

  // Now that every value and location has been computed off the
  // (pre-assignment) state, commit the stores.
  for (auto i = 0uz; i < Lhs.size(); ++i) {
    if (llvm_lhs_locs[i] == nullptr and llvm_rhs_vals[i] == nullptr) { continue; }
    SPP_ASSERT(llvm_lhs_locs[i] != nullptr and llvm_rhs_vals[i] != nullptr);
    ctx->Builder.CreateStore(llvm_rhs_vals[i], llvm_lhs_locs[i]);
  }

  // Statements are always generated into a builder so no need
  // to return anything.
  return nullptr;
}

SPP_MOD_END
