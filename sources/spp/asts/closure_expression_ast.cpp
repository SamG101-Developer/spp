module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.closure_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_func;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;

SPP_MOD_BEGIN
spp::asts::ClosureExpressionAst::ClosureExpressionAst(
  decltype(Tok) &&tok,
  decltype(PcGroup) &&pc_group,
  decltype(TokArrow) &&tok_arrow,
  decltype(ReturnType) return_type,
  decltype(Body) &&body) :
  Tok(std::move(tok)),
  PcGroup(std::move(pc_group)),
  TokArrow(std::move(tok_arrow)),
  ReturnType(std::move(return_type)),
  Body(std::move(body)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Tok, lex::SppTokenType::KW_FUN, "fun");
  Source._OriginalRetType = nullptr;
  _TrueRetType = nullptr;
}

spp::asts::ClosureExpressionAst::~ClosureExpressionAst() = default;

auto spp::asts::ClosureExpressionAst::PosStart() const
  -> std::size_t {
  // Use the "cor" token if present, otherwise the pc group.
  return Tok ? Tok->PosStart() : PcGroup->PosStart();
}

auto spp::asts::ClosureExpressionAst::PosEnd() const
  -> std::size_t {
  // Use the body.
  return Body->PosEnd();
}

auto spp::asts::ClosureExpressionAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<ClosureExpressionAst>(
    AstClone(Tok),
    AstClone(PcGroup),
    AstClone(TokArrow),
    AstCloneShared(ReturnType),
    AstClone(Body));
  c->_TrueRetType = _TrueRetType;
  c->_MockType = _MockType;
  return c;
}

auto spp::asts::ClosureExpressionAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Tok).append(" ");
  SPP_STRING_APPEND(PcGroup).append(" ");
  SPP_STRING_APPEND(TokArrow).append(TokArrow ? " " : "");
  SPP_STRING_APPEND(ReturnType).append(ReturnType ? " " : "");
  SPP_STRING_APPEND(Body);
  SPP_STRING_END;
}

auto spp::asts::ClosureExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::type_predicates::IsTypeBorrowed;
  using analyse::errors::SppSecondClassBorrowViolationError;

  // Save the current scope for later resetting.
  const auto parent_scope = sm->CurrentScope;
  {
    const auto _meta_guard = meta::MetaGuard(meta, true);
    meta->OverriddenScopeForClosure = parent_scope;
    PcGroup->Stage7_AnalyseSemantics(sm, meta);

    const auto inherited_type_generics = sm->CurrentScope->AllTypeSymbols()
      | genex::views::filter([](auto const &sym) { return sym->IsGeneric; })
      | genex::to<Vec>();

    const auto inherited_comp_generics = sm->CurrentScope->AllVarSymbols()
      | genex::views::filter([](auto const &sym) { return sym->IsGeneric; })
      | genex::to<Vec>();

    // Update the meta args with the closure information for
    // body analysis. The closure-wide save/restore allows for
    // the "ret" to match the closure's inferred return type.
    meta->Save();
    meta->EnclosingFunctionScope = sm->CurrentScope; // this will be the closure-outer scope
    sm->CurrentScope->Parent = sm->CurrentScope->ParentModule();
    analyse::scopes::BumpScopeLinkageGeneration();

    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
      "closure-inner", {}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    meta->EnclosingFunctionFlavour = Tok.get();
    meta->EnclosingFunctionRetType = {};
    meta->EnclosingFunctionSourceRetType = {};

    // A declared return type is seeded here, so that a "ret"
    // in the body is checked against it and coerced into it -
    // the same path a subroutine's body takes.
    if (ReturnType != nullptr) {
      ReturnType->Stage7_AnalyseSemantics(sm, meta);
      meta->EnclosingFunctionRetType.EmplaceBack(ReturnType);
      meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    }

    // A "ret" or "?" in the body leaves the closure rather
    // than the function the closure is written in, so a
    // closure written inside a deferred expression is past
    // the point that restriction applies to.
    meta->WithinDeferTok = nullptr;

    // Add the inherited generics into the closure-inner scope.
    for (auto const &type_generic_sym : inherited_type_generics) {
      sm->CurrentScope->AddTypeSymbol(type_generic_sym->SharedFromThis<analyse::scopes::TypeSymbol>());
    }
    for (auto const &comp_generic_sym : inherited_comp_generics) {
      sm->CurrentScope->AddVarSymbol(comp_generic_sym->SharedFromThis<analyse::scopes::VariableSymbol>());
    }

    // Analyse the body of the closure.
    Body->Stage7_AnalyseSemantics(sm, meta);
    _TrueRetType = not meta->EnclosingFunctionRetType.IsEmpty()
      ? meta->EnclosingFunctionRetType[0]
      : Body->InferType(sm, meta);
    _TrueRetType->Stage7_AnalyseSemantics(sm, meta);
    Source._OriginalRetType = _TrueRetType;

    // The return type is inferred rather than declared, so it
    // never passes through the function prototype's return type
    // borrow check.
    RaiseIf<SppSecondClassBorrowViolationError>(
      Tok->TokenType == lex::SppTokenType::KW_FUN and IsTypeBorrowed(*_TrueRetType, *sm),
      {sm->CurrentScope}, ERR_ARGS(*this, *_TrueRetType, "function return type"));
  }
  meta->Restore();

  // Set the scope back.
  sm->CurrentScope = parent_scope;

  // Minted last, because the functional type it superimposes is
  // not known until the body has given up its return type.
  _MockType = _MakeMockType(sm, meta);
}

