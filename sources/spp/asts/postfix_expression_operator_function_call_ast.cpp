module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.regions;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
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
import spp.asts.function_parameter_ast;
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
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
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
import spp.codegen.llvm_coerce;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import llvm;

namespace {
  /// The convention a parameter is declared with: a "self"
  /// parameter keeps it in "Conv", rather than in its type.
  auto ParamConvention(spp::asts::FunctionParameterAst const &param) -> spp::asts::ConventionAst const* {
    if (const auto self = param.To<spp::asts::FunctionParameterSelfAst>(); self != nullptr) { return self->Conv.get(); }
    return param.Type->GetConvention();
  }
}

SPP_MOD_BEGIN
PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
  decltype(GnArgGroup) &&generic_arg_group,
  decltype(FnArgGroup) &&arg_group,
  decltype(Fold) &&fold) :
  GnArgGroup(std::move(generic_arg_group)),
  FnArgGroup(std::move(arg_group)),
  Fold(std::move(fold)),
  _OverloadInfo(Nullopt),
  _Indirect(Nullopt),
  _CalleeArgGroup(nullptr),
  _IsAsync(nullptr),
  _IsCoroAndAutoResume(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
  Source.OriginalExpr = this;
}

PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;

auto PostfixExpressionOperatorFunctionCallAst::PosStart() const -> std::size_t {
  // Use the generic argument group.
  return not GnArgGroup->Args.empty() ? GnArgGroup->PosStart() : FnArgGroup->PosStart();
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
  ast->_OverloadInfo = _OverloadInfo;
  ast->_Indirect = _Indirect;
  ast->_CoroRetSym = _CoroRetSym;
  ast->_CalleeArgGroup = AstClone(_CalleeArgGroup);
  ast->_IsAsync = _IsAsync;
  ast->_FoldedAsts = AstCloneVec(_FoldedAsts);
  ast->_FoldArgsScope = _FoldArgsScope;
  ast->_FoldTemps = AstCloneVec(_FoldTemps);
  ast->_IsCoroAndAutoResume = _IsCoroAndAutoResume;
  return ast;
}

