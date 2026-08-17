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
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

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
  meta->Save();
  meta->EnclosingFunctionFlavour = TokFun.get();
  meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
  meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
  meta->EnclosingFunctionScope = sm->CurrentScope;
  Impl->Stage7_AnalyseSemantics(sm, meta);

  // Check the return type superimposes the generator type.
  auto [_, yield_type, is_once] = GetGenAndYieldTypes(
    *ret_type_sym->FqName(), *sm->CurrentScope,
    *Source.OriginalReturnType, "coroutine return type");
  _YieldType = yield_type;
  _IsOnce = is_once;

  // Analyse the semantics of the function body, and move out the scope.
  sm->MoveOutOfCurrentScope();
  meta->Restore(true);
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
  const auto llvm_i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto llvm_ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto llvm_coro_align = llvm::ConstantInt::get(llvm_i32_ty, alignof(std::max_align_t));

  // The generator environment holding the yield and send
  // slots, which "gen" and "res" load/store/GEP through.
  const auto llvm_gen_state_ty = codegen::CreateLlvmGeneratorStateType(ctx);
  const auto llvm_gen_state = ctx->Builder.CreateAlloca(
    llvm_gen_state_ty, nullptr, "coro.gen.state" + uid);
  llvm_gen_state->setAlignment(llvm::Align(alignof(std::max_align_t)));

  // "llvm.coro.id" is "[token] (i32, ptr, ptr, ptr)". The
  // trailing two pointer operands (coroaddr, fnaddrs) are
  // unused here, but they are still operands: they have to
  // be null pointer *constants*.
  const auto llvm_null_ptr = llvm::ConstantPointerNull::get(llvm_ptr_ty);
  const auto coro_id = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_id, {}, {llvm_coro_align, llvm_gen_state, llvm_null_ptr, llvm_null_ptr}, {},
    "coro.id" + uid);

  // Guard the frame allocation with "llvm.coro.alloc".
  const auto coro_need_alloc = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_alloc, {}, {coro_id}, {}, "coro.need.alloc" + uid);

  const auto alloc_trap_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.alloc.trap" + uid, llvm_func_target);
  const auto begin_bb = llvm::BasicBlock::Create(*ctx->Context, "coro.begin.block" + uid, llvm_func_target);
  ctx->Builder.CreateCondBr(coro_need_alloc, alloc_trap_bb, begin_bb);

  // Elision declined, so the frame would have to be heap allocated - which this language does not do for coroutines.
  // Reaching here means a generator outlived the frame that owns it, which the analyser is meant to have rejected, so
  // trap rather than quietly allocating.
  ctx->Builder.SetInsertPoint(alloc_trap_bb);
  ctx->Builder.CreateIntrinsic(llvm::Intrinsic::trap, {}, {}, {}, "");
  ctx->Builder.CreateUnreachable();

  // The frame is provided from outside, so "llvm.coro.begin" is handed a null pointer: there is exactly one live
  // predecessor here (the trap block does not fall through), so no phi is needed to merge an allocated one in.
  ctx->Builder.SetInsertPoint(begin_bb);
  const auto coro_handle = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_begin, {}, {coro_id, llvm_null_ptr}, {}, "coro.begin" + uid);

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

  meta->Save();
  meta->LlvmGenerator = MakeShared<codegen::LlvmGenerator>(coro_handle);
  meta->LlvmGenerator->CleanupBlock = cleanup_bb;
  meta->LlvmGenerator->SuspendBlock = suspend_bb;
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

  // Running off the end of the body is the same as being destroyed, so fall through into the cleanup edge (unless
  // the body already terminated its block, eg with a return).
  if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
    ctx->Builder.CreateBr(cleanup_bb);
  }

  // Cleanup: the destroy edge of every suspend switch. There is no matching "llvm.coro.free" because nothing was ever
  // allocated - the frame belongs to the caller, and is released with the caller's own frame. The block exists to give
  // the destroy edge somewhere to go before the final suspend.
  cleanup_bb->insertInto(llvm_func_target);
  ctx->Builder.SetInsertPoint(cleanup_bb);
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

  meta->Restore();
  sm->MoveOutOfCurrentScope();
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

SPP_MOD_END
