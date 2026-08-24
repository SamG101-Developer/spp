module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
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
import spp.codegen.llvm_coros;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_materialize;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
  decltype(TokDot) &&tok_dot,
  decltype(TokRes) &&tok_res,
  decltype(FnArgGroup) &&arg_group) :
  TokDot(std::move(tok_dot)),
  TokRes(std::move(tok_res)),
  FnArgGroup(std::move(arg_group)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

spp::asts::PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosStart() const
  -> std::size_t {
  // Use the "." token.
  return TokDot != nullptr ? TokDot->PosStart() : 0;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosEnd() const
  -> std::size_t {
  // Use the argument group if it exists, otherwise use the "res" token.
  return FnArgGroup != nullptr ? FnArgGroup->PosEnd() : TokRes != nullptr ? TokRes->PosEnd() : 0;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorKeywordResAst>(
    AstClone(TokDot),
    AstClone(TokRes),
    AstClone(FnArgGroup));
  ast->_MappedFunc = _MappedFunc;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW(".");
  SPP_STRING_APPEND_RAW("res");
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Already analysed => return early.
  using analyse::utils::type_utils::GetGenAndYieldTypes;
  if (_MappedFunc != nullptr) { return; }

  // Check the left-hand-side is a generator type (for specific errors).
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  GetGenAndYieldTypes(
    *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");

  // Check the argument (send value) is valid, by passing it into the ".send" function call.
  auto send = MakeUnique<IdentifierAst>(PosStart(), "send");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(send));
  auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(FnArgGroup), nullptr);
  func_call->Source.OriginalExpr = this;
  _MappedFunc = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  meta->Save();
  meta->IgnoreAccessModifierViolations = true; // Because of "Generated" Todo: Too broad?
  _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
  meta->Restore();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Forward the memory check to the mapped function, which will check the arguments, and the function call.
  _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // The three-step operation for the "res" operation is to
  // store the potential argument into the send slot of the
  // env, resume the coroutine, then use the yielded value.

  // Step 0: Retrieve the correct generator environment from
  // the llvm context, keyed by the address of the generator's
  // storage. The left-hand-side is not necessarily a bare
  // identifier - it can be a field, an element, or a temporary -
  // so it is resolved to an address rather than to a symbol.
  const auto llvm_generator_addr = codegen::llvm_addr_of(*meta->PostfixExpressionLhs, sm, meta, ctx);
  const auto llvm_generator_it = ctx->LlvmGenerators.find(llvm_generator_addr);

  // A generator that was produced by a coroutine call in this function was registered when that call was generated.
  // One that arrived as a value was not: "loop item in self", inside a coroutine taking another generator as "self",
  // resumes something this function never called. Its handle is not lost though - it is the first field of the
  // generator value itself, which is exactly what the call site extracts before registering - so rebuild the
  // environment from the value in storage, the same way and with the same field.
  auto rebuilt_generator = Unique<codegen::LlvmGenerator>(nullptr);
  if (llvm_generator_it == ctx->LlvmGenerators.end()) {
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta)->WithoutConvention();
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_type.get());

    const auto no_env_msg = Str(
      "No generator environment was registered for this resumption, and none could be rebuilt from the value. The "
      "resumed value is generator-typed but carries no coroutine handle, so there is nothing to resume");
    RaiseIf<analyse::errors::SppInternalCompilerError>(
      lhs_type_sym == nullptr or lhs_type_sym->LlvmInfo->LlvmType == nullptr,
      {sm->CurrentScope}, ERR_ARGS(*this, no_env_msg));

    const auto handle_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 0);
    const auto llvm_handle_ptr = ctx->Builder.CreateStructGEP(
      lhs_type_sym->LlvmInfo->LlvmType, llvm_generator_addr, handle_idx, "gen.handle.slot");
    const auto llvm_handle = ctx->Builder.CreateLoad(
      llvm::PointerType::get(*ctx->Context, 0), llvm_handle_ptr, "gen.handle");

    rebuilt_generator = MakeUnique<codegen::LlvmGenerator>();
    rebuilt_generator->Handle = llvm_handle;
    rebuilt_generator->State = codegen::GetLlvmGeneratorStateFromHandle(llvm_handle, ctx);
  }

  const auto &llvm_generator_env = rebuilt_generator != nullptr
    ? rebuilt_generator
    : llvm_generator_it->second;

  // Step 1: Place the value of the argument (if it exists),
  // into the "send" slot on the generator state struct. A bare
  // "res()" sends nothing, so there is simply no store to make:
  // there is no such thing as a void value to write into the
  // slot, and asking for one ("getNullValue" of a void type)
  // is itself invalid. The receiver is an argument of the
  // mapped ".send()" call too, and is not one of these.
  const auto &args_group = _MappedFunc->Op->ToUnchecked<PostfixExpressionOperatorFunctionCallAst>()->FnArgGroup;
  const auto send_arg = std::ranges::find_if(
    args_group->Args, [](auto const &x) { return x->GetSelfType() == nullptr; });

  if (send_arg != args_group->Args.end()) {
    const auto llvm_send_slot = codegen::GetLlvmGeneratorSlotPtr(
      llvm_generator_env->State, codegen::LlvmGeneratorStateStructFields::SEND_SLOT, "gen.send.slot", ctx);
    const auto llvm_send_value = (*send_arg)->Stage11_CodeGen(sm, meta, ctx);
    ctx->Builder.CreateStore(llvm_send_value, llvm_send_slot);
  }

  // The yielded value is read with the yield type's own layout, because that is what the "gen" expression stored
  // into the slot. Reading the slot's raw cell type instead would hand back eight bytes whatever the yield type is,
  // and storing those into a narrower binding writes past it.
  const auto uid = spp::utils::Uid(this);
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  auto [_, yield_type, is_once] = analyse::utils::type_utils::GetGenAndYieldTypes(
    *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");
  const auto llvm_yield_ty = codegen::GetLlvmTypeOf(*yield_type, *sm->CurrentScope, ctx);

  const auto read_yielded_val = [&] {
    const auto llvm_yield_slot = codegen::GetLlvmGeneratorSlotPtr(
      llvm_generator_env->State, codegen::LlvmGeneratorStateStructFields::YIELD_SLOT, "gen.yield.slot", ctx);
    return ctx->Builder.CreateLoad(llvm_yield_ty, llvm_yield_slot, "gen.yield.value");
  };

  // A "GenOnce" is guaranteed to yield exactly once before it completes, so there is no exhausted case to report and
  // nothing to resume past: the value is already in the slot, put there by the ramp running up to the first suspend.
  if (is_once) { return read_yielded_val(); }

  // Otherwise the result says whether the generator had a value at all, so completion has to be tested before it is
  // read. A coroutine parked on its final suspend has already run its body to the end and left nothing in the slot.
  const auto llvm_done = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_done, {}, {llvm_generator_env->Handle}, {}, "gen.done" + uid);

  const auto llvm_func_target = ctx->Builder.GetInsertBlock()->getParent();
  const auto yielded_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.yielded" + uid, llvm_func_target);
  const auto exhausted_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.exhausted" + uid, llvm_func_target);
  const auto joined_bb = llvm::BasicBlock::Create(*ctx->Context, "gen.joined" + uid, llvm_func_target);
  ctx->Builder.CreateCondBr(llvm_done, exhausted_bb, yielded_bb);

  // The result type is "Yield or None", so both edges tag their way into it.
  const auto res_type = InferType(sm, meta);
  const auto llvm_res_ty = codegen::GetLlvmTypeOf(*res_type, *sm->CurrentScope, ctx);
  const auto none_type = generate::common_types::None(PosStart());
  const auto yield_tag = codegen::GetVariantIndexOfMember(*res_type, *yield_type, *sm->CurrentScope);
  const auto none_tag = codegen::GetVariantIndexOfMember(*res_type, *none_type, *sm->CurrentScope);

  const auto bad_shape_msg = Str(
    "The result of a resumption is not the \"Yield or None\" variant it has to be, so there is no discriminant to "
    "tag the yielded value or the exhausted case into");
  RaiseIf<analyse::errors::SppInternalCompilerError>(
    llvm_res_ty == nullptr or not yield_tag.has_value() or not none_tag.has_value(),
    {sm->CurrentScope}, ERR_ARGS(*this, bad_shape_msg));

  // Read before resuming, not after. The ramp already ran the body up to its first suspend, so the value waiting in
  // the slot is this resumption's; resuming first would step over it and hand back the following one.
  ctx->Builder.SetInsertPoint(yielded_bb);
  const auto llvm_yielded_val = read_yielded_val();
  ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_resume, {}, {llvm_generator_env->Handle}, {}, "");
  const auto llvm_some = codegen::BuildVariant(llvm_yielded_val, llvm_res_ty, *yield_tag, "gen.some" + uid, ctx);
  const auto some_from_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(joined_bb);

  ctx->Builder.SetInsertPoint(exhausted_bb);
  const auto llvm_none = codegen::BuildVariant(nullptr, llvm_res_ty, *none_tag, "gen.none" + uid, ctx);
  const auto none_from_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(joined_bb);

  ctx->Builder.SetInsertPoint(joined_bb);
  const auto llvm_res = ctx->Builder.CreatePHI(llvm_res_ty, 2, "gen.res" + uid);
  llvm_res->addIncoming(llvm_some, some_from_bb);
  llvm_res->addIncoming(llvm_none, none_from_bb);
  return llvm_res;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // The mapped ".send()" call is what says how much a resumption tells the caller: "Gen" declares it as
  // "Generated[Yield or None]", because a "Gen" may be exhausted, and "GenOnce" as "Generated[Yield]", because it
  // cannot be. Reading it off the declaration keeps the two in step instead of deciding it a second time here.
  // "Generated" is the compiler-known wrapper the coroutine machinery travels in, and is unwrapped on the way out.
  const auto send_type = _MappedFunc->InferType(sm, meta);
  return send_type->LastTypePart()->GnArgGroup->TypeAt("Yield")->Val;
}

SPP_MOD_END