auto PostfixExpressionOperatorFunctionCallAst::ToString() const -> Str {
  SPP_STRING_START;
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

  // A call is resolved once. There are some instances where we
  // are getting double analysis, and re-analysing is a waste.
  // Todo: Why is this occurring?
  if (_OverloadInfo.has_value()) { return; }

  // Analyse the generic arguments, find what is called and its
  // candidates once, and analyse the function call arguments
  // against them before determining the overload.
  auto solver = Opt<overload_resolution::OverloadSolver>();
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    solver.emplace(*this, *meta->PostfixExpressionLhs, *sm, *meta);
    FnArgGroup->ExpectedTypes = solver->ExpectedArgTypes();

    // A fold's arguments are analysed in a scope of their own (see
    // "_FoldArgsScope").
    _FoldArgsScope = nullptr;
    _FoldTemps.clear();
    if (Fold != nullptr) {
      auto scope_name = analyse::scopes::ScopeBlockName::FromParts("fold-args", {}, FnArgGroup->PosStart());
      sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
      _FoldArgsScope = sm->CurrentScope;
    }
    FnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    if (Fold != nullptr) { sm->MoveOutOfCurrentScope(); }
  }

  // If we are function folding, create transformed asts.
  if (Fold != nullptr) {
    _FoldedAsts = _HandleFnFolding(sm, meta);
    for (auto const &ast : _FoldedAsts) { ast->Stage7_AnalyseSemantics(sm, meta); }
    return;
  }

  // Resolve the overload for this function call.
  auto [overload, indirect] = solver->Resolve();

  // An indirect callee is called through its function type, which
  // decides how the call holds it: a "FunRef" is borrowed, a
  // "FunMut" borrowed mutably, and a "FunMov" taken.
  if (indirect != nullptr) {
    const auto head = TypeRef::ForKindCheck(*indirect, *sm->CurrentScope);
    const auto is_fun = [&](auto const &tmpl) {
      return head.KindSymbol() != nullptr and head.IsA(*tmpl, *sm->CurrentScope);
    };
    const auto conv = is_fun(FUN_MUT) ? ConventionTag::MUT : is_fun(FUN_REF) ? ConventionTag::REF : ConventionTag::MOV;
    auto &lhs = *meta->PostfixExpressionLhs;
    const auto callee = lhs.InferTypeRef(sm, meta);
    _Indirect = _IndirectInfo{
      .FnType = indirect, .Conv = conv, .Callee = callee.Symbol,
      .FnSup = MakeShared<TypeRef>(marker_sups::FindFnSup(
        *lhs.InferType(sm, meta)->WithConvention(nullptr), *sm->CurrentScope)),
      .Borrowed = callee.IsBorrowed()};

    // A "FunMov" (that is not a "FunMut") is called by value, so
    // calling one held through a borrow ("v[i]", a "&FunMov"
    // parameter) moves it out of the borrow - the rule a method
    // taking "self" by value has (see "FunctionCallArgumentGroupAst"
    // ::Stage8_CheckMemory). The function type has its borrow
    // stripped, so the expression's own type is read.
    if (conv != ConventionTag::MUT and callee.IsBorrowed() and callee.Symbol != nullptr
      and not callee.Symbol->IsCopyable()) {
      const auto [lhs_sym, _] = sm->CurrentScope->FindVarSymbolOutermost(lhs);
      auto const &borrowed_at = lhs_sym != nullptr and lhs_sym->MemInfo->AstBorrowed != nullptr
        ? *lhs_sym->MemInfo->AstBorrowed
        : static_cast<Ast const&>(lhs);
      Raise<SppMoveFromBorrowedMemoryError>(ERR_ARGS(lhs, borrowed_at, borrowed_at));
    }

    // A borrowing call holds its callee as an argument would, with the convention its function type takes.
    if (conv != ConventionTag::MOV) {
      auto callee_arg = MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, AstClone(&lhs));
      if (conv == ConventionTag::MUT) {
        callee_arg->Conv = MakeUnique<ConventionMutAst>(nullptr, nullptr);
        callee_arg->Conv->To<ConventionMutAst>()->TokBorrow->PatchPos(lhs.PosStart());
      }
      else {
        callee_arg->Conv = MakeUnique<ConventionRefAst>(nullptr);
        callee_arg->Conv->To<ConventionRefAst>()->TokBorrow->PatchPos(lhs.PosStart());
      }
      auto callee_args = Vec<Unique<FunctionCallArgumentAst>>();
      callee_args.emplace_back(std::move(callee_arg));
      _CalleeArgGroup = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, std::move(callee_args), nullptr);
      _CalleeArgGroup->Stage7_AnalyseSemantics(sm, meta);
    }
  }

  // Set the overload to the only pass overload; a return type inferred before (a revoked resolution's) is not its.
  _Inferred = nullptr;
  _InferredIn = nullptr;
  _OverloadInfo = _OInfo{
    .OverloadScope = overload.FnScope,
    .Proto = overload.Proto,
    .SelfId = overload.SelfId,
    .HasReceiver = overload.HasReceiver
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
  for (auto i = 0uz; i < FnArgGroup->Args.size() and i < params.size(); ++i) {
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
    Raise<SppUnitTestNotCallableError>(ERR_ARGS(*this, *test_annotation));
  }

  // Check that if we are in a cmp context, that the overload
  // is also cmp.
  RaiseIf<SppInvalidCompTimeOperationError>(
    meta->EnclosingFnCmp != nullptr and _OverloadInfo->Proto->TokCmp == nullptr,
    ERR_ARGS(*this));

  // Special case for GenOnce called as a coroutine => auto
  // move into the "Yield" type.
  if (_OverloadInfo->Proto->TokFun->TokenType == lex::SppTokenType::KW_COR) {
    // This needs to be any type that EXTENDS GenOnce, not
    // just GenOnce itself.
    auto const &proto_ret_type = _OverloadInfo->Proto->ReturnType;
    _CoroRetSym = _OverloadInfo->OverloadScope->FindTypeSymbol(proto_ret_type.get());
    const auto gen = marker_sups::FindGenSup(
      TypeRef::Of(*proto_ret_type, *sm->CurrentScope), *sm->CurrentScope,
      *meta->PostfixExpressionLhs, [&] { return proto_ret_type; }, "GenOnce collapse");
    _IsCoroAndAutoResume = marker_sups::IsGenOnce(gen, *sm->CurrentScope);
  }
}

auto PostfixExpressionOperatorFunctionCallAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // If a fold is taking place, analyse the folded
  // transformations.
  if (Fold != nullptr) {
    _SkipFoldArgsScope(sm);
    for (auto const &temp : _FoldTemps) { temp->Stage8_CheckMemory(sm, meta); }
    for (auto const &ast : _FoldedAsts) { ast->Stage8_CheckMemory(sm, meta); }
    return;
  }

  // An indirect callee is used by the call: taken by a "FunMov" call, borrowed by the others.
  if (_Indirect.has_value() and _Indirect->Conv == ConventionTag::MOV) {
    // Calling a "FunMov" consumes it where it stands, so what it
    // borrows is used up with it rather than carried anywhere -
    // unlike moving the closure away, which the rule against
    // moving a holder of escaping borrows is there for.
    mem_utils::ValidateSymbolMemory(
      *meta->PostfixExpressionLhs, *this, *sm, meta, {.CheckEscapingBorrowMove = false});
  }
  else if (_CalleeArgGroup != nullptr) {
    _CalleeArgGroup->Stage8_CheckMemory(sm, meta);
  }

  // Check the generic and function arguments, the latter
  // knowing the target (whether it pins its borrows).
  GnArgGroup->Stage8_CheckMemory(sm, meta);

  const auto _meta_guard = MetaGuard(meta);
  meta->TargetCall = this;
  FnArgGroup->Stage8_CheckMemory(sm, meta);
}