auto spp::asts::ClosureExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Save the current scope for later resetting.
  const auto parent_scope = sm->CurrentScope;
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    PcGroup->Stage8_CheckMemory(sm, meta);

    // Prevent the body inheriting external assignments.
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;

    // Check the memory of the body of the closure. A "ret" inside it
    // leaves the closure, not the function the closure is written in,
    // so the linearity walk has to stop here.
    sm->MoveToNextScope();
    meta->EnclosingFunctionScope = sm->CurrentScope;
    Body->Stage8_CheckMemory(sm, meta);

    // Set the scope back.
  }
  sm->CurrentScope = parent_scope;
}

auto spp::asts::ClosureExpressionAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Strategy: build an "environment" struct for the closure,
  // with fields for captures. The safety is already guaranteed
  // by semantic analysis.
  // Todo: Add LLVM attributes to pointer types for optimizations (unique, nonnull, etc).
  const auto uid = "." + spp::utils::Uid(this);
  const auto closure_env_ty = llvm::StructType::create(
    *ctx->Context, "closure.env_type." + uid);
  auto closure_env_field_tys = Vec<llvm::Type*>{};

  // For each capture, determine its type symbol and add it to
  // the environment's fields list. This uses a "C" layout as it
  // is just done in order, no index map. TODO: spp layout.
  for (auto const &capture : PcGroup->CaptureGroup->Captures) {
    const auto cap_ty = capture->InferType(sm, meta);
    const auto cap_ty_sym = sm->CurrentScope->GetTypeSymbol(cap_ty.get());
    closure_env_field_tys.EmplaceBack(codegen::GetLlvmType(*cap_ty_sym, ctx));
  }
  closure_env_ty->setBody(closure_env_field_tys.ToStdVector(), false);

  // Build a new function that the body of the closure is built into.
  // It needs a variable binding at the top (ie allow "let a = env.a";
  // function sig is "(env: $ClosureX, ...params: Params) -> RetType").
  auto llvm_param_types = PcGroup->ParamGroup->GetAllParams()
    | genex::views::transform([&](auto const &param) {
      return codegen::GetLlvmTypeOf(*param->Type, *sm->CurrentScope, ctx);
    })
    | genex::to<Vec>();
  llvm_param_types.Insert(llvm_param_types.begin(), llvm::PointerType::get(*ctx->Context, 0));
  const auto llvm_ret_ty = codegen::GetLlvmTypeOf(*_TrueRetType, *sm->CurrentScope, ctx);

  const auto llvm_fn_ty = llvm::FunctionType::get(
    llvm_ret_ty, llvm_param_types.ToStdVector(), PcGroup->ParamGroup->GetVariadicParams() != nullptr);

  // The closure body has internal linkage, so it cannot be declared
  // into a second module the way an external symbol can - it has to
  // be created in the module that takes its address, which is the
  // one the enclosing function belongs to rather than "ctx->Module".
  const auto llvm_fn = llvm::Function::Create(
    llvm_fn_ty, llvm::Function::InternalLinkage,
    "closure.fn." + uid, codegen::GetEmissionModule(*ctx));

  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry", llvm_fn);

  const auto saved_bb = ctx->Builder.GetInsertBlock();
  const auto saved_fn_scope = meta->EnclosingFunctionScope;
  const auto saved_ret_ty = meta->EnclosingFunctionRetType;
  const auto saved_src_ret_ty = meta->EnclosingFunctionSourceRetType;
  const auto saved_flavour = meta->EnclosingFunctionFlavour;
  const auto saved_current_closure_type = ctx->CurrentClosureType;

  ctx->Builder.SetInsertPoint(entry_bb);
  sm->CurrentScope->AstNode = this;
  _LlvmFunc = MakeShared<codegen::LlvmFuncWrapper>(llvm_fn);
  meta->EnclosingFunctionScope = sm->CurrentScope;
  meta->EnclosingFunctionRetType = {_TrueRetType};
  meta->EnclosingFunctionSourceRetType = {Source._OriginalRetType};
  meta->EnclosingFunctionFlavour = Tok.get();
  ctx->CurrentClosureType = closure_env_ty;
  ctx->CurrentClosureScope = sm->CurrentScope;

  // For now, just skip scopes and return a nullptr.
  const auto parent_scope = sm->CurrentScope;
  {
    const auto _meta_guard = meta::MetaGuard(meta);

    // Copy stage 8 meta reset changes to prevent leakage between
    // info from outside the closure and inside the closure.
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    meta->LlvmAssignmentTarget = nullptr;
    meta->LlvmAssignmentTargetType = nullptr;

    PcGroup->Stage11_CodeGen(sm, meta, ctx);
    sm->MoveToNextScope();
    const auto body_val = Body->Stage11_CodeGen(sm, meta, ctx);

    // Terminate the closure function with a return of the body's
    // value (closures return their body implicitly).
    if (not ctx->Builder.GetInsertBlock()->hasTerminator()) {
      if (llvm_ret_ty->isVoidTy()) { ctx->Builder.CreateRetVoid(); }
      else if (body_val != nullptr) { ctx->Builder.CreateRet(body_val); }
      else { ctx->Builder.CreateUnreachable(); }
    }
  }
  sm->CurrentScope = parent_scope;

  // Restore the previous context.
  ctx->Builder.SetInsertPoint(saved_bb);
  meta->EnclosingFunctionScope = saved_fn_scope;
  meta->EnclosingFunctionRetType = saved_ret_ty;
  meta->EnclosingFunctionSourceRetType = saved_src_ret_ty;
  meta->EnclosingFunctionFlavour = saved_flavour;
  ctx->CurrentClosureType = saved_current_closure_type;

  // Todo: Manage the moved captures' destruction properly,
  // currently they all leak. The env pointer is freed, but
  // not its contents. Check this more carefully.
  const auto env_outlives_frame = genex::any_of(
    PcGroup->CaptureGroup->Captures,
    [](auto const &c) { return c->Conv == nullptr or *c->Conv == ConventionTag::MUT; });

  const auto env_alloca = [&]() -> llvm::Value* {
    // No captures means we don't even need to allocate
    // an env pointer. This is also safe to free, so no
    // special logic needed there.
    if (PcGroup->CaptureGroup->Captures.IsEmpty()) {
      return llvm::ConstantPointerNull::get(llvm::PointerType::get(*ctx->Context, 0));
    }

    // Easy alloca for the closure's environment pointer,
    // as no additional lifetime logic is needed. Use the
    // stack.
    if (not env_outlives_frame) {
      return codegen::LlvmEntryAlloca(closure_env_ty, "closure.env.alloca." + uid, ctx);
    }

    // Use the heap, because of escaping borrows. This needs
    // more investigation as I'd rather not use the heap at
    // all here.
    const auto llvm_size_ty = llvm::Type::getInt64Ty(*ctx->Context);
    const auto llvm_malloc = codegen::GetEmissionModule(*ctx)->getOrInsertFunction(
      "sppc_malloc", llvm::FunctionType::get(llvm::PointerType::get(*ctx->Context, 0), {llvm_size_ty}, false));
    const auto env_size = codegen::GetEmissionModule(
      *ctx)->getDataLayout().getTypeAllocSize(closure_env_ty).getFixedValue();
    return ctx->Builder.CreateCall(
      llvm_malloc, {llvm::ConstantInt::get(llvm_size_ty, env_size)}, "closure.env.heap." + uid);
  }();

  for (auto const &[i, capture] : PcGroup->CaptureGroup->Captures | genex::views::ptr | genex::views::enumerate) {
    const auto zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), 0);
    const auto capture_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), i);

    const auto field_ptr = ctx->Builder.CreateInBoundsGEP(
      closure_env_ty, env_alloca, {zero, capture_index},
      "closure.env.gep." + std::to_string(i));

    // For a borrowed capture (&x / &mut x) the env field is a
    // pointer, so store the address of thec captured variable;
    // for a by-value (mov) capture, store the value itself.
    const auto val = capture->Conv != nullptr
      ? sm->CurrentScope->GetVarSymbol(capture->Val->To<IdentifierAst>())->LlvmInfo->Alloca
      : capture->Val->Stage11_CodeGen(sm, meta, ctx);
    ctx->Builder.CreateStore(val, field_ptr);
  }

  // Build the closure value as its FunXXX type, which lowers to a
  // { fn_ptr, env_ptr } pair (RegisterLlvmTypeInfo).
  const auto llvm_closure_ty = llvm::cast<llvm::StructType>(
    codegen::GetLlvmTypeOf(*InferType(sm, meta), *sm->CurrentScope, ctx));

  const auto closure_alloca = codegen::LlvmEntryAlloca(
    llvm_closure_ty, "closure.obj.alloca." + uid, ctx);

  ctx->Builder.CreateStore(
    llvm_fn, ctx->Builder.CreateStructGEP(llvm_closure_ty, closure_alloca, 0));

  ctx->Builder.CreateStore(
    env_alloca, ctx->Builder.CreateStructGEP(llvm_closure_ty, closure_alloca, 1));

  // Return the generated closure.
  return ctx->Builder.CreateLoad(llvm_closure_ty, closure_alloca, "load.closure.obj." + uid);
}

