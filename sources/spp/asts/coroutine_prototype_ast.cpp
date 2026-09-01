module;
#include <spp/macros.hpp>
#include <spp/codegen/macros.hpp>

module spp.asts.coroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

namespace {
  /**
   * The runtime's allocator, as the module sees it. A coroutine frame is allocated and released by the coroutine
   * itself rather than through the s++ allocator types, because the size is not known until llvm has laid the frame
   * out - there is no s++ expression to hand a "USize" to at this point, only the "llvm.coro.size" intrinsic.
   * Todo: Move this to stack not heap allocation.
   * @param[in,out] ctx The context whose module the declaration belongs to.
   * @return The "sppc_malloc" declaration, taking a byte count and returning the storage.
   */
  auto CoroFrameAllocFn(
    spp::codegen::LlvmCtx *ctx)
    -> llvm::Function* {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto size_ty = llvm::Type::getInt64Ty(*ctx->Context);
    const auto fn_ty = llvm::FunctionType::get(ptr_ty, {size_ty}, false);
    return llvm::cast<llvm::Function>(ctx->Module->getOrInsertFunction("sppc_malloc", fn_ty).getCallee());
  }

  /** The release half of @c CoroFrameAllocFn ; takes the storage that "llvm.coro.free" handed back. */
  auto CoroFrameFreeFn(
    spp::codegen::LlvmCtx *ctx)
    -> llvm::Function* {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
    const auto fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
    return llvm::cast<llvm::Function>(ctx->Module->getOrInsertFunction("sppc_free", fn_ty).getCallee());
  }
}

SPP_MOD_BEGIN
spp::asts::CoroutinePrototypeAst::CoroutinePrototypeAst(
  decltype(Annotations) &&annotations,
  decltype(TokCmp) &&tok_cmp,
  decltype(TokFun) &&tok_fun,
  decltype(Name) name,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(FnParamGroup) &&param_group,
  decltype(TokArrow) &&tok_arrow,
  decltype(ReturnType) return_type,
  decltype(Impl) &&impl) :
  FunctionPrototypeAst(
    std::move(annotations), std::move(tok_cmp), std::move(tok_fun), std::move(name),
    std::move(generic_param_group), std::move(param_group), std::move(tok_arrow),
    std::move(return_type), std::move(impl)),
  _IsOnce(false),
  _YieldType(nullptr),
  _SendType(nullptr),
  _GenOnceLowered(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_COR, "cor");
}

spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;

auto spp::asts::CoroutinePrototypeAst::Clone() const
  -> Unique<Ast> {
  auto ast = MakeUnique<CoroutinePrototypeAst>(
    AstCloneVec(Annotations),
    nullptr, // "cmp cor" not syntactically allowed. Todo: Raise semantic error instead?
    AstClone(TokFun),
    AstClone(Name),
    AstClone(GnParamGroup),
    AstClone(FnParamGroup),
    AstClone(TokArrow),
    AstClone(ReturnType),
    AstClone(Impl));
  ast->_AnnotationInfo = _AnnotationInfo
    ? MakeUnique<analyse::utils::annotation_utils::AnnotationInfo>(*_AnnotationInfo)
    : nullptr;
  ast->Source.OriginalImpl = AstClone(Source.OriginalImpl);
  ast->Source.OriginalReturnType = AstClone(Source.OriginalReturnType);
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->AbstractAnnotation = AbstractAnnotation;
  ast->VirtualAnnotation = VirtualAnnotation;
  ast->TemperatureAnnotation = TemperatureAnnotation;
  ast->FfiAnnotation = FfiAnnotation;
  ast->BuiltinAnnotation = BuiltinAnnotation;
  ast->TestAnnotation = TestAnnotation;
  ast->InlineAnnotation = InlineAnnotation;
  ast->Visibility = Visibility;
  ast->_LlvmFunc = _LlvmFunc;
  ast->VariadicPackType = VariadicPackType;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::CoroutinePrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::type_utils::GetGenAndYieldTypes;

  // Perform default function prototype semantic analysis
  FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
  const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType.get());

  // Update the meta information for enclosing function information.
  {
    const auto _meta_guard = meta::MetaGuard(meta, true);
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;
    Impl->Stage7_AnalyseSemantics(sm, meta);

    // Check the return type superimposes the generator type.
    auto [generator_type, yield_type, is_once] = GetGenAndYieldTypes(
      *ret_type_sym->FqName(), *sm->CurrentScope,
      *Source.OriginalReturnType, "coroutine return type");
    _YieldType = yield_type;
    _SendType = is_once
      ? generate::common_types_precompiled::VOID
      : generator_type->LastTypePart()->GnArgGroup->TypeAt("Send")->Val;
    _IsOnce = is_once;

    // Analyse the semantics of the function body, and move out the scope.
    sm->MoveOutOfCurrentScope();
  }
  meta->LoopReturnTypes->clear();
}

