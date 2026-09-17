module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.assignment_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.assignment_utils;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.lex.tokens;
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
  return Lhs.Front()->PosStart();
}

auto AssignmentStatementAst::PosEnd() const -> std::size_t {
  // Use the rightmost assignment value.
  return Rhs.Back()->PosEnd();
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
  using analyse::errors::SppInvalidMutationError;
  using analyse::errors::SppTypeMismatchError;
  using analyse::utils::assignment_utils::IsAttr;
  using analyse::utils::assignment_utils::IsDeref;
  using analyse::utils::assignment_utils::IsIdentifier;
  using analyse::utils::type_compare::TypeEq;

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
    return sm->CurrentScope->GetVarSymbolOutermost(*x);
  });

  // Full mutation checks, for identifiers, attribute
  // accesses, mutable/immutable values, mutable/immutable
  // borrows.
  for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(
         Lhs | genex::views::ptr,
         Rhs | genex::views::ptr,
         lhs_syms) | genex::to<Vec>()) {
    auto const &[lhs_sym, _] = lhs_sym_and_scope;
    const auto lhs_type = lhs_expr->InferType(sm, meta);
    const auto lhs_deref_ref = IsDeref(lhs_expr)
      ? lhs_expr->To<PostfixExpressionAst>()->Lhs->InferTypeRef(sm, meta)
      : TypeRef{};

    // Full assignment (ie "x" = "y") requires the "x" symbol to
    // be marked as "mut" or never initialized.
    RaiseIf<SppInvalidMutationError>(
      IsIdentifier(lhs_expr) and not(lhs_sym->IsMutable or lhs_sym->MemInfo->InitializationCounter == 0),
      {sm->CurrentScope},
      ERR_ARGS(*lhs_sym->Name, *TokAssign, *spp::get<0>(lhs_sym->MemInfo->AstInitialization), "immutable sym"));

    // Attribute assignment (ie "x.y = z"), for a non-borrowed
    // symbol, requires an outermost "mut" symbol.
    RaiseIf<SppInvalidMutationError>(
      IsAttr(lhs_expr, sm) and not(spp::get<0>(lhs_sym->MemInfo->AstBorrowed) or lhs_sym->IsMutable),
      {sm->CurrentScope},
      ERR_ARGS(*lhs_sym->Name, *TokAssign, *spp::get<0>(lhs_sym->MemInfo->AstInitialization), "immutable outer sym"));

    // Attribute assignment (ie "x.y = z"), for a borrowed symbol,
    // cannot be immutably borrowed.
    RaiseIf<SppInvalidMutationError>(
      IsAttr(lhs_expr, sm) and lhs_sym->Type->GetConvention() and *lhs_sym->Type->GetConvention() == ConventionTag::REF,
      {sm->CurrentScope},
      ERR_ARGS(*lhs_sym->Name, *TokAssign, *spp::get<0>(lhs_sym->MemInfo->AstInitialization), "immutable borrow"));

    // Dereference assignment (ie "x@ = y") writes through a
    // borrow, so the borrow being dereferenced must be &mut.
    RaiseIf<SppInvalidMutationError>(
      IsDeref(lhs_expr) and lhs_deref_ref.IsBorrowed() and lhs_deref_ref.Conv != ConventionTag::MUT,
      {sm->CurrentScope},
      ERR_ARGS(*lhs_expr, *TokAssign, *lhs_expr, "immutable index or slice"));

    // Prevent double initializations to immutable uninitialized
    // let statements.
    if (IsIdentifier(lhs_expr)) {
      lhs_sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    }

    // Ensure the lhs and rhs have the same type.
    auto rhs_type = rhs_expr->InferType(sm, meta);
    RaiseIf<SppTypeMismatchError>(
      not TypeEq(*lhs_type, *rhs_type, *sm->CurrentScope, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*lhs_expr, *lhs_type, *rhs_expr, *rhs_type));

    // A function named as the value stands for the overload the
    // target's type asks for.
    analyse::utils::func_utils::InstantiateFunctionValue(
      TypeRef::Of(*rhs_type, *sm->CurrentScope),
      TypeRef::Of(*lhs_type, *sm->CurrentScope), sm, meta);
  }
}

