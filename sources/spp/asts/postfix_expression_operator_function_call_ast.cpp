module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.monomorphization_utils;
import spp.analyse.utils.overload_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
  decltype(GnArgGroup) &&generic_arg_group,
  decltype(FnArgGroup) &&arg_group,
  decltype(Fold) &&fold) :
  GnArgGroup(std::move(generic_arg_group)),
  FnArgGroup(std::move(arg_group)),
  Fold(std::move(fold)),
  _OverloadInfo(std::nullopt),
  _ClosureDummyArg(nullptr),
  _ClosureDummyProto(nullptr),
  _IsAsync(nullptr),
  _IsCoroAndAutoResume(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
  Source.OriginalExpr = this;
}

spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::PosStart() const
  -> std::size_t {
  // Use the generic argument group.
  return not GnArgGroup->Args.IsEmpty() ? GnArgGroup->PosStart() : FnArgGroup->PosStart();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::PosEnd() const
  -> std::size_t {
  // Use the fold or function argument group.
  return Fold ? Fold->PosEnd() : FnArgGroup->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    AstClone(GnArgGroup),
    AstClone(FnArgGroup),
    AstClone(Fold));
  if (Source.OriginalExpr != this) {
    ast->Source.OriginalExpr = Source.OriginalExpr;
  }
  ast->_ClosureDummyProto = AstClone(_ClosureDummyProto);
  ast->_TransformedAst = AstClone(_TransformedAst);
  ast->_OverloadInfo = _OverloadInfo;
  if (ast->_OverloadInfo.has_value()
    and _ClosureDummyProto != nullptr
    and ast->_OverloadInfo->Proto == _ClosureDummyProto.get()) {
    ast->_OverloadInfo->Proto = ast->_ClosureDummyProto.get();
  }
  ast->_IsAsync = _IsAsync;
  ast->_FoldedAsts = AstCloneVec(_FoldedAsts);
  ast->_ClosureDummyArgGroup = AstClone(_ClosureDummyArgGroup);
  ast->_ClosureDummyArg = AstClone(_ClosureDummyArg);
  ast->_IsCoroAndAutoResume = _IsCoroAndAutoResume;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::ToString() const
  -> Str {
  SPP_STRING_START;
  if (_TransformedAst != nullptr) {
    SPP_STRING_APPEND(_TransformedAst);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND(GnArgGroup);
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_APPEND(Fold);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppInvalidComptimeOperationError;
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::utils::func_utils::IsTargetCallable;
  using analyse::utils::overload_utils::DetermineOverload;
  using analyse::utils::type_utils::IsTypeBorrowed;
  using analyse::utils::type_utils::TypeEq;
  using generate::common_types_precompiled::FUN_REF;
  using generate::common_types_precompiled::FUN_MUT;
  using generate::common_types_precompiled::GEN_ONCE;

  // Prevent double analysis.
  // Todo: See why this might be happening anyway, and remove this check preferably.
  if (_OverloadInfo.has_value()) { return; }

  // Analyse the generic arguments and the function call arguments before determining the overload.
  meta->Save();
  meta->ReturnTypeOverloadResolverType = nullptr;
  GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
  FnArgGroup->Stage7_AnalyseSemantics(sm, meta);
  meta->Restore();

  // If we are function folding, create transformed asts.
  if (Fold != nullptr) {
    _FoldedAsts = _HandleFunctionFolding(sm, meta);
    for (auto const &ast : _FoldedAsts) { ast->Stage7_AnalyseSemantics(sm, meta); }
    return;
  }

  // Resolve the overload for this function call.
  auto [overload, is_closure] = DetermineOverload(*this, sm, meta);

  // Special case for closures; apply the convention the closure name to ensure is it movable/mutable etc.
  if (is_closure) {
    const auto lhs_type = IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
    auto dummy_self_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
      nullptr, nullptr, AstClone(meta->PostfixExpressionLhs));

    if (TypeEq(*lhs_type->WithoutGenerics(), *FUN_MUT, *sm->CurrentScope, *sm->CurrentScope)) {
      dummy_self_arg->Conv = MakeUnique<ConventionMutAst>(nullptr, nullptr);
      dummy_self_arg->Conv->To<ConventionMutAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
    }
    else if (TypeEq(*lhs_type->WithoutGenerics(), *FUN_REF, *sm->CurrentScope, *sm->CurrentScope)) {
      dummy_self_arg->Conv = MakeUnique<ConventionRefAst>(nullptr);
      dummy_self_arg->Conv->To<ConventionRefAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
    }
    _ClosureDummyArg = std::move(dummy_self_arg);
  }

  // Set the overload to the only pass overload.
  _OverloadInfo = _OInfo{
    .OverloadScope = overload.FnScope,
    .Proto = overload.Proto
  };
  if (const auto self_param = _OverloadInfo->Proto->FnParamGroup->GetSelfParam()) {
    FnArgGroup->Args[0]->Conv = AstClone(self_param->Conv);
  }
  FnArgGroup->Args = std::move(overload.FnArgs->Args);

  // A unit test belongs to the harness, not to the program. Calling one would run it as part of whatever called it,
  // and there is no sensible meaning for that, so the call is rejected wherever it appears.
  if (const auto unit_test = _OverloadInfo->Proto->TestAnnotation;
    unit_test != nullptr and not meta->IsTestHarness) {
    Raise<analyse::errors::SppUnitTestNotCallableError>(
      {sm->CurrentScope}, ERR_ARGS(*this, *unit_test));
  }

  // Check that if we are in a cmp context, that the overload is also cmp.
  RaiseIf<SppInvalidComptimeOperationError>(
    meta->EnclosingFunctionCmp != nullptr and _OverloadInfo->Proto->TokCmp == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Special case for GenOnce called as a coroutine => auto move into the "Yield" type.
  if (_OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_COR and not meta->PreventAutoGeneratorResume) {
    // This needs to be any type that EXTENDS GenOnce, not just GenOnce itself.
    auto [_, _, is_once] = analyse::utils::type_utils::GetGenAndYieldTypes(
      *_OverloadInfo->Proto->ReturnType, *sm->CurrentScope, *meta->PostfixExpressionLhs, "GenOnce collapse");
    _IsCoroAndAutoResume = is_once;
  }
  meta->PreventAutoGeneratorResume = false;

  // Todo: Is this needed?
  const auto ret_type = InferType(sm, meta);
  RaiseIf<SppSecondClassBorrowViolationError>(
    _OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_FUN and IsTypeBorrowed(
      *ret_type->WithoutConvention(), *sm),
    {sm->CurrentScope}, ERR_ARGS(*this, *ret_type, "function return type"));

  // Copy some properties into the transform (clone arg group for the self arg convention).
  if (_TransformedAst) {
    const auto transformed_op = _TransformedAst->Op->To<PostfixExpressionOperatorFunctionCallAst>();
    transformed_op->FnArgGroup = AstClone(FnArgGroup);
    transformed_op->_OverloadInfo = _OverloadInfo;
    transformed_op->_IsAsync = _IsAsync;
    transformed_op->_IsCoroAndAutoResume = _IsCoroAndAutoResume;
    transformed_op->_FoldedAsts = AstCloneVec(_FoldedAsts);
    transformed_op->_ClosureDummyArg = AstClone(_ClosureDummyArg);
  }
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // If a fold is taking place, analyse the folded transformations.
  if (Fold != nullptr) {
    for (auto const &ast : _FoldedAsts) { ast->Stage8_CheckMemory(sm, meta); }
    return;
  }

  // If a closure is being called, apply memory rules to the symbolic target.
  if (_ClosureDummyArg != nullptr) {
    auto closure_args = Vec<Unique<FunctionCallArgumentAst>>();
    closure_args.EmplaceBack(std::move(_ClosureDummyArg));
    _ClosureDummyArgGroup = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(closure_args), nullptr);
    _ClosureDummyArgGroup->Stage7_AnalyseSemantics(sm, meta);
    _ClosureDummyArgGroup->Stage8_CheckMemory(sm, meta);
  }

  // Check the argument group, now the old borrows have been invalidated.
  GnArgGroup->Stage8_CheckMemory(sm, meta);

  meta->Save();
  meta->TargetCallFunctionPrototype = _OverloadInfo->Proto;
  meta->TargetCallWasFunctionAsync = _IsAsync;
  FnArgGroup->Stage8_CheckMemory(sm, meta);
  meta->Restore();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppCompileTimeConstantError;
  using analyse::errors::SppCompileTimeConstantError;

  // When coming from stage7 (also limit this allowance based on meta->CurrentStage for when we expand to cmp generics?)
  auto revoke = false;
  if (not _OverloadInfo.has_value()) {
    revoke = true;
    Stage7_AnalyseSemantics(sm, meta);
  }

  // Get the function prototype and resolve it.
  auto const *const fn_proto = _OverloadInfo->Proto->GetNonGenericImpl();
  RaiseIf<SppCompileTimeConstantError>(
    fn_proto->TokCmp == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Todo: For now, don't allow folding in comptime.
  RaiseIf<SppCompileTimeConstantError>(
    Fold != nullptr,
    {sm->CurrentScope}, ERR_ARGS(*Fold));

  // Create the argument map for the function to use. Positional arguments (including the implicit "self"
  // injected for method-call syntax) are matched to parameters by position; keyword arguments by name.
  const auto fn_params = fn_proto->FnParamGroup->GetAllParams();
  auto args = Vec<Pair<Shared<IdentifierAst>, Unique<ExpressionAst>>>();
  for (auto const &[i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
    const auto kw_arg = arg->To<FunctionCallArgumentKeywordAst>();
    auto name = kw_arg != nullptr ? kw_arg->Name : fn_params[i]->ExtractName();
    arg->Stage9_CompTimeResolve(sm, meta);
    args.EmplaceBack(std::move(name), std::move(meta->CmpResult));
  }
  auto fn_arg_map = decltype(meta->CmpArgs)();
  auto gn_arg_type_map = decltype(meta->CmpGnTypeArgs)();
  auto gn_arg_comp_map = decltype(meta->CmpGnCompArgs)();
  for (auto &&[name, val] : args) { fn_arg_map[name] = std::move(val); }
  for (auto &&gn_arg : GnArgGroup->GetTypeArgs()) { gn_arg_type_map.EmplaceBack(gn_arg->Val.get()); }
  for (auto &&gn_arg : GnArgGroup->GetCompArgs()) { gn_arg_comp_map.EmplaceBack(gn_arg->Val.get()); }

  // Resolve the function with the arguments.
  meta->Save();
  meta->CmpArgs = std::move(fn_arg_map);
  meta->CmpGnTypeArgs = std::move(gn_arg_type_map);
  meta->CmpGnCompArgs = std::move(gn_arg_comp_map);
  auto tm = ScopeManager(sm->GlobalScope, fn_proto->GetAstScope());
  tm.Reset(not tm.CurrentScope->Children.IsEmpty() ? tm.CurrentScope->Children[0].get() : tm.CurrentScope);
  fn_proto->Impl->Stage9_CompTimeResolve(&tm, meta);
  meta->Restore();

  // Every function reaches comp-time resolution through here, so this is where an integer result is checked against
  // what its type can hold. Comp-time arithmetic is exact, so a result that does not fit arrives intact rather than
  // having wrapped on the way out. Checking per call - rather than once at the end - is what makes it agree with the
  // same expression at runtime: an intermediate that overflows overflows either way.
  const auto owner = Source.OriginalExpr != nullptr ? Source.OriginalExpr : static_cast<Ast*>(this);
  if (const auto int_result = meta->CmpResult != nullptr ? meta->CmpResult->To<IntegerLiteralAst>() : nullptr) {
    int_result->ValidateBounds(*owner, *sm);
  }
  else if (const auto flt_result = meta->CmpResult != nullptr ? meta->CmpResult->To<FloatLiteralAst>() : nullptr) {
    flt_result->ValidateBounds(*owner, *sm);
  }


  if (revoke) {
    _OverloadInfo.reset();
  }
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx) -> llvm::Value* {
  // For folding, generate the code for the folded
  // transformations and combine into single block.
  if (Fold != nullptr) {
    const auto merge = InnerScopeExpressionAst::NewEmpty();
    merge->Members = _FoldedAsts
      | genex::views::transform([&meta](auto &&ast) {
        auto clone_lhs = AstClone(meta->PostfixExpressionLhs);
        auto pf = MakeUnique<PostfixExpressionAst>(std::move(clone_lhs), std::move(ast));
        return Unique<StatementAst>(pf.release());
      })
      | genex::to<Vec>();
    return merge->Stage11_CodeGen(sm, meta, ctx);
  }

  // Closure calls: the left-hand side is a closure value, a FunXXX type, which lowers to a { fn_ptr, env_ptr } pair.
  // Extract the two pointers and call through fn_ptr, prepending the environment pointer (the closure function is
  // compiled as "(env*, ...params) -> ret").
  if (_ClosureDummyProto != nullptr) {
    const auto closure_uid = "." + spp::utils::Uid(this);
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto closure_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);

    // The lhs' static type determines the physical field indices of "{ fn_ptr, env_ptr }": a plain "FunXXX" has no
    // extra fields, but a class that superimposes one (see "GetFatPointerFields") may declare its own attributes
    // too, and the "Spp" layout can reorder any of them - "GetPhysicalFieldIndex" maps back from the fixed
    // declared prefix (0, 1) to wherever they actually ended up.
    const auto lhs_ty = meta->PostfixExpressionLhs->InferType(sm, meta)->WithConvention(nullptr);
    const auto lhs_type_sym = sm->CurrentScope->GetTypeSymbol(lhs_ty.get());
    const auto fn_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 0);
    const auto env_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 1);

    // The lhs is the { fn_ptr, env_ptr } value directly, or for a borrowed closure, a pointer to it, so read the
    // fields accordingly.
    auto fn_ptr = static_cast<llvm::Value*>(nullptr);
    auto env_ptr = static_cast<llvm::Value*>(nullptr);
    if (closure_val->getType()->isPointerTy()) {
      const auto closure_ty = llvm::cast<llvm::StructType>(codegen::GetLlvmType(*lhs_type_sym, ctx));
      fn_ptr = ctx->Builder.CreateLoad(
        ptr_ty, ctx->Builder.CreateStructGEP(closure_ty, closure_val, fn_ptr_idx), "closure.fn_ptr" + closure_uid);
      env_ptr = ctx->Builder.CreateLoad(
        ptr_ty, ctx->Builder.CreateStructGEP(closure_ty, closure_val, env_ptr_idx), "closure.env_ptr" + closure_uid);
    }
    else {
      fn_ptr = ctx->Builder.CreateExtractValue(closure_val, {fn_ptr_idx}, "closure.fn_ptr" + closure_uid);
      env_ptr = ctx->Builder.CreateExtractValue(closure_val, {env_ptr_idx}, "closure.env_ptr" + closure_uid);
    }

    // Generate the argument values, prepending the environment pointer.
    auto closure_args = FnArgGroup->Args
      | genex::views::transform([sm, meta, ctx](auto const &x) { return x->Stage11_CodeGen(sm, meta, ctx); })
      | genex::to<Vec>();
    closure_args.Insert(closure_args.begin(), env_ptr);

    // Reconstruct the closure's function type ("(env*, ...params) -> ret") to call through the pointer.
    auto closure_param_tys = closure_args
      | genex::views::transform([](auto const &v) { return v->getType(); })
      | genex::to<Vec>();
    const auto closure_ret_ty = codegen::GetLlvmTypeOf(*InferType(sm, meta), *sm->CurrentScope, ctx);
    const auto closure_fn_ty = llvm::FunctionType::get(closure_ret_ty, closure_param_tys.ToStdVector(), false);

    // A call returning Void cannot be given a name (llvm forbids naming void values).
    return closure_ret_ty->isVoidTy()
      ? ctx->Builder.CreateCall(closure_fn_ty, fn_ptr, closure_args.ToStdVector())
      : ctx->Builder.CreateCall(closure_fn_ty, fn_ptr, closure_args.ToStdVector(), "closure.call" + closure_uid);
  }

  // Coroutine calls: calling a coroutine does not run its body, it
  // constructs a generator. The frame is owned by the llvm coroutine
  // intrinsics, and the value handed back is nothing but the
  // "llvm.coro.begin" handle.
  const auto is_coroutine_call = Target()->IsCoroutine();

  // For generically converted function prototypes, generate
  // their llvm declaration in-walk if it is still missing.
  if (Target()->GetLlvmFunc() == nullptr) {
    auto tm = ScopeManager(sm->GlobalScope, const_cast<analyse::scopes::Scope*>(_OverloadInfo->OverloadScope));
    tm.Reset(tm.CurrentScope);
    const auto owner_ctx = Target()->OwnerCtx();
    Target()->GenerateLlvmDeclaration(
      &tm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
  }

  const auto uid = "." + spp::utils::Uid(this);
  const auto o = "Call target has no llvm declaration: " + Target()->PrintSignature("");
  RaiseIf<analyse::errors::SppInternalCompilerError>(
    Target()->GetLlvmFunc() == nullptr, {sm->CurrentScope}, ERR_ARGS(*this, o));

  // Because we have individual modules for each compilation
  // unit, the declaration for the target has to be added to
  // the module the call is being emitted into.
  auto llvm_func = Target()->GetLlvmFunc()->Target;
  SPP_ASSERT(llvm_func != nullptr);
  llvm_func = codegen::GetOrAddTargetIntoCurrentModule(
    *llvm_func, *codegen::GetEmissionModule(*ctx));

  // The arguments have already been reordered to match the
  // parameters, so the two line up index for index.
  const auto &fn_params = Target()->FnParamGroup->Params;
  auto llvm_func_args = Vec<llvm::Value*>();
  llvm_func_args.Reserve(FnArgGroup->Args.Len());

  for (auto i = 0uz; i < FnArgGroup->Args.Len(); ++i) {
    auto const &arg = FnArgGroup->Args[i];
    auto llvm_arg = arg->Stage11_CodeGen(sm, meta, ctx);
    SPP_ASSERT(llvm_arg != nullptr);

    // The parameter's type is named where the overload lives,
    // so it is qualified there and then re-resolved here. The
    // coercion compares the two types from this scope, and a
    // parameter that does not resolve from it (a "Self" or a
    // generic still standing in for one) is not a variant this
    // call has to widen into anyway.
    const auto param_type_sym = i < fn_params.Len()
      ? _OverloadInfo->OverloadScope->GetTypeSymbol(fn_params[i]->Type.get())
      : nullptr;
    const auto param_type = param_type_sym != nullptr ? param_type_sym->FqName() : nullptr;

    if (param_type != nullptr and sm->CurrentScope->GetTypeSymbol(param_type.get()) != nullptr) {
      llvm_arg = codegen::CoerceToVariant(
        llvm_arg, *param_type, *arg->InferType(sm, meta),
        *sm->CurrentScope, "arg.variant" + uid, ctx);
      SPP_ASSERT(llvm_arg != nullptr);
    }

    // Just because a argument type is a borrow, it doesn't mean
    // that the borrow is happening here. For example, if "x" is
    // "&X", that borrow can then be moved into "fun a(y: &X)" -
    // re don't re-borrow just because it's a borrow type.
    const auto self_param = i < fn_params.Len()
      ? fn_params[i]->To<FunctionParameterSelfAst>()
      : nullptr;

    const auto param_by_value = i < fn_params.Len() and (self_param != nullptr
      ? self_param->Conv == nullptr
      : fn_params[i]->Type->GetConvention() == nullptr);

    if (param_by_value and llvm_arg->getType()->isPointerTy()) {
      const auto arg_type = arg->InferType(sm, meta);
      if (arg_type->GetConvention() != nullptr) {
        if (const auto llvm_arg_type = codegen::GetLlvmTypeOf(
          *arg_type->WithoutConvention(), *sm->CurrentScope, ctx); llvm_arg_type != nullptr) {
          llvm_arg = ctx->Builder.CreateLoad(llvm_arg_type, llvm_arg, "arg.copy" + uid);
        }
      }
    }
    llvm_func_args.EmplaceBack(llvm_arg);
  }

  // Create the call instruction (a call returning Void cannot be given a name - llvm forbids naming void
  // values).
  if (llvm_func->getReturnType()->isVoidTy()) {
    return ctx->Builder.CreateCall(llvm_func, llvm_func_args.ToStdVector());
  }
  const auto llvm_call = ctx->Builder.CreateCall(
    llvm_func, llvm_func_args.ToStdVector(), "call" + uid);

  // Todo: Document this.
  if (is_coroutine_call) {
    llvm_call->addFnAttr(llvm::Attribute::CoroElideSafe);
  }

  // A generator is either bound to something ("let g = f()",
  // so a later "res" can find it again) or collapsed on the
  // spot into the one value it yields ("v[0]" reads as the
  // element, not as a generator over it). Both need the frame's
  // promise, which is reached through the handle.
  if (is_coroutine_call and meta->LlvmAssignmentTarget != nullptr) {
    auto llvm_coro_handle = static_cast<llvm::Value*>(llvm_call);
    if (not llvm_call->getType()->isPointerTy()) {
      const auto coro_ret_type_sym =
        _OverloadInfo->OverloadScope->GetTypeSymbol(Target()->ReturnType.get());
      const auto handle_idx = codegen::GetPhysicalFieldIndex(
        *coro_ret_type_sym->LlvmInfo, 0);
      llvm_coro_handle = ctx->Builder.CreateExtractValue(
        llvm_call, {handle_idx}, "coro.handle" + uid);
    }

    const auto llvm_promise_align = llvm::ConstantInt::get(
      llvm::Type::getInt32Ty(*ctx->Context), alignof(std::max_align_t));
    const auto llvm_gen_state = ctx->Builder.CreateIntrinsic(
      llvm::Intrinsic::coro_promise, {}, {llvm_coro_handle, llvm_promise_align, ctx->Builder.getFalse()}, {},
      "coro.gen.state" + uid);

    if (meta->LlvmAssignmentTarget != nullptr) {
      auto generator = MakeUnique<codegen::LlvmGenerator>();
      generator->Handle = llvm_coro_handle;
      generator->State = llvm_gen_state;
      ctx->LlvmGenerators[meta->LlvmAssignmentTarget] = std::move(generator);
    }
  }

  return llvm_call;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using generate::common_types::SelfType;
  using generate::common_types::TupleType;
  using analyse::utils::type_utils::ResolveAndSubstituteSelfType;

  // For function folding, collect a tuple of all return types.
  if (not _FoldedAsts.IsEmpty()) {
    auto folded_return_types = _FoldedAsts
      | genex::views::transform([sm, meta](auto const &ast) { return ast->InferType(sm, meta); })
      | genex::to<Vec>();
    auto tuple_type = TupleType(0, std::move(folded_return_types));
    return tuple_type;
  }

  // Get the function return type from the overload.
  auto ret_type = _OverloadInfo->Proto->ReturnType;

  // If there is a scope present (non-closure), then fully qualify the return type.
  if (_OverloadInfo->OverloadScope != nullptr and not ret_type->IsSelfType()) {
    ret_type = _OverloadInfo->OverloadScope->GetTypeSymbol(ret_type.get())->FqName();
  }

  // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
  if (_IsCoroAndAutoResume and not meta->PreventAutoGeneratorResume) {
    auto [_, yield_type, _] = analyse::utils::type_utils::GetGenAndYieldTypes(
      *ret_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "function call");
    ret_type = yield_type;
  }

  const auto pf = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
  const auto is_runtime = pf ? pf->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr : false;
  const auto is_static = pf
    ? pf->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>() != nullptr and pf->Lhs->To<TypeAst>()
    : false;

  if (ret_type->IsSelfType()) {
    ret_type =
      meta->PostfixExpressionLhs->To<PostfixExpressionAst>()->Lhs->InferType(sm, meta)->WithConvention(nullptr);
  }
  else if (pf and (is_runtime or is_static)) {
    // Perform a "Self=FQType" substitution to handle "Self" being part of the generics of the return type.
    // Todo: use Resolve method (which scope??)
    const auto inferred = is_runtime
      ? pf->Lhs->InferType(sm, meta)
      : AstClone(pf->Lhs->ToUnchecked<TypeAst>());

    auto generic = MakeUnique<GenericArgumentTypeKeywordAst>(
      SelfType(0), nullptr,
      inferred->WithConvention(nullptr));

    const auto generic_group = GenericArgumentGroupAst::NewEmpty();
    generic_group->Args.PushBack(std::move(generic));
    ret_type = ret_type->SubstituteGenerics(generic_group->GetAllArgs());
  }

  // Generic instantiations embedded in the return type (eg "Var[Tup[Pass[Str], Fail[Utf8Err]]]" from a "Res[...]"
  // alias) are only registered in the scope of whichever module first analysed them (see CreateGenericClsScope), and
  // that scope isn't guaranteed to be reachable from this call site's scope. Rather than relying on lookup finding a
  // registration made elsewhere, re-analyse a fresh clone here so the instantiation is guaranteed to exist (and be
  // reachable) from this scope too before anything downstream tries to look up its symbol.
  ret_type = AstCloneShared(ret_type);
  ret_type->Stage7_AnalyseSemantics(sm, meta);

  // Return the type.
  return ret_type;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::MarkAsAsync(
  Ast *async_token)
  -> void {
  _IsAsync = async_token;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::Target() const
  -> FunctionPrototypeAst* {
  if (not _OverloadInfo.has_value()) { return nullptr; }
  const auto target_proto = _OverloadInfo->Proto;
  if (const auto coro_proto = target_proto->To<CoroutinePrototypeAst>(); coro_proto != nullptr and coro_proto->IsOnce()) {
    return coro_proto->GenOnceLowered();
  }
  return target_proto;
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::SetClosureDummyProto(
  Unique<FunctionPrototypeAst> &&proto) -> void {
  _ClosureDummyProto = std::move(proto);
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::SetTransformedAst(
  Unique<PostfixExpressionAst> &&ast)
  -> void {
  _TransformedAst = std::move(ast);
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::GetTransformedAst() const
  -> PostfixExpressionAst* {
  return _TransformedAst.get();
}

auto spp::asts::PostfixExpressionOperatorFunctionCallAst::_HandleFunctionFolding(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> {
  // Populate the list of arguments to fold.
  auto folded_args = Vec<FunctionCallArgumentAst*>{};
  auto folded_arg_types = Vec<TypeAst*>{};
  auto folded_tup_lens = Vec<std::size_t>{};
  auto fold_indexes = Vec<std::size_t>{};
  for (auto [i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
    auto arg_type = arg->InferType(sm, meta);
    if (analyse::utils::type_utils::IsTypeTup(*arg_type, *sm->CurrentScope)) {
      fold_indexes.EmplaceBack(i);
      folded_args.EmplaceBack(arg);
      folded_arg_types.EmplaceBack(arg_type.get());
      folded_tup_lens.EmplaceBack(arg_type->LastTypePart()->GnArgGroup->Args.Len());
    }
  }

  // Build the unrolled AST transformations.
  const auto smallest_tuple = genex::min_element(folded_tup_lens);
  auto transformed_asts = Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>{};
  for (auto i = 0uz; i < smallest_tuple; ++i) {
    auto new_arg_group = AstClone(FnArgGroup);
    for (const auto fold_index : fold_indexes) {
      // Create the postfix access into the tuple.
      auto id = MakeUnique<IdentifierAst>(new_arg_group->Args[fold_index]->Val->PosEnd(), std::to_string(i));
      auto ma = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(id));
      auto pf = MakeUnique<PostfixExpressionAst>(std::move(new_arg_group->Args[fold_index]->Val), std::move(ma));
      new_arg_group->Args[fold_index]->Val = std::move(pf);
    }

    auto transformed_ast = AstClone(this);
    transformed_ast->FnArgGroup = std::move(new_arg_group);
    transformed_ast->Fold = nullptr;
    transformed_asts.EmplaceBack(std::move(transformed_ast));
  }

  // Return the transformed asts.
  return transformed_asts;
}

SPP_MOD_END