auto spp::asts::CoroutinePrototypeAst::Stage10_PreCodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // For "GenOnce" coroutines, we can desugar them into
  // subroutines returning the yielded value. This is memory
  // safe as we have finished stage 8 already.
  _LowerGenOnce();
  if (_GenOnceLowered == nullptr) {
    return FunctionPrototypeAst::Stage10_PreCodeGen(sm, meta, ctx);
  }

  // The lowering is what a call now targets, so it is the lowering
  // that is declared. The stamp still belongs on this prototype,
  // because the lowering reaches it through "SetNonGenericImpl",
  // and because a non-lowered reader (an instantiation of this
  // template, below) asks this one for it.
  _OwnerCtx = ctx;
  _GenOnceLowered->Stage10_PreCodeGen(sm, meta, ctx);
  _ForceInlineBorrowedYield(*_GenOnceLowered);

  // An instantiation is a clone of this prototype holding its own
  // analysed body, so it needs a lowering, and a declaration, of
  // its own. This is the only walk that reaches one: instantiations
  // are registered against their template, never visited as
  // prototypes in their own right.
  for (auto const &sub : _GenericSubstitutions) {
    if (sub.Proto == nullptr or not sub.IsConcrete) { continue; }
    auto sub_target = sub.Proto.get();
    if (const auto sub_coro = sub.Proto->To<CoroutinePrototypeAst>(); sub_coro != nullptr) {
      sub_coro->_OwnerCtx = ctx;
      sub_coro->_LowerGenOnce();
      if (sub_coro->_GenOnceLowered != nullptr) { sub_target = sub_coro->_GenOnceLowered.get(); }
    }

    auto tm = ScopeManager(sm->GlobalScope, sub.WalkScope());
    sub_target->GenerateLlvmDeclaration(&tm, meta, ctx);
    if (const auto sub_coro = sub.Proto->To<CoroutinePrototypeAst>();
      sub_coro != nullptr and sub_coro->_GenOnceLowered != nullptr) {
      sub_coro->_ForceInlineBorrowedYield(*sub_coro->_GenOnceLowered);
    }
  }

  return nullptr;
}

