module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_call_argument_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.borrows;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.regions;
import spp.analyse.utils.type_predicates;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.convention_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;
import sys;

SPP_MOD_BEGIN
auto FunctionCallArgumentGroupAst::NewEmpty() -> Unique<FunctionCallArgumentGroupAst> {
  // Empty ast.
  return MakeUnique<FunctionCallArgumentGroupAst>(
    nullptr, decltype(Args)(), nullptr);
}

FunctionCallArgumentGroupAst::FunctionCallArgumentGroupAst(
  decltype(TokL) &&tok_l,
  decltype(Args) &&args,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Args(std::move(args)),
  TokR(std::move(tok_r)) {
}

FunctionCallArgumentGroupAst::~FunctionCallArgumentGroupAst() = default;

auto FunctionCallArgumentGroupAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Args.empty() ? 0 : Args.front()->PosStart();
}

auto FunctionCallArgumentGroupAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Args.empty() ? 0 : Args.back()->PosEnd();
}

auto FunctionCallArgumentGroupAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<FunctionCallArgumentGroupAst>(
    AstClone(TokL),
    AstCloneVec(Args),
    AstClone(TokR));
}

auto FunctionCallArgumentGroupAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("(");
  SPP_STRING_EXTEND(Args, ", ");
  SPP_STRING_APPEND_RAW(")");
  SPP_STRING_END;
}

auto FunctionCallArgumentGroupAst::GetAllArgs() const -> Vec<FunctionCallArgumentAst*> {
  // Filter by casting.
  auto out = Vec<FunctionCallArgumentAst*>();
  for (auto const &arg : Args) {
    out.emplace_back(arg.get());
  }
  return out;
}

auto FunctionCallArgumentGroupAst::GetKeywordArgs() const -> Vec<FunctionCallArgumentKeywordAst*> {
  // Filter by casting.
  auto out = Vec<FunctionCallArgumentKeywordAst*>();
  for (auto const &arg : Args) {
    if (auto *kw_arg = arg->To<FunctionCallArgumentKeywordAst>(); kw_arg != nullptr) {
      out.emplace_back(kw_arg);
    }
  }
  return out;
}

auto FunctionCallArgumentGroupAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check there are no duplicate argument names.
  const auto arg_names = GetKeywordArgs()
    | genex::views::transform([](auto const &x) { return x->Name.get(); })
    | genex::to<Vec>()
    | genex::views::duplicates({}, genex::meta::deref)
    | genex::to<Vec>();

  RaiseIf<SppIdentifierDuplicateError>(
    not arg_names.empty(),
    ERR_ARGS(*arg_names[0], *arg_names[1], "keyword function-argument"));

  // Check the arguments are in the correct order.
  const auto unordered_args = order_utils::DoOrderArgs(Args
    | genex::views::ptr
    | genex::views::cast_dynamic<mixins::OrderableAst*>()
    | genex::to<Vec>());

  RaiseIf<SppOrderInvalidError>(
    not unordered_args.empty(),
    ERR_ARGS(unordered_args[1].first, *unordered_args[1].second, unordered_args[0].first, *unordered_args[0].second));

  // Expand tuple-expansion arguments ("..tuple" =>
  // "tuple.0, tuple.1, ..."). Must use "materialize" because
  // the list gets updates from within the loop.
  for (auto const &[i, arg] : Args | genex::views::ptr | genex::views::enumerate | genex::to<Vec>()) {
    // Only check position arguments that have ".." tokens.
    const auto pos_arg = arg->To<FunctionCallArgumentPositionalAst>();
    if (pos_arg == nullptr or pos_arg->TokUnpack == nullptr) { continue; }

    // Check the argument value is a tuple expression.
    const auto arg_ref = arg->InferTypeRef(sm, meta);
    if (not type_predicates::IsTypeTuple(arg_ref, *sm->CurrentScope)) {
      const auto arg_type = arg->InferType(sm, meta);
      Raise<SppExpansionOfNonTupleError>(ERR_ARGS(*pos_arg->TokUnpack, *arg->Val, *arg_type));
    }

    // Replace the tuple-expansion argument with the expanded
    // arguments.
    const auto max = static_cast<sys::ssize_t>(arg_ref.Symbol->TypeArgRefs().size());
    for (auto j = max - 1; j > -1z; --j) {
      auto field = MakeUnique<IdentifierAst>(arg->Val->PosStart(), std::to_string(j));
      auto new_ast = MakeUnique<PostfixExpressionAst>(
        AstClone(arg->Val),
        MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
      auto new_arg = MakeUnique<FunctionCallArgumentPositionalAst>(AstClone(arg->Conv), nullptr, std::move(new_ast));
      Args.insert(Args.begin() + static_cast<std::ptrdiff_t>(i), std::move(new_arg));
    }
    genex::actions::erase(Args, Args.begin() + static_cast<std::ptrdiff_t>(i) + max);
  }

  // Analyse the arguments. The immutability/borrow mutation
  // checks are deferred to Stage8, because the "self" argument's
  // convention (for method calls) is only applied after overload
  // resolution.
  for (auto const &[i, arg] : Args | genex::views::ptr | genex::views::enumerate) {
    const auto _meta_guard = MetaGuard(meta);
    SPP_RETURN_TYPE_OVERLOAD_HELPER(arg->Val.get()) {
      meta->ReturnTypeOverloadResolverType = i < ExpectedTypes.size() ? ExpectedTypes[i] : nullptr;
    }
    arg->Stage7_AnalyseSemantics(sm, meta);
  }
}

auto FunctionCallArgumentGroupAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // If the target is a coroutine, or the target is called
  // as "async", then pins are required.
  const auto call = meta->TargetCall;
  auto target = call != nullptr ? call->ResolvedProto() : nullptr;
  const auto is_target_coro = target != nullptr and target->To<CoroutinePrototypeAst>() != nullptr;
  const auto pins_required = (call != nullptr and call->IsAsync()) or is_target_coro;

  // The law of exclusivity: the borrows the arguments make are held by the call while it runs ("call_loans"), and
  // are checked against each other and against every loan already held in the body.
  auto &loans = memory_state::LoansOf(*sm->CurrentScope);
  auto call_loans = memory_state::LoanSet();

  // Get potential handle to bind escaping borrows to.
  const auto handle = meta->AssignmentTarget;
  const auto handle_sym = mem_utils::AssignmentTargetSymbol(*sm->CurrentScope, *meta);
  const auto handle_pending = handle_sym != nullptr and handle_sym == meta->AssignmentTargetSymbol;

  // A coroutine or future created straight into a "ret" leaves
  // the function with every borrow it was given, and nothing
  // outside the function knows it holds them.
  const auto into_return = handle != nullptr and handle->Val == "$ret";
  const auto raise_if_returned = [&](FunctionCallArgumentAst const &arg) {
    if (not into_return or not pins_required) { return; }
    const auto arg_type = arg.Val->InferType(sm, meta);
    Raise<SppSecondClassBorrowViolationError>(ERR_ARGS(*arg.Val, *arg_type, "returned coroutine argument"));
  };

  for (auto const &arg : Args) {
    const auto is_receiver = call != nullptr and call->IsReceiverArg(*arg);

    // A drop ends the value it is given, and with it every loan the value holds ("drop_utils::EndsArgument"). They end
    // before the argument is read: dropping frees a frame or an environment without using what it borrowed, so a
    // conflict recorded on one of those loans is not a use, and the holder, holding nothing, can be moved into it.
    if (target != nullptr and arg->Val->To<IdentifierAst>() != nullptr
      and drop_utils::EndsArgument(*target, *arg, is_receiver, *sm, meta)) {
      if (auto *const held = sm->CurrentScope->FindVarSymbol(arg->Val->To<IdentifierAst>()); held != nullptr) {
        memory_state::ReleaseLoans(loans, *held);
      }
    }

    const auto closure_arg = arg->Val != nullptr
      ? arg->Val->To<ClosureExpressionAst>()
      : nullptr;
    const auto arg_carries_borrows = closure_arg != nullptr and closure_arg->HasBorrowedCaptures();

    const auto saved_assignment_target = meta->AssignmentTarget;
    if ((arg->Conv == nullptr or not pins_required) and not arg_carries_borrows) {
      meta->AssignmentTarget = nullptr;
    }
    arg->Stage8_CheckMemory(sm, meta);
    meta->AssignmentTarget = saved_assignment_target;

    // A closure written as an argument holds its borrowed captures while the call runs, as the call holds its other
    // arguments' borrows: a captured "&mut x" beside an argument "&mut x" is two ways to one place. Checked against
    // the call's own borrows only: the captures were checked against the body's loans as they were captured.
    if (closure_arg != nullptr) {
      for (auto const &cap : closure_arg->PcGroup->CaptureGroup->Captures) {
        if (cap->Conv == nullptr) { continue; }
        const auto is_mut = *cap->Conv == ConventionTag::MUT;
        const auto borrowed = regions::PlaceAtOf(*cap->Val, *sm->CurrentScope);
        auto none = memory_state::LoanSet();
        borrows::CheckAgainstLoans(borrowed, is_mut, none, call_loans, *cap->Val, *sm);
        call_loans.emplace_back(memory_state::MakeLoan(borrowed, is_mut));
      }
    }

    auto [sym, _] = sm->CurrentScope->FindVarSymbolOutermost(*arg->Val);

    // A method taking "self" by value consumes its receiver, so it
    // cannot be reached through a borrow ("a.take()" with "a: &mut
    // A", or the "&mut" a chaining method hands back): the value
    // would be moved out while its owner still owns it, and freed
    // twice. The checks below miss it - moving a borrow variable is
    // fine, and a chained receiver is a temporary with no symbol.
    if (is_receiver and arg->Conv == nullptr) {
      const auto self_param = target->FnParamGroup->GetSelfParam();
      const auto receiver = arg->Val->InferTypeRef(sm, meta);
      if (self_param != nullptr and self_param->Conv == nullptr and receiver.IsBorrowed()
        and receiver.Symbol != nullptr and not receiver.Symbol->IsCopyable()) {
        auto const *where_borrow = sym != nullptr and sym->MemInfo->AstBorrowed != nullptr
          ? sym->MemInfo->AstBorrowed
          : static_cast<Ast const*>(arg->Val.get());
        Raise<SppMoveFromBorrowedMemoryError>(ERR_ARGS(*arg->Val, *where_borrow, *where_borrow));
      }
    }

    // An argument that is itself a "&" borrow ("v[i]", a "&T" whatever "v" is) cannot be mutably borrowed, whatever
    // its root symbol: the borrow it is reached through says it is immutable here. A temporary has no symbol for the
    // checks below, so this reads the argument's own type.
    if (arg->Conv != nullptr and *arg->Conv == ConventionTag::MUT) {
      const auto arg_ref = arg->Val->InferTypeRef(sm, meta);
      RaiseIf<SppInvalidMutationError>(
        arg_ref.IsBorrowed() and arg_ref.Conv == ConventionTag::REF,
        ERR_ARGS(*arg->Val, *arg->Conv, *arg->Val, "immutable borrow"));
    }

    // A borrow the argument list has to keep apart, but that is named neither by a spelled convention nor by an
    // outermost symbol, and so is invisible to all three branches below. "v[mut i]" is the shape; see the note on
    // "ValidateUnnamedArgumentBorrow" for why it arrives that way and why nothing else catches it.
    borrows::ValidateUnnamedArgumentBorrow(*arg, sym, call_loans, *sm, meta);

    // An argument with no outermost symbol is a temporary, so
    // the borrow bookkeeping below has nothing to key on. But
    // in the special case that the unnamed argument is a tuple
    // or array, we need to check the elements inside it.
    if (sym == nullptr) {
      if (arg->Conv == nullptr) { mem_utils::ValidateSymbolMemory(*arg->Val, *arg, *sm, meta); }
      continue;
    }

    // Ensure the argument isn't moved or partially moved (applies to all conventions). For non-symbolic arguments,
    // nested checking is done via the argument itself (tuples, arrays, etc). Can borrow attributes so don't check
    // for moving from borrowed context right here.
    mem_utils::ValidateSymbolMemory(
      *arg->Val, *arg, *sm, meta,
      {.CheckMove = false, .CheckPartialMove = false, .CheckMoveFromBorrowedCtx = false, .MarkMoves = false});

    // An argument rooted at a holder of loans (a borrow binding passed on, a closure or generator given or borrowed)
    // hands the callee the ways to what those loans borrow: the call holds them beside its other arguments' borrows,
    // so two ways to one place never reach a call together, whatever the holder's later uses.
    for (auto const *held : memory_state::LoansHeldBy(loans, *sym)) {
      auto none = memory_state::LoanSet();
      borrows::CheckAgainstLoans(held->Borrowed, held->IsMut, none, call_loans, *arg->Val, *sm);
      call_loans.emplace_back(memory_state::MakeLoan(held->Borrowed, held->IsMut));
    }

    if (arg->Conv == nullptr) {
      // Ensure that attributes aren't being moved off of a borrowed value. Mark the move or partial move of the
      // argument. Function calls can only imply an inner scope, so it is guaranteed that lifetimes aren't being
      // extended.
      mem_utils::ValidateSymbolMemory(*arg->Val, *arg, *sm, meta);

      // A borrow passed on as it is - a borrow parameter - is held
      // by a coroutine or future just as one written "&x" is.
      if (const auto arg_ref = arg->Val->InferTypeRef(sm, meta); pins_required and arg_ref.IsBorrowed()) {
        raise_if_returned(*arg);
        if (handle_sym != nullptr) {
          loans.emplace_back(memory_state::MakeLoan(
            regions::PlaceAtOf(*arg->Val, *sm->CurrentScope), arg_ref.Conv == ConventionTag::MUT, handle_sym, false,
            handle_pending));
        }
      }

      // Check the move doesn't overlap with any borrows. This is to ensure that "f(&x, x)" can never happen,
      // because the first argument requires the owned object to outlive the function call, and moving it as the
      // second argument breaks this. Doesn't apply to copyable types.
      if (not arg->Val->InferTypeRef(sm, meta).Symbol->IsCopyable()) {
        borrows::CheckAgainstLoans(
          regions::PlaceAtOf(*arg->Val, *sm->CurrentScope), true, loans, call_loans, *arg->Val, *sm);
      }
    }

    else if (arg->Conv and *arg->Conv == ConventionTag::REF) {
      // An immutable borrow cannot overlap a mutable one.
      const auto borrowed = regions::PlaceAtOf(*arg->Val, *sm->CurrentScope);
      borrows::CheckAgainstLoans(borrowed, false, loans, call_loans, *arg->Val, *sm);

      // A coroutine or future holds the borrow past the call; the call holds it while it runs.
      raise_if_returned(*arg);
      if (handle_sym != nullptr and pins_required) {
        loans.emplace_back(memory_state::MakeLoan(borrowed, false, handle_sym, false, handle_pending));
      }
      call_loans.emplace_back(memory_state::MakeLoan(borrowed, false));
    }

    else if (arg->Conv and *arg->Conv == ConventionTag::MUT) {
      const auto is_sym_mutable = sym->IsMutable or (
        sym->Type->GetConvention() and *sym->Type->GetConvention() == ConventionTag::MUT);

      // Immutable symbols cannot be mutably borrowed. This also catches "&mut self" method calls.
      RaiseIf<SppInvalidMutationError>(
        not is_sym_mutable,
        ERR_ARGS(
          *sym->Name, *arg->Conv, *memory_state::StateOf(*sm->CurrentScope, *sym).AstInitialization,
          "immutable symbol"));

      // Immutable borrows, even if their symbol is mutable, cannot be mutably borrowed. An argument that is itself a
      // "&" borrow, a symbol's or not, was rejected above.
      RaiseIf<SppInvalidMutationError>(
        sym->MemInfo->AstBorrowed and *sym->Type->GetConvention() == ConventionTag::REF,
        ERR_ARGS(*sym->Name, *arg->Conv, *sym->MemInfo->AstBorrowed, "immutable borrow"));

      // A mutable borrow cannot overlap any other borrow.
      const auto borrowed = regions::PlaceAtOf(*arg->Val, *sm->CurrentScope);
      borrows::CheckAgainstLoans(borrowed, true, loans, call_loans, *arg->Val, *sm);

      // A coroutine or future holds the borrow past the call; the call holds it while it runs.
      raise_if_returned(*arg);
      if (handle_sym != nullptr and pins_required) {
        loans.emplace_back(memory_state::MakeLoan(borrowed, true, handle_sym, false, handle_pending));
      }
      call_loans.emplace_back(memory_state::MakeLoan(borrowed, true));
    }
  }
}

auto FunctionCallArgumentGroupAst::At(
  const char *key) const -> FunctionCallArgumentAst const* {
  // Iterate the comptime arguments to find the matching key.
  for (const auto *arg : GetKeywordArgs()) {
    if (arg->Name->Val == key) { return arg; }
  }
  return nullptr;
}

auto FunctionCallArgumentGroupAst::IsAllowedInDefault() const -> bool {
  // Check every argument - one bad one prevents the entire
  // group from being allowed in this specific context.
  for (auto const &x : Args) {
    if (not x->IsAllowedInDefault()) { return false; }
  }
  return true;
}

SPP_MOD_END