/// [CLEAN] -> Move param/arg writebacks (&mut) into helper probably
auto PostfixExpressionOperatorFunctionCallAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // A call folded before its own stage 7 (a comp default folded where it is used) is resolved here, once.
  if (not _OverloadInfo.has_value()) { Stage7_AnalyseSemantics(sm, meta); }

  // Get the function prototype and resolve it.
  auto const *const fn_proto = _OverloadInfo->Proto->GetNonGnImpl();
  RaiseIf<SppCompTimeConstantError>(
    fn_proto->TokCmp == nullptr,
    ERR_ARGS(*this));

  // Todo: For now, don't allow folding in comptime.
  RaiseIf<SppCompTimeConstantError>(
    Fold != nullptr,
    ERR_ARGS(*Fold));

  // Create the argument map for the function to use: stage 7 left one keyword argument per parameter
  // ("ElaborateCall"), the receiver of a method call included. An argument given to a "&mut" parameter is a copy of
  // its value, so the place it came from is kept, to be given the value the callee leaves it with.
  auto args = Vec<Pair<Shared<IdentifierAst>, Unique<ExpressionAst>>>();
  auto mut_places = Vec<Pair<Shared<IdentifierAst>, ExpressionAst*>>();
  for (auto *arg : FnArgGroup->GetAllArgs()) {
    auto name = arg->As<FunctionCallArgumentKeywordAst>()->Name;
    const auto params = fn_proto->FnParamGroup->GetAllParams();
    const auto param = genex::find_if(params, [&](auto const *p) {
      const auto param_name = p->ExtractName();
      return param_name != nullptr and *param_name == *name;
    });
    const auto conv = param != params.end() ? ParamConvention(**param) : nullptr;
    if (conv != nullptr and conv->To<ConventionMutAst>() != nullptr) {
      mut_places.emplace_back(name, arg->Val.get());
    }
    arg->Stage9_CompTimeResolve(sm, meta);
    args.emplace_back(std::move(name), std::move(meta->CompTimeResult));
  }
  auto fn_arg_map = decltype(meta->CompTimeArgs)();
  auto gn_arg_type_map = decltype(meta->CompTimeGnTypeArgs)();
  auto gn_arg_comp_map = decltype(meta->CompTimeGnCompArgs)();
  auto gn_comp_values = decltype(meta->CompTimeGnCompValues)();
  for (auto &&[name, val] : args) { fn_arg_map[name] = std::move(val); }
  for (auto &&gn_arg : GnArgGroup->GetTypeArgs()) {
    gn_arg_type_map.emplace_back(MakeShared<TypeRef>(
      TypeRef::Of(*gn_arg->TypeVal, *sm->CurrentScope)));
  }

  // What "Self" stands for at this call goes after the call's own
  // type arguments: a builtin with none of its own reads the type
  // it was called on from it ("S32::zero()" is an "S32").
  if (_OverloadInfo->SelfId != nullptr) {
    gn_arg_type_map.emplace_back(MakeShared<TypeRef>(TypeRef::Of(_OverloadInfo->SelfId, *sm->CurrentScope)));
  }

  // Each comp argument by the parameter it binds: its keyword,
  // or, given by position, the comp parameter in the same
  // place (one for one only: a variadic parameter takes several).
  const auto comp_args = GnArgGroup->GetCompArgs();
  const auto comp_params = fn_proto->GnParamGroup->GetCompParams();
  for (auto i = 0uz; i < comp_args.size(); ++i) {
    gn_arg_comp_map.emplace_back(sm->CurrentScope->CompIdOf(*comp_args[i]->CompVal));
    auto name = comp_args[i]->CompNameAsId();
    if (name == nullptr and comp_args.size() == comp_params.size()) {
      name = IdentifierAst::FromType(*comp_params[i]->Name);
    }
    if (name != nullptr) { gn_comp_values.emplace_back(std::move(name), gn_arg_comp_map.back()); }
  }

  // Resolve the function with the arguments. The first call to
  // be folded is the one the user wrote, and the calls it makes
  // report their errors there rather than inside std.
  const auto owner = Source.OriginalExpr != nullptr ? Source.OriginalExpr : static_cast<Ast*>(this);
  const auto *const outer_site = meta->CompTimeCallSite;

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
    ERR_ARGS(outer_site != nullptr ? *outer_site : *owner));
  {
    const auto _meta_guard = MetaGuard(meta);
    if (outer_site == nullptr) { meta->CompTimeCallSite = owner; }
    meta->CompTimeArgs = std::move(fn_arg_map);
    meta->CompTimeGnTypeArgs = std::move(gn_arg_type_map);
    meta->CompTimeGnCompArgs = std::move(gn_arg_comp_map);
    meta->CompTimeGnCompValues = std::move(gn_comp_values);
    auto *const fn_scope = fn_proto->GetAstScope();
    auto tm = sm->NewManagerAt(not fn_scope->Children.empty() ? fn_scope->Children[0].get() : fn_scope);
    fn_proto->Impl->Stage9_CompTimeResolve(&tm, meta);

    // Each "&mut" argument's place takes the value the callee left
    // its parameter with, as an assignment to it would.
    for (auto const &[name, place] : mut_places) {
      auto value = meta->CompTimeArgs.contains(name) ? std::move(meta->CompTimeArgs[name]) : nullptr;
      if (value == nullptr) { continue; }
      const auto place_sym = sm->CurrentScope->FindVarSymbolOutermost(*place).first;
      if (place->To<IdentifierAst>() != nullptr) {
        place_sym->CompTimeValue = std::move(value);
      }
      else if (regions::IsAttr(place, sm)) {
        comp_time_intrinsics::SetCompTimeAttrValue(
          place_sym->CompTimeValue->To<ObjectInitializerAst>(), place, std::move(value), sm);
      }
      else {
        Raise<SppInvalidCompTimeOperationError>(ERR_ARGS(*place));
      }
    }
  }

  // Every function reaches comp-time resolution through here,
  // so this is where an integer result is checked against what
  // its type can hold. Comp-time arithmetic is exact, so a
  // result that does not fit arrives intact rather than having
  // wrapped on the way out. Checked per call to catch overflow.
  auto const &site = outer_site != nullptr ? *outer_site : *owner;
  if (const auto int_result =
    meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<IntegerLiteralAst>() : nullptr) {
    int_result->ValidateBounds(site);
  }
  else if (const auto flt_result =
    meta->CompTimeResult != nullptr ? meta->CompTimeResult->To<FloatLiteralAst>() : nullptr) {
    flt_result->ValidateBounds(site);
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
    _SkipFoldArgsScope(sm);
    for (auto const &temp : _FoldTemps) { temp->Stage11_CodeGen(sm, meta, ctx); }
    auto results = Vec<llvm::Value*>();
    for (auto const &ast : _FoldedAsts) {
      auto pf = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), AstClone(ast));
      results.emplace_back(pf->Stage11_CodeGen(sm, meta, ctx));
    }

    const auto tuple_sym = InferTypeRef(sm, meta).Symbol;
    const auto tuple_llvm_type = tuple_sym != nullptr ? codegen::GetLlvmType(*tuple_sym, ctx) : nullptr;
    if (tuple_llvm_type == nullptr or not tuple_llvm_type->isStructTy()) { return nullptr; }
    auto tuple_val = static_cast<llvm::Value*>(llvm::PoisonValue::get(tuple_llvm_type));
    for (auto i = 0uz; i < results.size(); ++i) {
      if (results[i] == nullptr or codegen::IsValuelessType(results[i]->getType())) { continue; }
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
  if (_Indirect.has_value()) {
    const auto closure_uid = "." + Uid();
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    const auto closure_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);

    // The lhs' static type determines the physical field indices
    // of "{ fn_ptr, env_ptr }": a plain "FunXXX" has no extra
    // fields, but a class that superimposes one may declare its
    // own attributes too, and the "spp" layout can reorder any of
    // them - "GetPhysicalFieldIndex" maps back from the fixed
    // declared prefix (0, 1) to wherever they actually ended up.
    const auto lhs_type_sym = _Indirect->Callee;
    const auto fn_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 0);
    const auto env_ptr_idx = codegen::GetPhysicalFieldIndex(*lhs_type_sym->LlvmInfo, 1);

    // The lhs is the { fn_ptr, env_ptr } value directly, or for
    // a borrowed closure, a pointer to it, so read the fields
    // accordingly.
    auto fn_ptr = static_cast<llvm::Value*>(nullptr);
    auto env_ptr = static_cast<llvm::Value*>(nullptr);
    if (_Indirect->Borrowed) {
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
    closure_args.insert(closure_args.begin(), env_ptr);

    // Reconstruct the closure's function type
    // ("(env*, ...params) -> ret") to call through the pointer.
    auto closure_param_tys = closure_args
      | genex::views::transform([](auto const &v) { return v->getType(); })
      | genex::to<Vec>();

    // The value returns what its own function type's "Out" is, which can be narrower than what the call is typed as:
    // called through a constraint ("F: FunMov[(T,), Opt[U]]"), a closure answering "Some[U]" is widened into the
    // "Opt[U]" asked for. So it is read off the value's type here, not the type the call resolved through.
    const auto expected_ret_ref = InferTypeRef(sm, meta);
    auto actual_ret_ref = expected_ret_ref;
    auto const &fn_sup = *_Indirect->FnSup;
    if (fn_sup.Symbol != nullptr) {
      if (const auto out = fn_sup.Symbol->TypeArgRef("Out"); out.Symbol != nullptr) { actual_ret_ref = out; }
    }

    const auto closure_ret_ty = codegen::GetLlvmTypeOf(actual_ret_ref, ctx);
    const auto closure_fn_ty = llvm::FunctionType::get(
      closure_ret_ty, closure_param_tys, false);

    // Calling a "FunMov" held by value consumes it, so nothing
    // drops it afterwards: its environment is released here, as
    // "FunMov::drop" would. Whether there is storage to free is
    // read from the environment itself: a "FunRef" passed as a
    // "FunMov" still has its environment on the caller's stack.
    const auto consumes_closure = not _Indirect->Borrowed and _Indirect->Conv == ConventionTag::MOV;
    const auto release_env = [&] {
      if (not consumes_closure or ctx->Builder.GetInsertBlock()->hasTerminator()) { return; }
      codegen::EmitReleaseClosureEnv(env_ptr, ctx);
    };

    // A call returning Void cannot be given a name (llvm forbids
    // naming void values).
    if (closure_ret_ty->isVoidTy()) {
      const auto void_call = ctx->Builder.CreateCall(
        closure_fn_ty, fn_ptr, closure_args,
        codegen::CfiFnCallBundles(fn_sup, ctx));
      release_env();
      return void_call;
    }

    // Build the proper (named) call and then do the variant
    // coercion.
    const auto closure_call = ctx->Builder.CreateCall(
      closure_fn_ty, fn_ptr, closure_args,
      codegen::CfiFnCallBundles(fn_sup, ctx),
      "closure.call" + closure_uid);
    release_env();
    return codegen::CoerceToSlot(
      closure_call, expected_ret_ref, actual_ret_ref, *sm, "closure.ret" + closure_uid, ctx);
  }

  // Coroutine calls: calling a coroutine does not run its body,
  // it constructs a generator. The frame is owned by the llvm
  // coroutine intrinsics, and the value handed back is nothing
  // but the "llvm.coro.begin" handle.
  const auto is_coroutine_call = Target()->IsCoroutine();

  // For generically converted function prototypes, generate
  // their llvm declaration in-walk if it is still missing.
  if (Target()->GetLlvmFn() == nullptr) {
    auto tm = sm->NewManagerAt(const_cast<Scope*>(_OverloadInfo->OverloadScope));
    const auto owner_ctx = Target()->OwnerCtx();
    Target()->GenerateLlvmDeclaration(
      &tm, meta, owner_ctx != nullptr ? owner_ctx : ctx);
  }

  const auto uid = "." + Uid();
  const auto o = "Call target has no llvm declaration: " + Target()->PrintSignature("");
  if (Target()->GetLlvmFn() == nullptr) { Ice(*this, *sm->CurrentScope, o); }

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

  // "Self" in a parameter's type is the owner, as it is in the declaration's own signature, else "Self or S32" widens
  // into "Var[Self, S32]" and the call mismatches it.
  auto const *const fn_scope = _OverloadInfo->OverloadScope;
  const auto owner = fn_scope->FindEnclosingSelfType(*meta);
  const auto owner_id = owner != nullptr ? fn_scope->TypeIdOf(*owner) : nullptr;
  auto llvm_func_args = Vec<llvm::Value*>();
  llvm_func_args.reserve(FnArgGroup->Args.size());

  for (auto i = 0uz, p = 0uz; i < FnArgGroup->Args.size(); ++i) {
    auto const &arg = FnArgGroup->Args[i];
    const auto arg_is_void = type_predicates::IsTypeVoid(arg->InferTypeRef(sm, meta), *sm->CurrentScope);

    auto llvm_arg = arg->Stage11_CodeGen(sm, meta, ctx);
    if (arg_is_void) { continue; }
    SPP_ASSERT(llvm_arg != nullptr);

    // The parameter's type by identity, keyed where the overload
    // lives and read from this scope, where the coercion compares
    // the two types. One with nothing filed under it here (a
    // generic still standing in for one) is not a variant this
    // call has to widen into anyway.
    const auto param_ref = p < fn_params.size()
      ? TypeRef::Of(
        analyse::scopes::SubstituteSelfId(fn_scope->TypeIdOf(*fn_params[p]->Type), owner_id), *sm->CurrentScope)
      : TypeRef();

    // Only a by-value parameter is ever widened. A borrowed one
    // receives a pointer to something that is already the variant,
    // so there is nothing to tag and copy.
    const auto param_is_borrow = p < fn_params.size() and ParamConvention(*fn_params[p]) != nullptr;

    if (param_ref.Symbol != nullptr and not param_is_borrow) {
      llvm_arg = codegen::CoerceToSlot(llvm_arg, param_ref, arg->InferTypeRef(sm, meta), *sm, "arg.variant" + uid, ctx);
      SPP_ASSERT(llvm_arg != nullptr);
    }

    // Just because a argument type is a borrow, it doesn't mean
    // that the borrow is happening here. For example, if "x" is
    // "&X", that borrow can then be moved into "fun a(y: &X)" -
    // re don't re-borrow just because it's a borrow type.
    const auto self_param = p < fn_params.size()
      ? fn_params[p]->To<FunctionParameterSelfAst>()
      : nullptr;

    const auto param_by_value = p < fn_params.size() and (self_param != nullptr
      ? self_param->Conv == nullptr
      : fn_params[p]->Type->GetConvention() == nullptr);

    if (const auto arg_ref = arg->InferTypeRef(sm, meta); param_by_value and arg_ref.IsBorrowed()) {
      if (const auto llvm_arg_type = codegen::GetLlvmTypeOf(arg_ref.WithoutConvention(), ctx);
        llvm_arg_type != nullptr) {
        llvm_arg = ctx->Builder.CreateLoad(llvm_arg_type, llvm_arg, "arg.copy" + uid);
      }
    }

    // A "!" argument never exists, so the call is dead code, but
    // its operand still has to be the parameter's type. Taken from
    // the llvm function itself, which holds however the parameter
    // is written (a generic "sup" method's may not resolve here).
    if (arg->InferTypeRef(sm, meta).IsNever) {
      const auto fn_ty = llvm_func->getFunctionType();
      if (llvm_func_args.size() < fn_ty->getNumParams()) {
        llvm_arg = llvm::PoisonValue::get(fn_ty->getParamType(static_cast<unsigned>(llvm_func_args.size())));
      }
    }

    llvm_func_args.emplace_back(llvm_arg);
    ++p;
  }

  // Create the call instruction (a call returning Void cannot be given a name - llvm forbids naming void
  // values).
  if (llvm_func->getReturnType()->isVoidTy()) {
    return ctx->Builder.CreateCall(llvm_func, llvm_func_args);
  }
  const auto llvm_call = ctx->Builder.CreateCall(
    llvm_func, llvm_func_args, "call" + uid);

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
    // "Gen"/"GenOnce" lower to the bare handle; a class superimposing one carries it in its first field.
    auto llvm_coro_handle = static_cast<llvm::Value*>(llvm_call);
    if (not type_predicates::IsTypeGenerator(
      TypeRef::ForKindCheck(*_CoroRetSym, *sm->CurrentScope), *sm->CurrentScope)) {
      const auto handle_idx = codegen::GetPhysicalFieldIndex(*_CoroRetSym->LlvmInfo, 0);
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
  using generate::common_types::TupleType;

  // For function folding, collect a tuple of all return types.
  if (not _FoldedAsts.empty()) {
    auto folded_return_types = _FoldedAsts
      | genex::views::transform([sm, meta](auto const &ast) { return ast->InferType(sm, meta); })
      | genex::to<Vec>();
    auto tuple_type = TupleType(0, std::move(folded_return_types));

    // Analysed as every other path's result is, so the tuple has a symbol for whoever resolves it.
    tuple_type->Stage7_AnalyseSemantics(sm, meta);
    return tuple_type;
  }

  // Built and analysed once per scope it is spelled in, and handed out as a node of its own: callers modify the type
  // they are given.
  if (_Inferred != nullptr and _InferredIn == sm->CurrentScope) { return AstCloneShared(_Inferred); }

  // Get the function return type from the overload.
  auto const &proto = *_OverloadInfo->Proto;
  auto ret_type = proto.ReturnType;

  // An instantiation whose arguments name a parameter it binds returns by identity, as does one naming "Self", keyed
  // where the overload is; "Self" is what the call decided it stands for ("PassedOverload::SelfId"). Spelled where the
  // call is, whose parameters those are.
  const auto ret_id = analyse::utils::self_type::TypeIdWithSelf(
    proto.InstanceReturnTypeId, *ret_type, _OverloadInfo->OverloadScope, _OverloadInfo->SelfId);
  if (ret_id != nullptr) {
    if (auto spelled = sm->CurrentScope->TypeAstOf(ret_id); spelled != nullptr) {
      ret_type = std::move(spelled);
    }
  }

  // Otherwise, if there is a scope present (non-closure),
  // then fully qualify the return type.
  else if (_OverloadInfo->OverloadScope != nullptr and not ret_type->IsSelfType()) {
    auto ret_sym = _OverloadInfo->OverloadScope->FindTypeSymbol(ret_type.get());
    if (ret_sym == nullptr) { ret_sym = sm->CurrentScope->FindTypeSymbol(ret_type.get()); }
    if (ret_sym != nullptr) { ret_type = ret_sym->FqName(); }
  }

  // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
  if (_IsCoroAndAutoResume) {
    const auto yield_ref = marker_sups::GenYieldOf(marker_sups::FindGenSup(
      TypeRef::Of(*ret_type, *sm->CurrentScope), *sm->CurrentScope,
      *meta->PostfixExpressionLhs, [&] { return ret_type; }, "function call"));
    // Read off the generator's identity, shared by every spelling of it, so it points at the return type the callee
    // wrote.
    ret_type = yield_ref.AstIn(*sm->CurrentScope)->WithSourceSpanAt(*proto.ReturnType);
  }

  // Generic instantiations embedded in the return type (eg "Var[Tup[Pass[Str], Fail[Utf8Err]]]" from a "Res[...]"
  // alias) are only registered in the scope of whichever module first analysed them (see FindOrMakeClsInstance), and
  // that scope isn't guaranteed to be reachable from this call site's scope. Rather than relying on lookup finding a
  // registration made elsewhere, re-analyse a fresh clone here so the instantiation is guaranteed to exist (and be
  // reachable) from this scope too before anything downstream tries to look up its symbol.
  ret_type = AstCloneShared(ret_type);
  ret_type->Stage7_AnalyseSemantics(sm, meta);
  _Inferred = ret_type;
  _InferredIn = sm->CurrentScope;
  return AstCloneShared(ret_type);
}

auto PostfixExpressionOperatorFunctionCallAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  IMPORT_UTILS;
  // The plain case answers from the return type's identity read once where the call is ("Scope::ReadIn"), as
  // "InferType"'s name resolves from here, without building and analysing a copy of the name every time; the call's
  // own analysis has already made sure the instantiation exists. A folded call, a coroutine auto-resumed, and a return
  // type written in terms of "Self" build the type they return, so they are inferred as one - as is any return type
  // whose instantiation is not filed under that identity here.
  if (_FoldedAsts.empty() and not _IsCoroAndAutoResume and _OverloadInfo->OverloadScope != nullptr) {
    // An instantiation's return type by identity, "Self" what the call decided.
    if (const auto ret_id = analyse::scopes::SubstituteSelfId(
        _OverloadInfo->Proto->InstanceReturnTypeId, _OverloadInfo->SelfId);
      ret_id != nullptr) {
      if (not ret_id->HasSelf) {
        if (const auto read = TypeRef::Of(ret_id, *sm->CurrentScope); read.Symbol != nullptr) {
          return read;
        }
      }
      return TypeRef::Of(*InferType(sm, meta), *sm->CurrentScope);
    }
    auto const &ret_type = _OverloadInfo->Proto->ReturnType;
    if (not type_predicates::DoesTypeNameSelf(*ret_type)) {
      auto *ret_sym = _OverloadInfo->OverloadScope->FindTypeSymbol(ret_type.get());
      if (ret_sym == nullptr) { ret_sym = sm->CurrentScope->FindTypeSymbol(ret_type.get()); }
      if (ret_sym != nullptr and ret_sym->Kind == TypeKind::Cls and ret_sym->Alias == nullptr
        and ret_sym->Convention == ConventionTag::MOV) {
        // Read in once, and no symbol when nothing is filed under it here (not the callee's, "Open" would keep).
        const auto read = TypeRef::Of(*ret_sym, *sm->CurrentScope, Nullopt, TypeRef::OnMissing::Null);
        if (read.Symbol != nullptr) { return read; }
      }
    }
  }
  return TypeRef::Of(*InferType(sm, meta), *sm->CurrentScope);
}