auto spp::asts::CoroutinePrototypeAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // The lowering emits this prototype's body, but it is this
  // prototype the instantiations are registered against, so their
  // bodies (each emitted through its own lowering) are still driven
  // from here.
  if (_GenOnceLowered != nullptr) {
    _GenOnceLowered->Stage11_CodeGen(sm, meta, ctx);
    _CodeGenGenericSubstitutions(sm, meta, ctx);
    return nullptr;
  }

  //
  using spp::utils::Uid;
  sm->MoveToNextScope();

  // Create the entry block for this function. The first
  // instructions contained in this function will be the
  // coroutine boot intrinsics.
  const auto llvm_func = GetLlvmFunc();
  const auto llvm_func_target = llvm_func != nullptr ? llvm_func->Target : nullptr;

  if (llvm_func_target == nullptr) {
    const auto final_scope = sm->CurrentScope->FinalChildScope();
    while (sm->CurrentScope != final_scope) { sm->MoveToNextScope(false); }
    sm->MoveOutOfCurrentScope();
    _CodeGenGenericSubstitutions(sm, meta, ctx);
    return nullptr;
  }
  llvm_func_target->setPresplitCoroutine();

  const auto uid = "." + Uid();
  const auto entry_bb = llvm::BasicBlock::Create(
    *ctx->Context, "entry", llvm_func_target);
  ctx->Builder.SetInsertPoint(entry_bb);

  // Firstly, emit the llvm coroutine intrinsics for the
  // coroutine id, size and begin. These form the "boot"
  // instructions for the coroutine.
  const auto llvm_i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto llvm_ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto llvm_coro_align = codegen::GetLlvmGeneratorFrameAlign(ctx);

  // The generator environment holding the yield and send
  // slots, which "gen" and "res" load/store/GEP through.
  const auto llvm_yield_ty = codegen::GetLlvmTypeOf(*_YieldType, *sm->CurrentScope, ctx);
  const auto llvm_send_ty = codegen::GetLlvmTypeOf(*_SendType, *sm->CurrentScope, ctx);
  const auto llvm_gen_state_ty = codegen::CreateLlvmGeneratorStateType(llvm_yield_ty, llvm_send_ty, ctx);
  const auto llvm_gen_state = ctx->Builder.CreateAlloca(
    llvm_gen_state_ty, nullptr, "coro.gen.state" + uid);
  // The same alignment "GetLlvmGeneratorFrameAlign" reports, because "llvm.coro.promise" reads the promise back out
  // of the frame on that assumption.
  llvm_gen_state->setAlignment(llvm::Align(alignof(std::max_align_t)));

  // "llvm.coro.id" is "[token] (i32, ptr, ptr, ptr)". The third operand is the coroutine's own address, which is what
  // identifies this coroutine to the elision pass: given it, "CoroElide" can recognise a frame whose lifetime is
  // contained in its caller and place it in the caller's stack frame instead of allocating one. Passing null there
  // leaves every frame on the heap. The fourth (fnaddrs) is filled in by "CoroSplit" once the resume and destroy
  // functions exist, so it stays a null constant here.
  const auto llvm_null_ptr = llvm::ConstantPointerNull::get(llvm_ptr_ty);
  const auto coro_id = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_id, {}, {llvm_coro_align, llvm_gen_state, llvm_func_target, llvm_null_ptr}, {},
    "coro.id" + uid);

  // Guard the frame allocation with "llvm.coro.alloc".
  const auto coro_need_alloc = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_alloc, {}, {coro_id}, {}, "coro.need.alloc" + uid);

  const auto dyn_alloc_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.dyn.alloc" + uid, llvm_func_target);
  const auto begin_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.begin.block" + uid, llvm_func_target);
  ctx->Builder.CreateCondBr(coro_need_alloc, dyn_alloc_bb, begin_bb);

  // The size is only known once the frame has been laid out,
  // which is why it is an intrinsic and not a constant here.
  ctx->Builder.SetInsertPoint(dyn_alloc_bb);
  const auto coro_size = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_size, {llvm_i64_ty}, {}, {}, "coro.size" + uid);
  const auto coro_alloc_mem = ctx->Builder.CreateCall(
    CoroFrameAllocFn(ctx), {coro_size}, "coro.alloc.mem" + uid);
  ctx->Builder.CreateBr(begin_bb);

  // Null on the non-allocating edge: that is what tells llvm the
  // frame was provided rather than allocated, and is the value
  // that survives when the frame is elided into the caller.
  ctx->Builder.SetInsertPoint(begin_bb);
  const auto coro_mem = ctx->Builder.CreatePHI(llvm_ptr_ty, 2, "coro.frame.mem" + uid);
  coro_mem->addIncoming(llvm_null_ptr, entry_bb);
  coro_mem->addIncoming(coro_alloc_mem, dyn_alloc_bb);
  const auto coro_handle = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_begin, {}, {coro_id, coro_mem}, {}, "coro.begin" + uid);

  // Generate the function's parameters and generic parameters
  // into the coroutine. This will add the param alloca instructions
  // into the coroutine.
  FnParamGroup->Stage11_CodeGen(sm, meta, ctx);
  GnParamGroup->Stage11_CodeGen(sm, meta, ctx);

  // Load the return type type symbol and the other
  // meta information values that the children asts
  // in the coroutine body might need to use.
  const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(
    ReturnType.get());

  // Create the two blocks that every suspend point branches to.
  // They are made up-front (detached, and inserted by the epilogue
  // below) because a "gen" expression in the body needs them as
  // targets of its suspend switch long before this function gets
  // to emit them.
  const auto cleanup_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.cleanup" + uid);
  const auto suspend_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.suspend" + uid);
  const auto final_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.final" + uid);

  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->LlvmGenerator = MakeShared<codegen::LlvmGenerator>(coro_handle);
    meta->LlvmGenerator->CleanupBlock = cleanup_bb;
    meta->LlvmGenerator->SuspendBlock = suspend_bb;
    meta->LlvmGenerator->FinalBlock = final_bb;
    meta->LlvmGeneratorState = llvm_gen_state;
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;

    // If there is an implementation, generate its code. Generic
    // bases have already returned above. There are no ffi
    // coroutines.
    const auto is_extern = AbstractAnnotation;
    if (not is_extern) {
      // Generate the coroutine implementation. Add a safety
      // return void at the end.
      Impl->Stage11_CodeGen(sm, meta, ctx);
    }

    // Running off the end of the body is the coroutine completing,
    // and completing is a suspend like any other - marked final.
    // It has to be a suspend rather than a fall-through into cleanup,
    // because "llvm.coro.done" is what tells a consumer there is
    // nothing left to resume, and it only ever reads true of a
    // coroutine parked on a final suspend. Freeing the frame here
    // instead would leave the consumer asking a destroyed frame
    // whether it was finished. "ret" must set the current block as
    // as the final block.
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      ctx->Builder.CreateBr(final_bb);
    }

    // A body with no way out - "loop true" over an unending "gen" -
    // never completes, so there is nothing to mark finished and
    // the block is dropped rather than left orphaned. A final
    // suspend sitting in unreachable code would still be collected
    // as this coroutine's, which is not something to hand the
    // coroutine passes. Resuming a coroutine that has already
    // finished is undefined behaviour rather than something to
    // lower, so the block the suspend leaves the builder in -
    // the one a resume would return to - is unreachable.
    if (final_bb->hasNPredecessorsOrMore(1)) {
      final_bb->insertInto(llvm_func_target);
      ctx->Builder.SetInsertPoint(final_bb);
      codegen::EmitLlvmGeneratorSuspend(
        true, suspend_bb, cleanup_bb, "coro.final.suspend" + uid, "coro.final.resume" + uid, ctx);
      ctx->Builder.CreateUnreachable();
    }
    else {
      delete final_bb;
    }

    // Cleanup: the destroy edge of every suspend switch, and where the frame is released. "llvm.coro.free" hands back
    // the pointer that "llvm.coro.begin" was given, or null when the frame was never allocated - elided into the
    // caller, in which case it goes away with the caller's own frame and there is nothing to free here. That is why
    // the free is guarded rather than unconditional.
    cleanup_bb->insertInto(llvm_func_target);
    ctx->Builder.SetInsertPoint(cleanup_bb);
    const auto coro_free_mem = ctx->Builder.CreateIntrinsic(
      llvm::Intrinsic::coro_free, {}, {coro_id, coro_handle}, {}, "coro.free.mem" + uid);
    const auto coro_was_alloced = ctx->Builder.CreateIsNotNull(coro_free_mem, "coro.was.alloced" + uid);

    const auto free_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.free" + uid, llvm_func_target);
    ctx->Builder.CreateCondBr(coro_was_alloced, free_bb, suspend_bb);

    ctx->Builder.SetInsertPoint(free_bb);
    ctx->Builder.CreateCall(CoroFrameFreeFn(ctx), {coro_free_mem});
    ctx->Builder.CreateBr(suspend_bb);

    // Final suspend: end the coroutine and return the handle. Like the cleanup block, this was created detached so a
    // "gen" in the body could name it as a suspend-switch target before it existed here, so it has to be attached
    // before anything is built into it - a parentless block has no module, and "CreateIntrinsic" needs one.
    suspend_bb->insertInto(llvm_func_target);
    ctx->Builder.SetInsertPoint(suspend_bb);
    ctx->Builder.CreateIntrinsic(
      llvm::Intrinsic::coro_end, {},
      {coro_handle, ctx->Builder.getFalse(), llvm::ConstantTokenNone::get(*ctx->Context)}, {}, "");

    // "Gen"/"GenOnce" lower to the bare handle, so the coroutine hands it straight back. A class superimposing one of
    // them ("Iterator[T]") is a struct instead, carrying the handle in the fat-pointer field ahead of whatever it
    // declares of its own, so the handle is packed into that shape before it leaves the function - the caller unwraps
    // it again to drive the coroutine intrinsics.
    const auto llvm_ret_type = llvm_func_target->getReturnType();
    if (llvm_ret_type->isPointerTy()) {
      ctx->Builder.CreateRet(coro_handle);
    }
    else {
      const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType.get());
      const auto handle_idx = codegen::GetPhysicalFieldIndex(*ret_type_sym->LlvmInfo, 0);
      const auto empty_ret_val = llvm::Constant::getNullValue(llvm_ret_type);
      ctx->Builder.CreateRet(
        ctx->Builder.CreateInsertValue(empty_ret_val, coro_handle, {handle_idx}, "coro.handle.wrap" + uid));
    }
    VALIDATE_LLVM;

  }
  sm->MoveOutOfCurrentScope();
  _CodeGenGenericSubstitutions(sm, meta, ctx);
  return nullptr;
}

