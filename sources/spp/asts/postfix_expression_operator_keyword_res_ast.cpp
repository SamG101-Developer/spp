module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.convention_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.LlvmMaterialize;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
  decltype(TokDot) &&tok_dot,
  decltype(TokRes) &&tok_res,
  decltype(FnArgGroup) &&arg_group) :
  TokDot(std::move(tok_dot)),
  TokRes(std::move(tok_res)),
  FnArgGroup(std::move(arg_group)),
  _MappedFn(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;

auto PostfixExpressionOperatorKeywordResAst::PosStart() const -> std::size_t {
  // Use the "." token.
  return TokDot != nullptr ? TokDot->PosStart() : 0;
}

auto PostfixExpressionOperatorKeywordResAst::PosEnd() const -> std::size_t {
  // Use the argument group if it exists, otherwise use the "res" token.
  return FnArgGroup != nullptr ? FnArgGroup->PosEnd() : TokRes != nullptr ? TokRes->PosEnd() : 0;
}

auto PostfixExpressionOperatorKeywordResAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorKeywordResAst>(
    AstClone(TokDot),
    AstClone(TokRes),
    AstClone(FnArgGroup));
  ast->_MappedFn = _MappedFn;
  return ast;
}

auto PostfixExpressionOperatorKeywordResAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW(".");
  SPP_STRING_APPEND_RAW("res");
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_END;
}

auto PostfixExpressionOperatorKeywordResAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Already analysed => return early.
  IMPORT_UTILS;
  if (_MappedFn != nullptr) { return; }

  // Check the left-hand-side is a generator type (for specific errors).
  const auto lhs = meta->PostfixExpressionLhs;
  const auto gen = marker_sups::FindGenSup(
    lhs->InferTypeRef(sm, meta), *sm->CurrentScope, *lhs, [&] { return lhs->InferType(sm, meta); },
    "resume expression");
  marker_sups::EnforceYieldTypeWithoutGenDone(gen, *sm->CurrentScope, *lhs, "resume expression");

  // Check the argument (send value) is valid, by passing it into the ".send" function call.
  auto send = MakeUnique<IdentifierAst>(PosStart(), "send");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(send));
  auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(FnArgGroup), nullptr);
  func_call->Source.OriginalExpr = this;
  _MappedFn = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  const auto _meta_guard = MetaGuard(meta);
  meta->IgnoreAccessModifierViolations = true; // Because of "Generated" Todo: Too broad?
  _MappedFn->Stage7_AnalyseSemantics(sm, meta);
}

namespace {
  /// Whether a value of this type is a "&mut" borrow, directly
  /// or as a member of a variant.
  auto HoldsMutBorrow(TypeAst const &type, Scope const &scope) -> bool {
    IMPORT_UTILS;
    const auto ref = TypeRef::Of(type, scope);
    if (ref.Conv == ConventionTag::MUT) { return true; }
    return genex::any_of(type_compare::VariantMemberRefs(ref, scope), [](auto const &member) {
      return member.Conv == ConventionTag::MUT;
    });
  }
}

auto PostfixExpressionOperatorKeywordResAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // The generator, when it is a named one. A borrow yielded by an
  // unnamed one cannot be resumed past, as nothing names it again.
  auto const *const lhs_name = meta->PostfixExpressionLhs->To<IdentifierAst>();
  auto *const gen_sym = lhs_name != nullptr ? sm->CurrentScope->FindVarSymbolOutermost(*lhs_name).first : nullptr;

  // Resuming the generator ends every "&mut" borrow it yielded
  // before: the next yield may be the same element, and two
  // live "&mut" borrows of one element would alias. Using one
  // afterwards reads as using memory moved by this resume.
  if (gen_sym != nullptr) {
    for (auto const *holder : gen_sym->MemInfo->AstYieldedMutBorrowHolders) {
      auto const *const holder_name = holder->To<IdentifierAst>();
      if (holder_name == nullptr) { continue; }
      // A name rebound since ("let x = 5") no longer holds the
      // borrow, so only one still typed as a "&mut" (or as the
      // "&mut T or GenDone" a resume produces) is ended.
      auto *const holder_sym = sm->CurrentScope->FindVarSymbolOutermost(*holder_name).first;
      if (holder_sym != nullptr and holder_sym->Type != nullptr and HoldsMutBorrow(*holder_sym->Type, *sm->CurrentScope)) {
        holder_sym->MemInfo->MovedBy(*this, sm->CurrentScope);
      }
    }
    gen_sym->MemInfo->AstYieldedMutBorrowHolders.Clear();
  }

  // Forward the memory check to the mapped function, which will check the arguments, and the function call.
  _MappedFn->Stage8_CheckMemory(sm, meta);

  // A "&mut" borrow this resume yields, bound to a name, lasts
  // until the next resume.
  if (gen_sym != nullptr and meta->AssignmentTarget != nullptr) {
    const auto yield_ref = marker_sups::GenYieldOf(marker_sups::FindGenSup(
      meta->PostfixExpressionLhs->InferTypeRef(sm, meta), *sm->CurrentScope, *meta->PostfixExpressionLhs,
      [&] { return meta->PostfixExpressionLhs->InferType(sm, meta); }, "resume expression", false));
    if (yield_ref.Conv == ConventionTag::MUT) {
      gen_sym->MemInfo->AstYieldedMutBorrowHolders.EmplaceBack(meta->AssignmentTarget.get());
    }
  }
}

auto PostfixExpressionOperatorKeywordResAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;
  // The three-step operation for the "res" operation is to
  // store the potential argument into the send slot of the
  // env, resume the coroutine, then use the yielded value.

  // Step 0: Retrieve the correct generator environment from
  // the llvm context, keyed by the address of the generator's
  // storage. The left-hand-side is not necessarily a bare
  // identifier - it can be a field, an element, or a temporary -
  // so it is resolved to an address rather than to a symbol.
  const auto llvm_generator_addr = codegen::LlvmAddrOf(*meta->PostfixExpressionLhs, sm, meta, ctx);
  const auto llvm_generator_it = ctx->LlvmGenerators.find(llvm_generator_addr);

  // A generator that was produced by a coroutine call in this function was registered when that call was generated.
  // One that arrived as a value was not: "loop item in self", inside a coroutine taking another generator as "self",
  // resumes something this function never called. Its handle is not lost though - it is the first field of the
  // generator value itself, which is exactly what the call site extracts before registering - so rebuild the
  // environment from the value in storage, the same way and with the same field.
  //
  // The registered handle is the call's own SSA value, which only
  // dominates uses in the block it was made in: a generator that
  // was assigned in a "case" branch is resumed after the merge,
  // so anywhere else the handle is reloaded from storage instead.
  const auto registered_is_local = [&] {
    if (llvm_generator_it == ctx->LlvmGenerators.end()) { return false; }
    const auto inst = llvm::dyn_cast<llvm::Instruction>(llvm_generator_it->second->Handle);
    return inst == nullptr or inst->getParent() == ctx->Builder.GetInsertBlock();
  }();

  auto rebuilt_generator = Unique<codegen::LlvmGenerator>(nullptr);
  if (not registered_is_local) {
    const auto lhs_type_sym = meta->PostfixExpressionLhs->InferTypeRef(sm, meta).Symbol;

    const auto no_env_msg = Str(
      "No generator environment was registered for this resumption, and none could be rebuilt from the value. The "
      "resumed value is generator-typed but carries no coroutine handle, so there is nothing to resume");
    RaiseIf<SppInternalCompilerError>(
      lhs_type_sym == nullptr or lhs_type_sym->LlvmInfo->LlvmType == nullptr,
      {sm->CurrentScope}, ERR_ARGS(*this, no_env_msg));

    // A generator whose only field is the handle lowers to the
    // handle itself, a plain pointer, so there is no struct to
    // index into - the storage holds the handle directly. That
    // is the shape a "Gen" returned by an ordinary function has.
    const auto llvm_gen_type = lhs_type_sym->LlvmInfo->LlvmType;
    const auto llvm_handle_ptr = llvm_gen_type->isPointerTy()
      ? llvm_generator_addr
      : ctx->Builder.CreateStructGEP(
        llvm_gen_type, llvm_generator_addr, codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 0),
        "gen.handle.slot");
    const auto llvm_handle = ctx->Builder.CreateLoad(
      llvm::PointerType::get(*ctx->Context, 0), llvm_handle_ptr, "gen.handle");

    rebuilt_generator = MakeUnique<codegen::LlvmGenerator>();
    rebuilt_generator->Handle = llvm_handle;
    rebuilt_generator->State = codegen::GetLlvmGeneratorStateFromHandle(llvm_handle, ctx);
  }

  const auto &llvm_generator_env = rebuilt_generator != nullptr
    ? rebuilt_generator
    : llvm_generator_it->second;

  // The yielded value is read with the yield type's own layout, because that is what the "gen" expression stored
  // into the slot. Reading the slot's raw cell type instead would hand back eight bytes whatever the yield type is,
  // and storing those into a narrower binding writes past it.
  const auto uid = Uid();
  const auto lhs = meta->PostfixExpressionLhs;
  const auto gen = marker_sups::FindGenSup(
    lhs->InferTypeRef(sm, meta), *sm->CurrentScope, *lhs, [&] { return lhs->InferType(sm, meta); },
    "resume expression");
  const auto yield_ref = marker_sups::GenYieldOf(gen);
  const auto is_once = marker_sups::IsGenOnce(gen, *sm->CurrentScope);
  const auto llvm_yield_ty = codegen::GetLlvmTypeOf(yield_ref, ctx);

  const auto send_ref = is_once
    ? TypeRef::Of(*generate::common_types_precompiled::VOID, *sm->CurrentScope)
    : gen.Symbol->TypeArgRef("Send");
  const auto llvm_send_ty = codegen::GetLlvmTypeOf(send_ref, ctx);
  const auto llvm_gen_state_ty = codegen::CreateLlvmGeneratorStateType(llvm_yield_ty, llvm_send_ty, ctx);

  // Step 1: Place the value of the argument (if it exists),
  // into the "send" slot on the generator state struct. A bare
  // "res()" sends nothing, so there is simply no store to make:
  // there is no such thing as a void value to write into the
  // slot, and asking for one ("getNullValue" of a void type)
  // is itself invalid. The receiver is an argument of the
  // mapped ".send()" call too, and is not one of these.
  const auto &args_group = _MappedFn->Op->ToUnchecked<PostfixExpressionOperatorFunctionCallAst>()->FnArgGroup;
  const auto send_arg = genex::find_if(
    args_group->Args, [](auto const &x) { return x->GetSelfType() == nullptr; });

  if (send_arg != args_group->Args.end()) {
    const auto llvm_send_slot = codegen::GetLlvmGeneratorSlotPtr(
      llvm_generator_env->State, llvm_gen_state_ty, codegen::LlvmGeneratorStateStructFields::SEND_SLOT,
      "gen.send.slot", ctx);
    const auto llvm_send_value = (*send_arg)->Stage11_CodeGen(sm, meta, ctx);
    ctx->Builder.CreateStore(llvm_send_value, llvm_send_slot);
  }

  const auto read_yielded_val = [&] {
    const auto llvm_yield_slot = codegen::GetLlvmGeneratorSlotPtr(
      llvm_generator_env->State, llvm_gen_state_ty, codegen::LlvmGeneratorStateStructFields::YIELD_SLOT,
      "gen.yield.slot", ctx);
    return ctx->Builder.CreateLoad(llvm_yield_ty, llvm_yield_slot, "gen.yield.value");
  };

  // Generators are lazy: creating one runs nothing, and each resumption runs the body on to its next "gen", so the
  // value is read after resuming, never before. A "GenOnce" is guaranteed to yield exactly once, so it has no
  // finished case to report.
  const auto resume = [&] {
    ctx->Builder.CreateIntrinsic(llvm::Intrinsic::coro_resume, {}, {llvm_generator_env->Handle}, {}, "");
  };
  if (is_once) {
    resume();
    return read_yielded_val();
  }

  // A coroutine parked on its final suspend must not be resumed again, so completion is tested both before resuming
  // (finished last time) and after (finished this time); either way there is no value, and the answer is "GenDone".
  const auto llvm_func_target = ctx->Builder.GetInsertBlock()->getParent();
  const auto resume_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.resume" + uid, llvm_func_target);
  const auto yielded_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.yielded" + uid, llvm_func_target);
  const auto exhausted_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.exhausted" + uid, llvm_func_target);
  const auto joined_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.joined" + uid, llvm_func_target);
  const auto llvm_was_done = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_done, {}, {llvm_generator_env->Handle}, {}, "gen.was.done" + uid);
  ctx->Builder.CreateCondBr(llvm_was_done, exhausted_bb, resume_bb);

  ctx->Builder.SetInsertPoint(resume_bb);
  resume();
  const auto llvm_done = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_done, {}, {llvm_generator_env->Handle}, {}, "gen.done" + uid);
  ctx->Builder.CreateCondBr(llvm_done, exhausted_bb, yielded_bb);

  // The result type is "Yield or GenDone", so both edges
  // tag their way into it. A yield type that is itself a
  // variant ("Opt[Str]") is flattened into the result
  // rather than being one member of it, so it is re-tagged
  // member by member instead.
  const auto res_ref = InferTypeRef(sm, meta);
  const auto llvm_res_ty = codegen::GetLlvmTypeOf(res_ref, ctx);
  const auto done_type = generate::common_types::GenDone(PosStart());
  const auto yield_tag = codegen::GetVariantIndexOfMember(res_ref, yield_ref, *sm->CurrentScope);
  const auto yield_is_variant = type_predicates::IsTypeVariant(yield_ref, *sm->CurrentScope);
  const auto done_tag = codegen::GetVariantIndexOfMember(
    res_ref, TypeRef::Of(*done_type, *sm->CurrentScope), *sm->CurrentScope);

  const auto bad_shape_msg = Str(
    "The result of a resumption is not the \"Yield or GenDone\" variant it has to be, so there is no discriminant "
    "to tag the yielded value or the finished case into");
  RaiseIf<SppInternalCompilerError>(
    llvm_res_ty == nullptr or (not yield_tag.has_value() and not yield_is_variant) or not done_tag.has_value(),
    {sm->CurrentScope}, ERR_ARGS(*this, bad_shape_msg));

  // The resumption just made put this value in the slot.
  ctx->Builder.SetInsertPoint(yielded_bb);
  const auto llvm_yielded_val = read_yielded_val();
  const auto llvm_some = yield_tag.has_value()
    ? codegen::BuildVariant(llvm_yielded_val, llvm_res_ty, *yield_tag, "gen.some" + uid, ctx)
    : codegen::CoerceToVariant(llvm_yielded_val, res_ref, yield_ref, *sm->CurrentScope, "gen.some" + uid, ctx);
  const auto some_from_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(joined_bb);

  ctx->Builder.SetInsertPoint(exhausted_bb);
  const auto llvm_none = codegen::BuildVariant(nullptr, llvm_res_ty, *done_tag, "gen.done" + uid, ctx);
  const auto none_from_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(joined_bb);

  ctx->Builder.SetInsertPoint(joined_bb);
  const auto llvm_res = ctx->Builder.CreatePHI(llvm_res_ty, 2, "gen.res" + uid);
  llvm_res->addIncoming(llvm_some, some_from_bb);
  llvm_res->addIncoming(llvm_none, none_from_bb);
  return llvm_res;
}

auto PostfixExpressionOperatorKeywordResAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The "Yield" argument of what the mapped ".send()"
  // returns ("InferType").
  return _MappedFn->InferTypeRef(sm, meta).Symbol->TypeArgRef("Yield");
}

auto PostfixExpressionOperatorKeywordResAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst> {
  // The potential resume arguments are expressions.
  auto fn_arg_group = AstClone(FnArgGroup);
  for (auto const &fn_arg : fn_arg_group->Args) {
    fn_arg->Val = AstClone(fn_arg->Val->ReadExpr(sub));
  }

  return MakeUnique<PostfixExpressionOperatorKeywordResAst>(
    AstClone(TokDot), AstClone(TokRes), std::move(fn_arg_group));
}

auto PostfixExpressionOperatorKeywordResAst::IsAllowedInDefault() const -> bool {
  // Resumes a generator the way a call runs a function,
  // so nothing leaves the code it is in. Should be safe
  // although I can't see where this would even be used.
  return true;
}

SPP_MOD_END
