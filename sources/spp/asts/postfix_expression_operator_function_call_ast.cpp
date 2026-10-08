module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
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
import spp.asts.generic_argument_group_ast;
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
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
  decltype(GnArgGroup) &&generic_arg_group,
  decltype(FnArgGroup) &&arg_group,
  decltype(Fold) &&fold) :
  GnArgGroup(std::move(generic_arg_group)),
  FnArgGroup(std::move(arg_group)),
  Fold(std::move(fold)),
  _OverloadInfo(std::nullopt),
  _TransformedLhs(nullptr),
  _ClosureDummyArgGroup(nullptr),
  _ClosureDummyArg(nullptr),
  _ClosureDummyProto(nullptr),
  _IsAsync(nullptr),
  _IsCoroAndAutoResume(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
  Source.OriginalExpr = this;
}

PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;

auto PostfixExpressionOperatorFunctionCallAst::PosStart() const -> std::size_t {
  // Use the generic argument group.
  return not GnArgGroup->Args.IsEmpty() ? GnArgGroup->PosStart() : FnArgGroup->PosStart();
}

auto PostfixExpressionOperatorFunctionCallAst::PosEnd() const -> std::size_t {
  // Use the fold or function argument group.
  return Fold ? Fold->PosEnd() : FnArgGroup->PosEnd();
}

auto PostfixExpressionOperatorFunctionCallAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    AstClone(GnArgGroup),
    AstClone(FnArgGroup),
    AstClone(Fold));
  if (Source.OriginalExpr != this) {
    ast->Source.OriginalExpr = Source.OriginalExpr;
  }
  ast->_ClosureDummyProto = AstClone(_ClosureDummyProto);
  ast->_TransformedLhs = AstClone(_TransformedLhs);
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

auto PostfixExpressionOperatorFunctionCallAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(_TransformedLhs);
  SPP_STRING_APPEND(GnArgGroup);
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_APPEND(Fold);
  SPP_STRING_END;
}

auto PostfixExpressionOperatorFunctionCallAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;
  using generate::common_types_precompiled::FUN_REF;
  using generate::common_types_precompiled::FUN_MUT;

  // Prevent double analysis.
  // Todo: See why this might be happening anyway, and remove this check preferably.
  if (_OverloadInfo.has_value()) { return; }

  // Analyse the generic arguments and the function call
  // arguments before determining the overload.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    FnArgGroup->ExpectedTypes = overload_resolution::ExpectedArgTypes(*this, sm, meta);
    FnArgGroup->Stage7_AnalyseSemantics(sm, meta);
  }

  // If we are function folding, create transformed asts.
  if (Fold != nullptr) {
    _FoldedAsts = _HandleFnFolding(sm, meta);
    for (auto const &ast : _FoldedAsts) { ast->Stage7_AnalyseSemantics(sm, meta); }
    return;
  }

  // Resolve the overload for this function call.
  auto [overload, is_closure] = overload_resolution::DetermineOverload(*this, sm, meta);

  // Special case for closures; apply the convention the
  // closure name to ensure is it movable/mutable etc.
  if (is_closure) {
    const auto lhs_type = fn_values::IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
    auto dummy_self_arg = MakeUnique<FunctionCallArgumentPositionalAst>(
      nullptr, nullptr, AstClone(meta->PostfixExpressionLhs));

    // The callable's function type, as the template it stands for (a borrow of one is neither).
    const auto lhs_head = type_predicates::HeadKindRef(*lhs_type, *sm->CurrentScope);
    const auto is_fun = [&](auto const &tmpl) {
      return lhs_head.KindSymbol() != nullptr and lhs_head.IsA(*tmpl, *sm->CurrentScope);
    };
    if (is_fun(FUN_MUT)) {
      dummy_self_arg->Conv = MakeUnique<ConventionMutAst>(nullptr, nullptr);
      dummy_self_arg->Conv->To<ConventionMutAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
    }
    else if (is_fun(FUN_REF)) {
      dummy_self_arg->Conv = MakeUnique<ConventionRefAst>(nullptr);
      dummy_self_arg->Conv->To<ConventionRefAst>()->TokBorrow->PatchPos(meta->PostfixExpressionLhs->PosStart());
    }
    _ClosureDummyArg = std::move(dummy_self_arg);
  }

  // Set the overload to the only pass overload.
  _OverloadInfo = _OInfo{
    .OverloadScope = overload.FnScope,
    .Proto = overload.Proto,
    .SelfType = overload.SelfType
  };

  // Use the hook to record information for the resolution and
  // completion plugin.
  if (overload.FnScope != nullptr) {
    lsp::resolution_index::RecordFnCallArguments(
      *FnArgGroup, *sm, *meta, *overload.Proto, *overload.FnScope);
  }

  // Only now are the arguments rewritten into what the chosen overload takes.
  overload_resolution::ElaborateCall(*FnArgGroup, overload, sm, meta);

  // An argument naming a function, passed as a function type, is
  // the overload that type picks; a generic one is minted here.
  auto const &params = _OverloadInfo->Proto->FnParamGroup->Params;
  for (auto i = 0uz; i < FnArgGroup->Args.Len() and i < params.Len(); ++i) {
    fn_values::InstantiateFnValue(
      FnArgGroup->Args[i]->InferTypeRef(sm, meta),
      TypeRef::Of(*params[i]->Type, *sm->CurrentScope), sm, meta);
  }

  // A unit test belongs to the harness, not to the program.
  // Calling one would run it as part of whatever called it,
  // and there is no sensible meaning for that, so the call
  // is rejected wherever it appears.
  if (const auto test_annotation = _OverloadInfo->Proto->TestAnnotation;
    test_annotation != nullptr and not meta->IsTestHarness) {
    Raise<SppUnitTestNotCallableError>(
      {_OverloadInfo->OverloadScope, sm->CurrentScope},
      ERR_ARGS(*this, *test_annotation));
  }

  // Check that if we are in a cmp context, that the overload
  // is also cmp.
  RaiseIf<SppInvalidCompTimeOperationError>(
    meta->EnclosingFnCmp != nullptr and _OverloadInfo->Proto->TokCmp == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Special case for GenOnce called as a coroutine => auto
  // move into the "Yield" type.
  if (_OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_COR) {
    // This needs to be any type that EXTENDS GenOnce, not
    // just GenOnce itself.
    auto const &proto_ret_type = _OverloadInfo->Proto->ReturnType;
    const auto gen = marker_sups::FindGenSup(
      TypeRef::Of(*proto_ret_type, *sm->CurrentScope), *sm->CurrentScope,
      *meta->PostfixExpressionLhs, [&] { return proto_ret_type; }, "GenOnce collapse");
    _IsCoroAndAutoResume = marker_sups::IsGenOnce(gen, *sm->CurrentScope);
  }

  // Todo: Is this needed?
  const auto ret_type = InferType(sm, meta);
  RaiseIf<SppSecondClassBorrowViolationError>(
    _OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_FUN
    and type_predicates::IsTypeBorrowed(*ret_type->WithoutConvention(), *sm),
    {_OverloadInfo->OverloadScope, sm->CurrentScope},
    ERR_ARGS(*this, *ret_type, "function return type"));
}

auto PostfixExpressionOperatorFunctionCallAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // If a fold is taking place, analyse the folded
  // transformations.
  if (Fold != nullptr) {
    for (auto const &ast : _FoldedAsts) { ast->Stage8_CheckMemory(sm, meta); }
    return;
  }

  // If a closure is being called, apply memory rules to
  // the symbolic target.
  if (_ClosureDummyArg != nullptr and _ClosureDummyArg->Conv == nullptr) {
    // Calling a "FunMov" consumes it where it stands, so what it
    // borrows is used up with it rather than carried anywhere -
    // unlike moving the closure away, which the rule against
    // moving a holder of escaping borrows is there for.
    _ClosureDummyArg->Val->Stage7_AnalyseSemantics(sm, meta);
    mem_utils::ValidateSymbolMemory(
      *_ClosureDummyArg->Val, *_ClosureDummyArg, *sm, meta, {.CheckEscapingBorrowMove = false});
  }
  else if (_ClosureDummyArg != nullptr) {
    auto closure_args = Vec<Unique<FunctionCallArgumentAst>>();
    closure_args.EmplaceBack(std::move(_ClosureDummyArg));
    _ClosureDummyArgGroup = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(closure_args), nullptr);
    _ClosureDummyArgGroup->Stage7_AnalyseSemantics(sm, meta);
    _ClosureDummyArgGroup->Stage8_CheckMemory(sm, meta);
  }

  // Check the argument group, now the old borrows have
  // been invalidated.
  GnArgGroup->Stage8_CheckMemory(sm, meta);

  const auto _meta_guard = MetaGuard(meta);
  meta->TargetCallFnPrototype = _OverloadInfo->Proto;
  meta->TargetCallWasFnAsync = _IsAsync;
  FnArgGroup->Stage8_CheckMemory(sm, meta);
}

auto PostfixExpressionOperatorFunctionCallAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // When coming from stage7 (also limit this allowance based
  // on meta->CurrentStage for when we expand to cmp generics?)
  auto revoke = false;
  if (not _OverloadInfo.has_value()) {
    revoke = true;
    Stage7_AnalyseSemantics(sm, meta);
  }

  // Get the function prototype and resolve it.
  auto const *const fn_proto = _OverloadInfo->Proto->GetNonGnImpl();
  RaiseIf<SppCompTimeConstantError>(
    fn_proto->TokCmp == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*this));

  // Todo: For now, don't allow folding in comptime.
  RaiseIf<SppCompTimeConstantError>(
    Fold != nullptr,
    {sm->CurrentScope}, ERR_ARGS(*Fold));

  // Create the argument map for the function to use. Positional
  // arguments (including the implicit "self" injected for
  // method-call syntax) are matched to parameters by position;
  // keyword arguments by name.
  const auto fn_params = fn_proto->FnParamGroup->GetAllParams();
  auto args = Vec<Pair<Shared<IdentifierAst>, Unique<ExpressionAst>>>();
  for (auto const &[i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
    const auto kw_arg = arg->To<FunctionCallArgumentKeywordAst>();
    auto name = kw_arg != nullptr ? kw_arg->Name : fn_params[i]->ExtractName();
    arg->Stage9_CompTimeResolve(sm, meta);
    args.EmplaceBack(std::move(name), std::move(meta->CompTimeResult));
  }
  auto fn_arg_map = decltype(meta->CompTimeArgs)();
  auto gn_arg_type_map = decltype(meta->CompTimeGnTypeArgs)();
  auto gn_arg_comp_map = decltype(meta->CompTimeGnCompArgs)();
  for (auto &&[name, val] : args) { fn_arg_map[name] = std::move(val); }
  for (auto &&gn_arg : GnArgGroup->GetTypeArgs()) {
    gn_arg_type_map.EmplaceBack(MakeShared<TypeRef>(
      TypeRef::Of(*gn_arg->TypeVal, *sm->CurrentScope)));
  }
  for (auto &&gn_arg : GnArgGroup->GetCompArgs()) {
    gn_arg_comp_map.EmplaceBack(sm->CurrentScope->CompIdOf(*gn_arg->CompVal));
  }

  // Resolve the function with the arguments. The first call to
  // be folded is the one the user wrote, and the calls it makes
  // report their errors there rather than inside std.
  const auto owner = Source.OriginalExpr != nullptr ? Source.OriginalExpr : static_cast<Ast*>(this);
  const auto *const outer_site = meta->CompTimeCallSite;
  auto *const outer_scope = meta->CompTimeCallSiteScope;

  // Each nested call is a nested evaluation on the compiler's
  // own stack, so recursion that never bottoms out has to be
  // stopped here, rather than left to overflow it.
  static thread_local auto cmp_call_depth = 0uz;
  constexpr auto kMaxCompTimeCallDepth = 1000uz;
  struct CompTimeCallDepthGuard {
    decltype(cmp_call_depth) &Depth;
    explicit CompTimeCallDepthGuard(decltype(cmp_call_depth) &depth) : Depth(depth) { ++Depth; }
    ~CompTimeCallDepthGuard() { --Depth; }
  } const depth_guard(cmp_call_depth);
  RaiseIf<SppInvalidCompTimeOperationError>(
    cmp_call_depth > kMaxCompTimeCallDepth,
    {outer_scope != nullptr ? outer_scope : sm->CurrentScope}, ERR_ARGS(outer_site != nullptr ? *outer_site : *owner));
  {
    const auto _meta_guard = MetaGuard(meta);
    if (outer_site == nullptr) {
      meta->CompTimeCallSite = owner;
      meta->CompTimeCallSiteScope = sm->CurrentScope;
    }
    meta->CompTimeArgs = std::move(fn_arg_map);
    meta->CompTimeGnTypeArgs = std::move(gn_arg_type_map);
    meta->CompTimeGnCompArgs = std::move(gn_arg_comp_map);
    auto tm = ScopeManager(
      sm->GlobalScope, fn_proto->GetAstScope());
    tm.Reset(not tm.CurrentScope->Children.IsEmpty() ? tm.CurrentScope->Children[0].get() : tm.CurrentScope);
    fn_proto->Impl->Stage9_CompTimeResolve(&tm, meta);
  }

  // Every function reaches comp-time resolution through here,
  // so this is where an integer result is checked against what
  // its type can hold. Comp-time arithmetic is exact, so a
  // result that does not fit arrives intact rather than having
  // wrapped on the way out. Checked per call to catch overflow.
  auto const &site = outer_site != nullptr ? *outer_site : *owner;
  auto const &site_scope = outer_site != nullptr ? *outer_scope : *sm->CurrentScope;
  if (const auto int_result =
    meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<IntegerLiteralAst>() : nullptr) {
    int_result->ValidateBounds(site, site_scope);
  }
  else if (const auto flt_result =
    meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<FloatLiteralAst>() : nullptr) {
    flt_result->ValidateBounds(site, site_scope);
  }

  if (revoke) {
    _OverloadInfo.reset();
  }
}

auto PostfixExpressionOperatorFunctionCallAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  //
  IMPORT_UTILS_AND_UID;

  // For folding, generate the code for the folded
  // transformations and combine into single block.
  // Each is generated in place, from a clone so the fold can be
  // generated again, and their results make up the tuple the fold
  // is typed as. An inner scope built here to hold them would step
  // the scope walk into a scope that was never created.
  if (Fold != nullptr) {
    const auto tuple_type = InferType(sm, meta);
    auto results = Vec<llvm::Value*>();
    for (auto const &ast : _FoldedAsts) {
      auto pf = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), AstClone(ast));
      results.EmplaceBack(pf->Stage11_CodeGen(sm, meta, ctx));
    }

    const auto tuple_sym = sm->CurrentScope->FindTypeSymbol(tuple_type.get());
    const auto tuple_llvm_type = tuple_sym != nullptr ? codegen::GetLlvmType(*tuple_sym, ctx) : nullptr;
    if (tuple_llvm_type == nullptr or not tuple_llvm_type->isStructTy()) { return nullptr; }
    auto tuple_val = static_cast<llvm::Value*>(llvm::PoisonValue::get(tuple_llvm_type));
    for (auto i = 0uz; i < results.Len(); ++i) {
      if (results[i] == nullptr or results[i]->getType()->isVoidTy()) { continue; }
      tuple_val = ctx->Builder.CreateInsertValue(
        tuple_val, results[i], {codegen::GetPhysicalFieldIndex(*tuple_sym->LlvmInfo, i)});
    }
    return tuple_val;
  }

  // Closure calls: the left-hand side is a closure value, a
  // FunXXX type, which lowers to a { fn_ptr, env_ptr } pair.
  // Extract the two pointers and call through fn_ptr,
  // prepending the environment pointer (the closure function
  // is compiled as "(env*, ...params) -> ret").
  if (_ClosureDummyProto != nullptr) {
    const auto closure_uid = "." + Uid();
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto closure_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);

    // The lhs' static type determines the physical field indices
    // of "{ fn_ptr, env_ptr }": a plain "FunXXX" has no extra
    // fields, but a class that superimposes one may declare its
    // own attributes too, and the "spp" layout can reorder any of
    // them - "GetPhysicalFieldIndex" maps back from the fixed
    // declared prefix (0, 1) to wherever they actually ended up.
    const auto lhs_ty = meta->PostfixExpressionLhs->InferType(sm, meta)->WithConvention(nullptr);
    const auto lhs_type_sym = sm->CurrentScope->FindTypeSymbol(lhs_ty.get());
    const auto fn_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 0);
    const auto env_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 1);

    // The lhs is the { fn_ptr, env_ptr } value directly, or for
    // a borrowed closure, a pointer to it, so read the fields
    // accordingly.
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

    // Generate the argument values, prepending the environment
    // pointer.
    auto closure_args = FnArgGroup->Args
      | genex::views::transform([sm, meta, ctx](auto const &x) { return x->Stage11_CodeGen(sm, meta, ctx); })
      | genex::to<Vec>();
    closure_args.Insert(closure_args.begin(), env_ptr);

    // Reconstruct the closure's function type
    // ("(env*, ...params) -> ret") to call through the pointer.
    auto closure_param_tys = closure_args
      | genex::views::transform([](auto const &v) { return v->getType(); })
      | genex::to<Vec>();

    // Bandaid for resolving generic issues with closure return
    // types, before any variant coercion is done. Todo: tidy this
    // up.
    const auto expected_ret_type = InferType(sm, meta);
    auto actual_ret_type = expected_ret_type;
    if (const auto callable = marker_sups::FindFnSup(*lhs_ty, *sm->CurrentScope); callable.Symbol != nullptr) {
      if (auto out = callable.Symbol->TypeArg("Out"); out != nullptr) { actual_ret_type = std::move(out); }
    }

    const auto closure_ret_ty = codegen::GetLlvmTypeOf(
      TypeRef::Of(*actual_ret_type, *sm->CurrentScope), ctx);
    const auto closure_fn_ty = llvm::FunctionType::get(
      closure_ret_ty, closure_param_tys.ToStdVector(), false);

    // A call returning Void cannot be given a name (llvm forbids
    // naming void values).
    if (closure_ret_ty->isVoidTy()) {
      return ctx->Builder.CreateCall(closure_fn_ty, fn_ptr, closure_args.ToStdVector());
    }

    // Build the proper (named) call and then do the variant
    // coercion.
    const auto closure_call = ctx->Builder.CreateCall(
      closure_fn_ty, fn_ptr, closure_args.ToStdVector(), "closure.call" + closure_uid);
    return codegen::CoerceToVariant(
      closure_call, TypeRef::Of(*expected_ret_type, *sm->CurrentScope),
      TypeRef::Of(*actual_ret_type, *sm->CurrentScope), *sm->CurrentScope,
      "closure.ret" + closure_uid, ctx);
  }

  // Coroutine calls: calling a coroutine does not run its body,
  // it constructs a generator. The frame is owned by the llvm
  // coroutine intrinsics, and the value handed back is nothing
  // but the "llvm.coro.begin" handle.
  const auto is_coroutine_call = Target()->IsCoroutine();

  // For generically converted function prototypes, generate
  // their llvm declaration in-walk if it is still missing.
  if (Target()->GetLlvmFn() == nullptr) {
    auto tm = ScopeManager(
      sm->GlobalScope, const_cast<Scope*>(_OverloadInfo->OverloadScope));
    tm.Reset(tm.CurrentScope);
    const auto owner_ctx = Target()->OwnerCtx();
    Target()->GenerateLlvmDeclaration(
      &tm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
  }

  const auto uid = "." + Uid();
  const auto o = "Call target has no llvm declaration: " + Target()->PrintSignature("");
  RaiseIf<SppInternalCompilerError>(
    Target()->GetLlvmFn() == nullptr, {sm->CurrentScope}, ERR_ARGS(*this, o));

  // Because we have individual modules for each compilation
  // unit, the declaration for the target has to be added to
  // the module the call is being emitted into.
  auto llvm_func = Target()->GetLlvmFn()->Target;
  SPP_ASSERT(llvm_func != nullptr);
  llvm_func = codegen::GetOrAddTargetIntoCurrentModule(
    *llvm_func, *codegen::GetEmissionModule(*ctx));

  // The arguments have already been reordered to match the
  // parameters, so the two line up index for index.
  const auto &fn_params = Target()->FnParamGroup->Params;
  auto llvm_func_args = Vec<llvm::Value*>();
  llvm_func_args.Reserve(FnArgGroup->Args.Len());

  for (auto i = 0uz, p = 0uz; i < FnArgGroup->Args.Len(); ++i) {
    auto const &arg = FnArgGroup->Args[i];
    const auto arg_is_void = type_predicates::IsTypeVoid(arg->InferTypeRef(sm, meta), *sm->CurrentScope);

    auto llvm_arg = arg->Stage11_CodeGen(sm, meta, ctx);
    if (arg_is_void) { continue; }
    SPP_ASSERT(llvm_arg != nullptr);

    // The parameter's type is named where the overload lives,
    // so it is qualified there and then re-resolved here. The
    // coercion compares the two types from this scope, and a
    // parameter that does not resolve from it (a generic still
    // standing in for one) is not a variant this call has to
    // widen into anyway. "Self" is the owner, as it is in the
    // declaration's own signature, else "Self or S32" widens
    // into "Var[Self, S32]" and the call mismatches it.
    const auto param_written = p < fn_params.Len()
      ? self_type::SubstituteSelf(*fn_params[p]->Type, _OverloadInfo->OverloadScope->FindEnclosingSelfType(*meta).get())
      : nullptr;
    const auto param_type_sym = param_written != nullptr
      ? _OverloadInfo->OverloadScope->FindTypeSymbol(param_written.get())
      : nullptr;
    const auto param_type = param_type_sym != nullptr ? param_type_sym->FqName() : nullptr;

    // Only a by-value parameter is ever widened. A borrowed one
    // receives a pointer to something that is already the variant,
    // so there is nothing to tag and copy.
    const auto param_is_borrow = p < fn_params.Len() and (
      fn_params[p]->To<FunctionParameterSelfAst>() != nullptr
      ? fn_params[p]->To<FunctionParameterSelfAst>()->Conv != nullptr
      : fn_params[p]->Type->GetConvention() != nullptr);

    if (param_type != nullptr and not param_is_borrow
      and sm->CurrentScope->FindTypeSymbol(param_type.get()) != nullptr) {
      llvm_arg = codegen::CoerceToFnValue(
        llvm_arg, TypeRef::Of(*param_type, *sm->CurrentScope), arg->InferTypeRef(sm, meta),
        *sm, ctx);
      llvm_arg = codegen::CoerceToVariant(
        llvm_arg, TypeRef::Of(*param_type, *sm->CurrentScope), arg->InferTypeRef(sm, meta),
        *sm->CurrentScope, "arg.variant" + uid, ctx);
      SPP_ASSERT(llvm_arg != nullptr);
    }

    // Just because a argument type is a borrow, it doesn't mean
    // that the borrow is happening here. For example, if "x" is
    // "&X", that borrow can then be moved into "fun a(y: &X)" -
    // re don't re-borrow just because it's a borrow type.
    const auto self_param = p < fn_params.Len()
      ? fn_params[p]->To<FunctionParameterSelfAst>()
      : nullptr;

    const auto param_by_value = p < fn_params.Len() and (self_param != nullptr
      ? self_param->Conv == nullptr
      : fn_params[p]->Type->GetConvention() == nullptr);

    if (param_by_value and llvm_arg->getType()->isPointerTy()) {
      const auto arg_ref = arg->InferTypeRef(sm, meta);
      if (arg_ref.IsBorrowed()) {
        if (const auto llvm_arg_type = codegen::GetLlvmTypeOf(arg_ref.WithoutConvention(), ctx);
          llvm_arg_type != nullptr) {
          llvm_arg = ctx->Builder.CreateLoad(llvm_arg_type, llvm_arg, "arg.copy" + uid);
        }
      }
    }

    // A "!" argument never exists, so the call is dead code, but
    // its operand still has to be the parameter's type. Taken from
    // the llvm function itself, which holds however the parameter
    // is written (a generic "sup" method's may not resolve here).
    if (arg->InferTypeRef(sm, meta).IsNever) {
      const auto fn_ty = llvm_func->getFunctionType();
      if (llvm_func_args.Len() < fn_ty->getNumParams()) {
        llvm_arg = llvm::PoisonValue::get(fn_ty->getParamType(static_cast<unsigned>(llvm_func_args.Len())));
      }
    }

    llvm_func_args.EmplaceBack(llvm_arg);
    ++p;
  }

  // Create the call instruction (a call returning Void cannot be given a name - llvm forbids naming void
  // values).
  if (llvm_func->getReturnType()->isVoidTy()) {
    return ctx->Builder.CreateCall(llvm_func, llvm_func_args.ToStdVector());
  }
  const auto llvm_call = ctx->Builder.CreateCall(
    llvm_func, llvm_func_args.ToStdVector(), "call" + uid);

  // A coroutine's frame may live in the caller's frame when the handle does not escape it: LLVM's "CoroElide" only
  // does that for a call marked safe to elide.
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
        _OverloadInfo->OverloadScope->FindTypeSymbol(Target()->ReturnType.get());
      const auto handle_idx = codegen::GetPhysicalFieldIndex(
        *coro_ret_type_sym->LlvmInfo, 0);
      llvm_coro_handle = ctx->Builder.CreateExtractValue(
        llvm_call, {handle_idx}, "coro.handle" + uid);
    }

    const auto llvm_gen_state = codegen::GetLlvmGeneratorStateFromHandle(llvm_coro_handle, ctx);

    if (meta->LlvmAssignmentTarget != nullptr) {
      auto generator = MakeUnique<codegen::LlvmGenerator>();
      generator->Handle = llvm_coro_handle;
      generator->State = llvm_gen_state;
      ctx->LlvmGenerators[meta->LlvmAssignmentTarget] = std::move(generator);
    }
  }

  return llvm_call;
}

auto PostfixExpressionOperatorFunctionCallAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  IMPORT_UTILS;
  //
  using generate::common_types::SelfType;
  using generate::common_types::TupleType;

  // For function folding, collect a tuple of all return types.
  if (not _FoldedAsts.IsEmpty()) {
    auto folded_return_types = _FoldedAsts
      | genex::views::transform([sm, meta](auto const &ast) { return ast->InferType(sm, meta); })
      | genex::to<Vec>();
    auto tuple_type = TupleType(0, std::move(folded_return_types));

    // Analysed as every other path's result is, so the tuple has a symbol for whoever resolves it.
    tuple_type->Stage7_AnalyseSemantics(sm, meta);
    return tuple_type;
  }

  // Get the function return type from the overload.
  auto ret_type = _OverloadInfo->Proto->ReturnType;

  // If there is a scope present (non-closure), then fully qualify the return type.
  if (_OverloadInfo->OverloadScope != nullptr and not ret_type->IsSelfType()) {
    // A return type naming the callee's "sup" block generics through a convention ("GenOnce[&V]") can miss from the
    // callee's own block, and is found from the call site, where the same generic is in view.
    auto *ret_sym = _OverloadInfo->OverloadScope->FindTypeSymbol(ret_type.get());
    if (ret_sym == nullptr) { ret_sym = sm->CurrentScope->FindTypeSymbol(ret_type.get()); }
    if (ret_sym != nullptr) { ret_type = ret_sym->FqName(); }
  }

  // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
  if (_IsCoroAndAutoResume) {
    const auto yield_type = marker_sups::GenYieldOf(marker_sups::FindGenSup(
      TypeRef::Of(*ret_type, *sm->CurrentScope), *sm->CurrentScope,
      *meta->PostfixExpressionLhs, [&] { return ret_type; }, "function call"));
    // Read off the generator's identity, shared by every spelling of it, so it points at the return type the callee
    // wrote.
    ret_type = yield_type->WithSourceSpanAt(*_OverloadInfo->Proto->ReturnType);
  }

  // "Self", alone or inside the return type ("Opt[Self]"), is what the call decided it stands for
  // ("PassedOverload::SelfType"), always as a node of its own: callers modify the type they are given, and "SelfType"
  // can be a symbol's cached name. A call through a receiver ("a.m()", "A::m()") always gets a fresh copy, recording no identity, of
  // its return type from "SubstituteSelf", "Self" or not - not "self_type::SubstituteSelf", which returns a plain
  // clone when "Self" is absent, and a clone keeps the written node's access marks, which then answer access checks
  // here as if the return type were written at this call.
  const auto pf = meta->PostfixExpressionLhs->To<PostfixExpressionAst>();
  const auto through_receiver = pf != nullptr and (
    pf->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr
    or (pf->Op->To<PostfixExpressionOperatorStaticMemberAccessAst>() != nullptr and pf->Lhs->To<TypeAst>() != nullptr));
  if (_OverloadInfo->SelfType != nullptr and ret_type->IsSelfType()) {
    ret_type = AstCloneShared(_OverloadInfo->SelfType);
  }
  else if (_OverloadInfo->SelfType != nullptr and through_receiver) {
    ret_type = ret_type->SubstituteSelf(*_OverloadInfo->SelfType);
  }

  // Generic instantiations embedded in the return type (eg "Var[Tup[Pass[Str], Fail[Utf8Err]]]" from a "Res[...]"
  // alias) are only registered in the scope of whichever module first analysed them (see CreateGnClsScope), and
  // that scope isn't guaranteed to be reachable from this call site's scope. Rather than relying on lookup finding a
  // registration made elsewhere, re-analyse a fresh clone here so the instantiation is guaranteed to exist (and be
  // reachable) from this scope too before anything downstream tries to look up its symbol.
  ret_type = AstCloneShared(ret_type);
  ret_type->Stage7_AnalyseSemantics(sm, meta);

  // Return the type.
  return ret_type;
}

auto PostfixExpressionOperatorFunctionCallAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  IMPORT_UTILS;
  // The plain case answers from the return type's own symbol, as "InferType"'s name resolves from here by its written
  // identity ("Scope::FindWrittenTypeSymbol"), without building and analysing a copy of the name every time; the call's
  // own analysis has already made sure the instantiation exists. A folded call, a coroutine auto-resumed, and a return
  // type written in terms of "Self" build the type they return, so they are inferred as one - as is any return type
  // "FindWrittenTypeSymbol" has no answer for from here.
  if (_FoldedAsts.IsEmpty() and not _IsCoroAndAutoResume and _OverloadInfo->OverloadScope != nullptr) {
    auto const &ret_type = _OverloadInfo->Proto->ReturnType;
    if (not type_predicates::DoesTypeNameSelf(*ret_type)) {
      auto *ret_sym = _OverloadInfo->OverloadScope->FindTypeSymbol(ret_type.get());
      if (ret_sym == nullptr) { ret_sym = sm->CurrentScope->FindTypeSymbol(ret_type.get()); }
      if (ret_sym != nullptr and ret_sym->Kind == TypeKind::Cls and ret_sym->Alias == nullptr
        and ret_sym->Convention == nullptr) {
        if (auto *const canon = sm->CurrentScope->FindWrittenTypeSymbol(analyse::scopes::WrittenTypeIdOf(*ret_sym));
          canon != nullptr) {
          return TypeRef::Of(*canon, *sm->CurrentScope);
        }
      }
    }
  }
  return TypeRef::Of(*InferType(sm, meta), *sm->CurrentScope);
}