auto spp::asts::CoroutinePrototypeAst::IsCoroutine() const
  -> bool {
  return true;
}

auto spp::asts::CoroutinePrototypeAst::IsOnce() const
  -> bool {
  return _IsOnce;
}

auto spp::asts::CoroutinePrototypeAst::GenOnceLowered() const
  -> SubroutinePrototypeAst* {
  return _GenOnceLowered.get();
}

auto spp::asts::CoroutinePrototypeAst::_LowerGenOnce()
  -> void {
  if (not _IsOnce or _GenOnceLowered != nullptr) { return; }

  // The signature is this coroutine's with the generator return
  // type replaced by what it yields; the body is taken over
  // wholesale, because nothing about it changes - a "gen" inside
  // it reads as a "ret" once the enclosing flavour says "fun".
  _GenOnceLowered = MakeUnique<SubroutinePrototypeAst>(
    SPP_NO_ANNOTATIONS, nullptr, nullptr, Name,
    AstClone(GnParamGroup), AstClone(FnParamGroup),
    nullptr, _YieldType, std::move(Impl));

  // The lowering is written in no module of its own, so point it
  // at the coroutine it came from: that is where Stage10 stamps
  // the owning context "OwnerCtx" reads back.
  _GenOnceLowered->SetNonGenericImpl(this);
}

