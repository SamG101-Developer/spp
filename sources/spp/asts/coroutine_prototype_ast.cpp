module;
#include <spp/macros.hpp>

module spp.asts.coroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.identifier_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
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
  decltype(Name) &&name,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(FnParamGroup) &&param_group,
  decltype(TokArrow) &&tok_arrow,
  decltype(ReturnType) &&return_type,
  decltype(Impl) &&impl) :
  FunctionPrototypeAst(
    std::move(annotations), std::move(tok_cmp), std::move(tok_fun), std::move(name),
    std::move(generic_param_group), std::move(param_group), std::move(tok_arrow),
    std::move(return_type), std::move(impl)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_COR, "cor");
}

spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;

auto spp::asts::CoroutinePrototypeAst::Clone() const
  -> Unique<Ast> {
  auto ast = MakeUnique<CoroutinePrototypeAst>( // Todo: why no "cmp"?
    AstCloneVec(Annotations),
    nullptr,
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
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::CoroutinePrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::type_utils::IsTypeGen;
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
  GetGenAndYieldTypes(
    *ret_type_sym->FqName(), *sm->CurrentScope, *Source.OriginalReturnType, "coroutine return type");

  // Analyse the semantics of the function body, and move out the scope.
  sm->MoveOutOfCurrentScope();
  meta->Restore(true);
  meta->LoopReturnTypes->clear();
}

auto spp::asts::CoroutinePrototypeAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
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
  ctx->Builder.CreateRet(coro_handle);

  meta->Restore();
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto spp::asts::CoroutinePrototypeAst::IsCoroutine() const
  -> bool {
  return true;
}

SPP_MOD_END