auto PostfixExpressionOperatorFunctionCallAst::MarkAsAsync(
  Ast *async_token) -> void {
  _IsAsync = async_token;
}

auto PostfixExpressionOperatorFunctionCallAst::TargetScope() const -> Scope const* {
  return _OverloadInfo.has_value() ? _OverloadInfo->OverloadScope : nullptr;
}

auto PostfixExpressionOperatorFunctionCallAst::Target() const -> FunctionPrototypeAst* {
  if (not _OverloadInfo.has_value()) { return nullptr; }
  const auto target_proto = _OverloadInfo->Proto;
  if (const auto coro_proto = target_proto->To<CoroutinePrototypeAst>(); coro_proto != nullptr and coro_proto->
    IsOnce()) {
    // Not every "GenOnce" is lowered: one whose body defers
    // stays a real coroutine, because the deferred expression
    // has to outlive the yield (see "_LowerGenOnce"). Such a
    // call targets the coroutine itself.
    if (const auto lowered = coro_proto->GenOnceLowered(); lowered != nullptr) { return lowered; }
  }
  return target_proto;
}

auto PostfixExpressionOperatorFunctionCallAst::SetClosureDummyProto(
  Unique<FunctionPrototypeAst> &&proto) -> void {
  _ClosureDummyProto = std::move(proto);
}