auto spp::asts::CoroutinePrototypeAst::_ForceInlineBorrowedYield(
  SubroutinePrototypeAst const &lowered) const
  -> void {
  // A "GenOnce" that yields a borrow hands back the address of something the body built, and a body that yields a
  // view over its argument ("fwd_ref", "slice_ref") has nowhere to build it but its own frame. That was sound while
  // this was a coroutine, because the frame is elided into the caller's and outlives the yield; it is not sound once
  // the same body is an ordinary call, whose frame dies at the return.
  //
  // Forcing the inline gives the storage back to the caller, which is where the borrow's lifetime says it belongs.
  // It also restores the code that is meant to come out of one of these: the view is promoted out of memory
  // entirely, leaving the pointer it was built from, so "arr.eq" is the "memcmp" it reads as rather than a chain of
  // calls through a struct. Left out of line, the stores into the view are deleted as writes to a dying frame, and
  // what remains is an empty function returning a dangling pointer.
  if (_YieldType == nullptr or _YieldType->GetConvention() == nullptr) { return; }
  const auto llvm_func = lowered.GetLlvmFunc();
  if (llvm_func == nullptr or llvm_func->Target == nullptr) { return; }
  llvm_func->Target->addFnAttr(llvm::Attribute::AlwaysInline);
}

SPP_MOD_END