auto spp::asts::ClosureExpressionAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // A closure is its own special type, which superimposes the
  // functional one. Before stage 7 has minted it there is nothing
  // to give but the functional type itself, which is what every
  // reader saw before closures had a type of their own.
  return _MockType != nullptr ? _MockType : _FunctionalType(sm, meta);
}

auto spp::asts::ClosureExpressionAst::_FunctionalType(
  ScopeManager *sm,
  CompilerMetaData *meta) const
  -> Shared<TypeAst> {
  // Create the type as a nullptr, so it can be analysed
  // later.
  using generate::common_types::FunRefType;
  using generate::common_types::FunMutType;
  using generate::common_types::FunMovType;
  using generate::common_types::TupleType;
  Shared<TypeAst> ty = nullptr;

  auto is_ref_cap = [](auto const &cap) { return cap->Conv and *cap->Conv == ConventionTag::REF; };
  auto is_mut_cap = [](auto const &cap) { return cap->Conv and *cap->Conv == ConventionTag::MUT; };

  // If there are no captures, return a FunRef type with
  // the parameters and return type.
  if (PcGroup->CaptureGroup->Captures.IsEmpty()) {
    auto param_types = PcGroup->ParamGroup->Params
      | genex::views::transform([](auto const &x) { return x->Type; })
      | genex::to<Vec>();
    ty = FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), _TrueRetType);
  }

  // If there are captures, but no borrowed captures, return a
  // FunMov type with the parameters and return type.
  else if (genex::any_of(PcGroup->CaptureGroup->Captures, [](auto const &x) { return x->Conv == nullptr; })) {
    auto param_types = PcGroup->ParamGroup->Params
      | genex::views::transform([](auto const &x) { return x->Type; })
      | genex::to<Vec>();
    ty = FunMovType(PosStart(), TupleType(PosStart(), std::move(param_types)), _TrueRetType);
  }

  // If there are mutably borrowed captures, return a FunMut
  // type with the parameters and return type.
  else if (genex::any_of(PcGroup->CaptureGroup->Captures, is_mut_cap)) {
    auto param_types = PcGroup->ParamGroup->Params
      | genex::views::transform([](auto const &x) { return x->Type; })
      | genex::to<Vec>();
    ty = FunMutType(PosStart(), TupleType(PosStart(), std::move(param_types)), _TrueRetType);
  }

  // If there are immutable borrowed captures, return a FunRef
  // type with the parameters and return type.
  else if (genex::any_of(PcGroup->CaptureGroup->Captures, is_ref_cap)) {
    auto param_types = PcGroup->ParamGroup->Params
      | genex::views::transform([](auto const &x) { return x->Type; })
      | genex::to<Vec>();
    ty = FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), _TrueRetType);
  }

  // Analyse the type and return it.
  ty->Stage7_AnalyseSemantics(sm, meta);
  return ty;
}