auto AssignmentStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::utils::assignment_utils::IsAttr;
  using analyse::utils::assignment_utils::IsIdentifier;
  using analyse::utils::mem_utils::PreventBorrowLifetimeExtension;
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // For each assignment, check the memory status and resolve
  // any (partial-)moves.
  auto lhs_syms = Lhs | genex::views::transform([sm](auto const &x) {
    return sm->CurrentScope->GetVarSymbolOutermost(*x);
  }) | genex::to<Vec>();

  for (auto const &[lhs_expr, rhs_expr, lhs_sym_and_scope] : genex::views::zip(
         Lhs | genex::views::ptr, Rhs | genex::views::ptr, lhs_syms)) {
    auto const &[lhs_sym, _] = lhs_sym_and_scope;

    // Partially validate the memory of the right-hand-side
    // expression, if it is an attribute being set. Don't mark
    // the move, but do some checks before calling the internal
    // memory checker on the postfix expression.
    ValidateSymbolMemory(*rhs_expr, *TokAssign, *sm, IsAttr(lhs_expr, sm), false, true, false, meta);

    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = AstCloneShared(lhs_expr->To<IdentifierAst>());
      meta->AssignmentTargetType = lhs_expr->InferType(sm, meta);
      rhs_expr->Stage8_CheckMemory(sm, meta);
    }

    // Fully validate the memory of the right-hand-side
    // expression, marking the move. A value carrying escaping
    // borrows is let through here, because the function
    // "PreventBorrowLifetimeExtension" below weighs the
    // destination against those borrows rather than refusing
    // the move on sight.
    ValidateSymbolMemory(
      *rhs_expr, *TokAssign, *sm, true, true, true, true, meta, false);

    // For an attribute-based left-hand-side, we ensure that
    // the object is valid and mark it as being written in
    // place, to fine tune the memory error system. Resolve
    // the partial move.
    if (IsAttr(lhs_expr, sm)) {
      ValidateSymbolMemory(
        *lhs_expr, *TokAssign, *sm, true, true, false, false, meta, true, true);
      lhs_sym->MemInfo->RemovePartialMoves(*lhs_expr, sm->CurrentScope);
    }

    // Otherwise, resolve the moved identifier's memory status
    // to the "initialised" state.
    else if (IsIdentifier(lhs_expr)) {
      lhs_sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    }

    // Ensure a borrow is not increasing its lifetime.
    const auto lhs_outermost = sm->CurrentScope->GetVarSymbolOutermost(*lhs_expr).first;
    const auto rhs_outermost = sm->CurrentScope->GetVarSymbolOutermost(*rhs_expr).first;
    PreventBorrowLifetimeExtension(
      *rhs_expr, lhs_outermost, rhs_outermost, this, *sm);
  }
}

auto AssignmentStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::utils::assignment_utils::IsAttr;
  using analyse::utils::assignment_utils::IsIdentifier;
  using analyse::utils::cmp_utils::SetCompTimeAttrValue;

  // Wrap the rhs value and move it into the value of the
  // variable symbol.
  for (auto i = 0uz; i < Lhs.Len(); ++i) {
    Rhs[i]->Stage9_CompTimeResolve(sm, meta);
    const auto lhs_sym = sm->CurrentScope->GetVarSymbolOutermost(*Lhs[i]).first;

    // Assign to a full identifier.
    if (IsIdentifier(Lhs[i].get())) {
      lhs_sym->CompTimeValue = std::move(meta->CmpResult);
    }

    // Assign to an attribute.
    else if (IsAttr(Lhs[i].get(), sm)) {
      SetCompTimeAttrValue(
        lhs_sym->CompTimeValue->To<ObjectInitializerAst>(),
        Lhs[i].get(), std::move(meta->CmpResult), sm);
    }

    // Otherwise, unsupported in the comptime context.
    else {
      Raise<analyse::errors::SppInvalidComptimeOperationError>(
        {sm->CurrentScope}, ERR_ARGS(*Lhs[i]));
    }
  }
}

