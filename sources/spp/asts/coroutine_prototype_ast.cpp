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

  // Unlike the subroutine case, nothing below this point can run without a real function to emit into. The entry
  // block would have no parent, and "IRBuilder::CreateIntrinsic" reaches the module through the block's parent
  // function, so emitting the coroutine boot intrinsics would dereference null inside llvm's intrinsic lookup.
  // Skip the body scopes (as the null-target branch further down used to) and leave.
  if (llvm_func_target == nullptr) {
    const auto final_scope = sm->CurrentScope->FinalChildScope();
    while (sm->CurrentScope != final_scope) { sm->MoveToNextScope(false); }
    return nullptr;
  }

  const auto uid = "." + Uid();
  const auto entry_bb = llvm::BasicBlock::Create(
    *ctx->Context, "entry", llvm_func_target);
  ctx->Builder.SetInsertPoint(entry_bb);

  // Firstly, emit the llvm coroutine intrinsics for the
  // coroutine id, size and begin. These form the "boot"
  // instructions for the coroutine.
  const auto llvm_u8_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto llvm_coro_align = llvm::ConstantInt::get(llvm_u8_ty, alignof(std::max_align_t));
  const auto coro_id = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_id, {}, {llvm_coro_align, nullptr, nullptr, nullptr}, {}, "coro.id" + uid);
  const auto coro_size = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_size, {}, {}, {}, "coro.size" + uid);
  const auto coro_mem = ctx->Builder.CreateAlloca(
    llvm::Type::getInt8Ty(*ctx->Context), coro_size, "coro.mem" + uid);
  const auto coro_handle = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_begin, {}, {coro_id, coro_mem}, {}, "coro.begin" + uid);

  // Generate the function's parameters and generic
  // parameters into the coroutine. This will add the
  // param alloca instructions into the coroutine.
  FnParamGroup->Stage11_CodeGen(sm, meta, ctx);
  GnParamGroup->Stage11_CodeGen(sm, meta, ctx);

  // Add the generator environment object that contains
  // the yield and send slot, allowing the "gen" and
  // "res" operators to interact with it (load/store/GEP).
  const auto llvm_gen_state_ty = codegen::CreateLlvmGeneratorStateType(ctx);
  const auto llvm_gen_state = ctx->Builder.CreateAlloca(
    llvm_gen_state_ty, nullptr, "coro.gen.state" + uid);

  // Load the return type type symbol and the other
  // meta information values that the children asts
  // in the coroutine body might need to use.
  const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(
    ReturnType.get());
  meta->Save();
  meta->LlvmGenerator = MakeUnique<codegen::LlvmGenerator>(coro_handle);
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

  // Add the exit intrinsics to the coroutine.
  ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_end, {}, {coro_handle, ctx->Builder.getFalse()}, {}, "coro.end" + uid);
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