auto PostfixExpressionOperatorFunctionCallAst::MarkAsAsync(
  Ast *async_token) -> void {
  _IsAsync = async_token;
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

auto PostfixExpressionOperatorFunctionCallAst::HasReceiver() const -> bool {
  return _OverloadInfo.has_value() and _OverloadInfo->HasReceiver;
}

auto PostfixExpressionOperatorFunctionCallAst::IsReceiverArg(
  FunctionCallArgumentAst const &arg) const -> bool {
  return HasReceiver() and not FnArgGroup->Args.empty() and FnArgGroup->Args.front().get() == &arg;
}

auto PostfixExpressionOperatorFunctionCallAst::ResolvedProto() const -> FunctionPrototypeAst* {
  return _OverloadInfo.has_value() ? _OverloadInfo->Proto : nullptr;
}

auto PostfixExpressionOperatorFunctionCallAst::IsAsync() const -> bool {
  return _IsAsync != nullptr;
}

auto PostfixExpressionOperatorFunctionCallAst::_SkipFoldArgsScope(
  ScopeManager *sm) const -> void {
  if (_FoldArgsScope == nullptr) { return; }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _FoldArgsScope);
  ignore = sm->SkipSubtree(sm->CurrentScope);
  sm->MoveOutOfCurrentScope();
}