auto spp::asts::ClosureExpressionAst::_MakeMockType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  using analyse::scopes::BumpTypeStructureGeneration;
  using analyse::scopes::Scope;
  using analyse::scopes::ScopeBlockName;
  using analyse::scopes::TypeSymbol;

  // The functional type is what the mock superimposes, so it
  // has to resolve before there is anything to attach to.
  const auto fun_type = _FunctionalType(sm, meta);
  const auto fun_sym = sm->CurrentScope->GetTypeSymbol(fun_type.get());
  if (fun_sym == nullptr or fun_sym->LinkedScope == nullptr) { return fun_type; }

  // Registered globally rather than against the frame or the
  // module the closure was written in. A closure handed to a
  // generic is resolved again inside that generic's own scope.
  const auto mod_scope = sm->GlobalScope.get();
  auto mock_name = MakeShared<TypeIdentifierAst>(
    PosStart(), Str("$closure") + spp::utils::Uid(this), nullptr);
  auto mock_ast = MakeUnique<ClassPrototypeAst>(
    SPP_NO_ANNOTATIONS, nullptr, mock_name, nullptr, nullptr);
  auto mock_scope = MakeUnique<Scope>(
    ScopeBlockName::FromParts("closure-type", {mock_name.get()}, PosStart()),
    mod_scope, mock_ast.get());

  // Build the symbol for this mock type name, for storage in
  // the symbol table.
  const auto mock_sym = MakeShared<TypeSymbol>(
    mock_name, mock_ast.get(), mock_scope.get(),
    mod_scope, mod_scope, false, false, utils::Visibility::kPublic);

  // Hook the genuine function type into the closure mock type's
  // sup scope list, as happens with normal overload resolution
  // of functions / methods.
  BumpTypeStructureGeneration();
  mock_scope->DirectSupScopes.EmplaceBack(fun_sym->LinkedScope);
  mock_scope->TySym = mock_sym;

  // Use the captures to determine if the closure can be copied
  // which is based on the state of the captures values; if they
  // are all copyable, so is the closure.
  mock_sym->IsDirectlyCopyable = genex::all_of(
    PcGroup->CaptureGroup->Captures,
    [](auto const &cap) { return cap->Conv != nullptr and *cap->Conv == ConventionTag::REF; });

  // As the $closure types are unique per closure, we can directly
  // attach the ThreadSafe constraint to the mock type. This is
  // required for the thread `spawn` function.
  mock_sym->IsDirectlyThreadHazard = genex::any_of(
    PcGroup->CaptureGroup->Captures, [&](auto const &cap) {
      if (cap->Conv != nullptr) { return true; }
      const auto cap_sym = sm->CurrentScope->GetVarSymbol(cap->Val->template To<IdentifierAst>());
      const auto cap_type_sym = sm->CurrentScope->GetTypeSymbol(cap_sym->Type.get());
      return cap_type_sym != nullptr and not cap_type_sym->IsThreadSafe();
    });

  // Add the $closure type symbol into the module scope and save
  // then scope into the temp scopes for persistence.
  mod_scope->AddTypeSymbol(mock_sym);
  _MockAsts.EmplaceBack(std::move(mock_ast));
  analyse::scopes::ScopeManager::temp_scopes.EmplaceBack(
    std::move(mock_scope));
  return mock_name;
}

auto spp::asts::ClosureExpressionAst::ClearMockAsts()
  -> void {
  // Empty the temp asts (manual memory freeing).
  _MockAsts.Clear();
}

auto spp::asts::ClosureExpressionAst::GetLlvmFunc() const
  -> Shared<codegen::LlvmFuncWrapper> {
  return _LlvmFunc;
}

SPP_MOD_END