auto PostfixExpressionOperatorFunctionCallAst::TakeClosureDummyProto() -> Unique<FunctionPrototypeAst> {
  return std::move(_ClosureDummyProto);
}

auto PostfixExpressionOperatorFunctionCallAst::SetTransformedLhs(
  Unique<PostfixExpressionAst> &&lhs) -> void {
  _TransformedLhs = std::move(lhs);
}

auto PostfixExpressionOperatorFunctionCallAst::GetTransformedLhs() const -> PostfixExpressionAst* {
  return _TransformedLhs.get();
}

auto PostfixExpressionOperatorFunctionCallAst::_HandleFnFolding(
  ScopeManager *sm, CompilerMetaData *meta) -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> {
  IMPORT_UTILS;
  // Populate the list of arguments to fold.
  auto folded_args = Vec<FunctionCallArgumentAst*>{};
  auto folded_arg_types = Vec<TypeAst*>{};
  auto folded_tup_lens = Vec<std::size_t>{};
  auto fold_indexes = Vec<std::size_t>{};
  for (auto [i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
    auto arg_type = arg->InferType(sm, meta);
    if (type_predicates::IsTypeTuple(*arg_type, *sm->CurrentScope)) {
      fold_indexes.EmplaceBack(i);
      folded_args.EmplaceBack(arg);
      folded_arg_types.EmplaceBack(arg_type.get());
      folded_tup_lens.EmplaceBack(sm->CurrentScope->FindTypeSymbol(arg_type.get())->TypeArgs().Len());
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

auto PostfixExpressionOperatorFunctionCallAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst> {
  // Handle the generic type and comp arguments that
  // take part in the function call.
  auto gn_arg_group = AstClone(GnArgGroup);
  for (auto const &gn_arg : gn_arg_group->Args) {
    if (gn_arg->IsTypeArg()) {
      gn_arg->TypeVal = gn_arg->TypeVal->ReadExprType(sub);
    }
    else if (gn_arg->IsCompArg()) {
      gn_arg->CompVal = AstClone(gn_arg->CompVal->ReadExpr(sub));
    }
  }

  // Handle the function runtime arguments too in the
  // same way.
  auto fn_arg_group = AstClone(FnArgGroup);
  for (auto const &fn_arg : fn_arg_group->Args) {
    fn_arg->Val = AstClone(fn_arg->Val->ReadExpr(sub));
  }

  // Move the substituted values into the new function
  // cast postfix operator AST.
  return MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    std::move(gn_arg_group), std::move(fn_arg_group), AstClone(Fold));
}

auto PostfixExpressionOperatorFunctionCallAst::IsAllowedInDefault() const -> bool {
  // A call is allowed when its arguments are. Folding
  // isn't allowed here (too complex right now).
  return
    FnArgGroup->IsAllowedInDefault() and Fold == nullptr;
}

SPP_MOD_END