auto AssignmentStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  using analyse::utils::assignment_utils::IsDeref;
  using analyse::utils::assignment_utils::IsIdentifier;

  // Use a 2-pass system to ensure that "a, b = b, a" is supported
  // and doesn't clobber the values being reused. Firstly generate
  // all the right-hand-side values into llvm ir.
  auto llvm_rhs_vals = Vec<llvm::Value*>{};
  llvm_rhs_vals.Reserve(Rhs.Len());
  for (auto i = 0uz; i < Rhs.Len(); ++i) {
    auto llvm_rhs = [&] {
      const auto _meta_guard = MetaGuard(meta);
      meta->AssignmentTarget = AstCloneShared(Lhs[i]->To<IdentifierAst>());
      meta->AssignmentTargetType = Lhs[i]->InferType(sm, meta);
      if (IsIdentifier(Lhs[i].get())) {
        meta->LlvmAssignmentTarget = sm->CurrentScope->GetVarSymbol(
          Lhs[i]->To<IdentifierAst>())->LlvmInfo->Alloca;
      }

      auto value = Rhs[i]->Stage11_CodeGen(sm, meta, ctx);

      // Just like a "let" with a declared type: the target may be
      // a variant the value is only a member of, in which case it
      // is tagged and copied into the payload rather than written
      // raw over the slot (which would land the member on top of
      // the tag).
      if (const auto target_type = Lhs[i]->InferType(sm, meta); target_type != nullptr) {
        value = codegen::CoerceToFunctionValue(
          value, TypeRef::Of(*target_type, *sm->CurrentScope),
          Rhs[i]->InferTypeRef(sm, meta), *sm, ctx);

        value = codegen::CoerceToVariant(
          value, TypeRef::Of(*target_type, *sm->CurrentScope),
          Rhs[i]->InferTypeRef(sm, meta), *sm->CurrentScope,
          "assign.variant." + spp::utils::Uid(this), ctx);
      }

      return value;
    }();
    llvm_rhs_vals.EmplaceBack(llvm_rhs);
  }

  // Resolve every LHS store location, still before any stores happen.
  // This finalizes the expression swapping support.
  auto llvm_lhs_locs = Vec<llvm::Value*>{};
  llvm_lhs_locs.Reserve(Lhs.Len());
  for (auto i = 0uz; i < Lhs.Len(); ++i) {
    auto llvm_lhs = static_cast<llvm::Value*>(nullptr);

    // The statement "x@ = v" writes through a borrow: the target is
    // the borrow pointer itself.
    if (IsDeref(Lhs[i].get())) {
      const auto inner = Lhs[i]->To<PostfixExpressionAst>()->Lhs.get();
      llvm_lhs = inner->Stage11_CodeGen(sm, meta, ctx);
    }

    // The statement "a = v" targets the variable's allocation directly
    // (loading it would yield the rvalue).
    else if (IsIdentifier(Lhs[i].get())) {
      const auto var_sym = sm->CurrentScope->GetVarSymbol(
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

    llvm_lhs_locs.EmplaceBack(llvm_lhs);
  }

  // Now that every value and location has been computed off the
  // (pre-assignment) state, commit the stores.
  for (auto i = 0uz; i < Lhs.Len(); ++i) {
    if (llvm_lhs_locs[i] == nullptr and llvm_rhs_vals[i] == nullptr) { continue; }
    SPP_ASSERT(llvm_lhs_locs[i] != nullptr and llvm_rhs_vals[i] != nullptr);
    ctx->Builder.CreateStore(llvm_rhs_vals[i], llvm_lhs_locs[i]);
  }

  // Statements are always generated into a builder so no need
  // to return anything.
  return nullptr;
}

SPP_MOD_END