auto PostfixExpressionOperatorFunctionCallAst::_HandleFnFolding(
  ScopeManager *sm, CompilerMetaData *meta) -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> {
  IMPORT_UTILS;
  // Populate the list of arguments to fold.
  auto folded_args = Vec<FunctionCallArgumentAst*>{};
  auto folded_arg_types = Vec<TypeAst*>{};
  auto folded_tup_lens = Vec<std::size_t>{};
  auto fold_indexes = Vec<std::size_t>{};
  auto args_sm = sm->NewManagerAt(_FoldArgsScope);
  for (auto [i, arg] : FnArgGroup->GetAllArgs() | genex::views::enumerate) {
    auto arg_type = arg->InferType(&args_sm, meta);
    if (type_predicates::IsTypeTuple(TypeRef::ForKindCheck(*arg_type, *sm->CurrentScope), *sm->CurrentScope)) {
      fold_indexes.emplace_back(i);
      folded_args.emplace_back(arg);
      folded_arg_types.emplace_back(arg_type.get());
      folded_tup_lens.emplace_back(TypeRef::Of(*arg_type, *sm->CurrentScope).Symbol->TypeArgRefs().size());
    }
  }

  // A folded tuple that is not a name is evaluated once, into a
  // temporary, and each folded call reads its element off that.
  auto folded_names = Vec<Shared<IdentifierAst>>{};
  for (const auto arg : folded_args) {
    if (const auto name = arg->Val->To<IdentifierAst>(); name != nullptr) {
      folded_names.emplace_back(AstCloneShared(name));
      continue;
    }
    auto temp_name = MakeShared<IdentifierAst>(arg->Val->PosStart(), "$fold." + spp::utils::Uid());
    auto temp_var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, temp_name, nullptr);
    auto temp_let = MakeUnique<LetStatementInitializedAst>(
      nullptr, std::move(temp_var), nullptr, nullptr, AstClone(arg->Val));
    temp_let->Stage7_AnalyseSemantics(sm, meta);
    _FoldTemps.emplace_back(std::move(temp_let));
    folded_names.emplace_back(std::move(temp_name));
  }

  // Build the unrolled AST transformations.
  const auto smallest_tuple = genex::min_element(folded_tup_lens);
  auto transformed_asts = Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>{};
  for (auto i = 0uz; i < smallest_tuple; ++i) {
    auto new_arg_group = AstClone(FnArgGroup);
    for (auto j = 0uz; j < fold_indexes.size(); ++j) {
      // Create the postfix access into the tuple.
      const auto fold_index = fold_indexes[j];
      auto id = MakeUnique<IdentifierAst>(new_arg_group->Args[fold_index]->Val->PosEnd(), std::to_string(i));
      auto ma = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(id));
      auto pf = MakeUnique<PostfixExpressionAst>(AstClone(folded_names[j]), std::move(ma));
      new_arg_group->Args[fold_index]->Val = std::move(pf);
    }

    auto transformed_ast = AstClone(this);
    transformed_ast->FnArgGroup = std::move(new_arg_group);
    transformed_ast->Fold = nullptr;
    transformed_ast->_FoldArgsScope = nullptr;
    transformed_ast->_FoldTemps.clear();
    transformed_asts.emplace_back(std::move(transformed_ast));
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
      gn_arg->TypeVal = analyse::utils::type_resolution::ReadType(*gn_arg->TypeVal, sub);
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

auto PostfixExpressionOperatorFunctionCallAst::WalksOperand() const -> bool {
  // A method call checks its receiver as its "self" argument.
  return HasReceiver();
}

SPP_MOD_END
