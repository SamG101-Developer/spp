module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/codegen/macros.hpp>

module spp.asts.function_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.builtins;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.instantiation_queue;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_implementation_lowered_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.string_literal_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.compiler_stages;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
  decltype(Annotations) &&annotations,
  decltype(TokCmp) &&tok_cmp,
  decltype(TokFun) &&tok_fun,
  decltype(Name) name,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(FnParamGroup) &&param_group,
  decltype(TokArrow) &&tok_arrow,
  decltype(ReturnType) return_type,
  decltype(Impl) &&impl) :
  AbstractAnnotation(nullptr),
  VirtualAnnotation(nullptr),
  TemperatureAnnotation(nullptr),
  FfiAnnotation(nullptr),
  BuiltinAnnotation(nullptr),
  TestAnnotation(nullptr),
  InlineAnnotation({nullptr, ""}),
  Annotations(std::move(annotations)),
  TokCmp(std::move(tok_cmp)),
  TokFun(std::move(tok_fun)),
  Name(std::move(name)),
  GnParamGroup(std::move(generic_param_group)),
  FnParamGroup(std::move(param_group)),
  TokArrow(std::move(tok_arrow)),
  ReturnType(std::move(return_type)),
  Impl(std::move(impl)),
  _NonGenericImpl(nullptr),
  _LlvmFunc(nullptr),
  _OwnerCtx(nullptr),
  _AnnotationInfo(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
  // SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
  Source.OriginalImpl = AstClone(this->Impl);
  Source.OriginalReturnType = AstClone(this->ReturnType);
  _NonGenericImpl = this;
  _LlvmFunc = MakeShared<Shared<codegen::LlvmFuncWrapper>>(nullptr);
}

spp::asts::FunctionPrototypeAst::~FunctionPrototypeAst() = default;

auto spp::asts::FunctionPrototypeAst::PosStart() const
  -> std::size_t {
  // Use the "fun"/"cor" token.
  return TokCmp ? TokCmp->PosStart() : TokFun->PosStart();
}

auto spp::asts::FunctionPrototypeAst::PosEnd() const
  -> std::size_t {
  // Use the return type.
  return Source.OriginalReturnType->PosEnd();
}

auto spp::asts::FunctionPrototypeAst::Clone() const
  -> Unique<Ast> {
  // FunctionPrototypeAst is abstract, so cloning it is not allowed.
  throw std::runtime_error(
    "Use SubroutinePrototypeAst or CoroutinePrototypeAst instead");
}

auto spp::asts::FunctionPrototypeAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n")
  SPP_STRING_APPEND_RAW(not Annotations.IsEmpty() ? "\n" : "");
  SPP_STRING_APPEND(TokCmp).append(TokCmp ? " " : "");
  SPP_STRING_APPEND(TokFun).append(" ");
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(GnParamGroup);
  SPP_STRING_APPEND(FnParamGroup).append(" ");
  SPP_STRING_APPEND_RAW("-> ");
  SPP_STRING_APPEND(ReturnType).append(" ");
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto spp::asts::FunctionPrototypeAst::GenerateLlvmDeclaration(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> Shared<codegen::LlvmFuncWrapper> {
  // Generate the return and parameter types.
  using analyse::utils::type_predicates::IsTypeNever;
  using A = analyse::utils::annotation_utils::BuiltinAnnotations;
  auto [is_generic, llvm_ret_type, llvm_param_types] = _IsPureGeneric(
    sm, meta, ctx);

  if (not is_generic) {
    // Create the LLVM function type. A "..a: T" parameter is never true C-ABI varargs - the call site (NameFnArgs)
    // always collapses the trailing arguments into a single tuple value ahead of time, so the callee is an ordinary
    // fixed-arity function whose last parameter happens to have a tuple type.
    const auto llvm_fun_type = llvm::FunctionType::get(
      llvm_ret_type, llvm_param_types.ToStdVector(), false);

    // Create the LLVM function and add it to the context. An ffi
    // function is declared under the symbol its annotation names,
    // because that is what the shared library exports and what
    // the linker will resolve against. Everything else gets the
    // S++ mangled name.
    const auto ffi_symbol = GetFfiSymbolName();

    // Shortcut for ffi functions: only ever one symbol
    // because of the C ABI compatibility. Enforced by the
    // generics ban too.
    if (not ffi_symbol.empty()) {
      if (const auto existing = ctx->Module->getFunction(ffi_symbol); existing != nullptr) {
        *_LlvmFunc = MakeShared<codegen::LlvmFuncWrapper>(existing);
        return *_LlvmFunc;
      }
    }

    const auto created_llvm_func = llvm::Function::Create(
      llvm_fun_type, llvm::Function::ExternalLinkage,
      ffi_symbol.empty() ? codegen::mangle::mangle_fun_name(*sm->CurrentScope, *this) : ffi_symbol,
      ctx->Module.get());
    const auto func = MakeShared<codegen::LlvmFuncWrapper>(created_llvm_func);

    // Apply standard optimization flags for the function,
    // and read the inline annotation if its present.
    func->Target->addFnAttr(llvm::Attribute::NoUnwind);
    if (InlineAnnotation.first != nullptr) {
      if (InlineAnnotation.second == A::kLlvmInline) {
        func->Target->addFnAttr(llvm::Attribute::InlineHint);
      }
      else if (InlineAnnotation.second == A::kLlvmAlwaysInline) {
        func->Target->addFnAttr(llvm::Attribute::AlwaysInline);
      }
      else if (InlineAnnotation.second == A::kLlvmNoInline) {
        func->Target->addFnAttr(llvm::Attribute::NoInline);
      }
    }

    func->Target->addFnAttr(IsTypeNever(*ReturnType, *sm->CurrentScope)
      ? llvm::Attribute::NoReturn
      : llvm::Attribute::WillReturn);

    // const auto is_coro = To<CoroutinePrototypeAst>() != nullptr;
    // Todo: Captures, NoFree (in non "del" methods), NoSync, NoRecurse (detect recursion in stage7)
    //  ZExt, SExt?
    const auto deref_bytes = [&](TypeAst const &param_type) -> std::uint64_t {
      const auto pointee = codegen::GetLlvmTypeOf(*param_type.WithoutConvention(), *sm->CurrentScope, ctx);
      return pointee != nullptr and pointee->isSized()
        ? ctx->Module->getDataLayout().getTypeAllocSize(pointee).getFixedValue()
        : 0;
    };

    for (const auto i : genex::views::iota(FnParamGroup->Params.Len())) {
      const auto j = static_cast<unsigned>(i);
      auto const &param_type = *FnParamGroup->Params[i]->Type;
      if (param_type.GetConvention() == nullptr) {
        func->Target->addParamAttr(j, llvm::Attribute::NoUndef);
        continue;
      }

      const auto is_ref = param_type.GetConvention()->To<ConventionRefAst>() != nullptr;
      const auto is_mut = param_type.GetConvention()->To<ConventionMutAst>() != nullptr;
      if (not is_ref and not is_mut) { continue; }

      func->Target->addParamAttr(j, llvm::Attribute::NonNull);
      func->Target->addParamAttr(j, llvm::Attribute::NoUndef);
      // func->Target->addParamAttr(j, is_ref ? llvm::Attribute::ReadOnly : llvm::Attribute::NoAlias);
      if (is_ref) { func->Target->addParamAttr(j, llvm::Attribute::ReadOnly); }
      if (const auto bytes = deref_bytes(param_type); bytes > 0) {
        func->Target->addDereferenceableParamAttr(j, bytes);
      }
      // if (is_ref and not is_coro) func->Target->addParamAttr(j, llvm::Attribute::NoCapture);
    }

    // "noundef" is meaningless on a "void" return (there is no value to be
    // undefined), and llvm rejects it outright, so only mark value returns.
    if (not llvm_ret_type->isVoidTy()) {
      func->Target->addRetAttr(llvm::Attribute::NoUndef);
    }

    *_LlvmFunc = func;
  }
  return *_LlvmFunc;
}

auto spp::asts::FunctionPrototypeAst::Stage1_PreProcess(
  Ast *ctx)
  -> void {
  // Get the name of either the module, sup, or sup-ext context name.
  Ast::Stage1_PreProcess(ctx);

  // Convert the "fun" function to a "sup" superimposition of a "Fun[Mov|Mut|Ref]" type over a mock type.
  auto mock_class_name = TypeIdentifierAst::FromIdentifier(*Name->ToFuncIdentifier());
  auto [function_type, function_call_name] = _DeduceMockClassType();

  // If this is the first overload being converted, then the class needs to be made for the mock type.
  const auto needs_generation = genex::operations::empty(AstBody(ctx)
    | genex::views::cast_dynamic<ClassPrototypeAst*>()
    | genex::views::filter([&mock_class_name](auto const &x) {
      return x->Name->WithoutGenerics() == mock_class_name->WithoutGenerics();
    })
    | genex::to<Vec>());

  if (needs_generation) {
    auto mock_class_ast = MakeUnique<ClassPrototypeAst>(
      SPP_NO_ANNOTATIONS, nullptr, AstClone(mock_class_name), nullptr, nullptr);
    auto mock_constant_value = MakeUnique<ObjectInitializerAst>(AstClone(mock_class_name), nullptr);
    auto mock_constant_ast = MakeUnique<CmpStatementAst>(
      SPP_NO_ANNOTATIONS, nullptr, AstClone(Name), nullptr, AstClone(mock_class_name), nullptr,
      std::move(mock_constant_value));

    if (const auto mod_ctx = ctx->To<ModulePrototypeAst>(); mod_ctx != nullptr) {
      mod_ctx->Impl->Members.EmplaceBack(std::move(mock_class_ast));
      mod_ctx->Impl->Members.EmplaceBack(std::move(mock_constant_ast));
    }
    else if (const auto sup_ctx = ctx->To<SupPrototypeFunctionsAst>(); sup_ctx != nullptr) {
      sup_ctx->Impl->Members.EmplaceBack(std::move(mock_class_ast));
      sup_ctx->Impl->Members.EmplaceBack(std::move(mock_constant_ast));
    }
    else if (const auto ext_ctx = ctx->To<SupPrototypeExtensionAst>(); ext_ctx != nullptr) {
      ext_ctx->Impl->Members.EmplaceBack(std::move(mock_class_ast));
      ext_ctx->Impl->Members.EmplaceBack(std::move(mock_constant_ast));
    }
  }

  // Superimpose the function type over the mock class.
  auto sup_ext_impl_members = Vec<Unique<Ast>>();
  auto clone = AstClone(this);

  // Modify generic pulls. Todo: Document this.
  const auto sup_fn_ctx = ctx->To<SupPrototypeFunctionsAst>();
  const auto sup_ext_ctx = ctx->To<SupPrototypeExtensionAst>();
  if (const auto sup_gn_params = sup_fn_ctx != nullptr
    ? sup_fn_ctx->GnParamGroup.get()
    : sup_ext_ctx != nullptr
    ? sup_ext_ctx->GnParamGroup.get()
    : nullptr; sup_gn_params != nullptr) {
    const auto inherited = sup_gn_params->OptToReq();
    auto &own = clone->GnParamGroup->Params;

    inherited->Params |= genex::actions::remove_if([&own](auto const &p) {
      return genex::any_of(own, [&p](auto const &o) { return *o->Name == *p->Name; });
    });

    auto at = 0uz;
    while (at < own.Len() and own[at]->GetOrderTag() == utils::OrderableTag::kRequiredParam) { ++at; }
    own.Insert(
      own.begin() + static_cast<std::ptrdiff_t>(at),
      std::make_move_iterator(inherited->Params.begin()),
      std::make_move_iterator(inherited->Params.end()));
  }

  for (auto const &a : clone->Annotations) { a->Stage1_PreProcess(clone.get()); }
  sup_ext_impl_members.EmplaceBack(std::move(clone));
  auto mock_sup_ext_impl = MakeUnique<SupImplementationAst>(nullptr, std::move(sup_ext_impl_members), nullptr);
  auto mock_sup_ext = MakeUnique<SupPrototypeExtensionAst>(
    nullptr, GnParamGroup->OptToReq(), std::move(mock_class_name), nullptr, std::move(function_type),
    std::move(mock_sup_ext_impl));
  mock_sup_ext->SetAstCtx(_Ctx);

  // Manipulate the context body with the new mock
  // superimposition extension.
  if (const auto mod_ctx = ctx->To<ModulePrototypeAst>()) {
    mod_ctx->Impl->Members.Insert(mod_ctx->Impl->Members.begin(), std::move(mock_sup_ext));
    mod_ctx->Impl->Members |= genex::actions::remove_if([this](auto const &x) { return x.get() == this; });
  }
  else if (const auto sup_ctx = ctx->To<SupPrototypeFunctionsAst>()) {
    sup_ctx->Impl->Members.Insert(sup_ctx->Impl->Members.begin(), std::move(mock_sup_ext));
    sup_ctx->Impl->Members |= genex::actions::remove_if([this](auto const &x) { return x.get() == this; });
  }
  else if (const auto ext_ctx = ctx->To<SupPrototypeExtensionAst>()) {
    ext_ctx->Impl->Members.Insert(ext_ctx->Impl->Members.begin(), std::move(mock_sup_ext));
    ext_ctx->Impl->Members |= genex::actions::remove_if([this](auto const &x) { return x.get() == this; });
  }
}

auto spp::asts::FunctionPrototypeAst::Stage2_GenTopLvlScopes(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::scopes::ScopeBlockName;
  using analyse::errors::SppSelfIdentifierInvalidContextError;

  // Create a new scope for the function prototype, and
  // move into it.
  auto scope_name = ScopeBlockName::FromParts(
    "function", {Name.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // If there is a self parameter in a free function, throw
  // as error.
  RaiseIf<SppSelfIdentifierInvalidContextError>(
    _Ctx->To<ModulePrototypeAst>() and FnParamGroup->GetSelfParam() != nullptr,
    {sm->CurrentScope}, ERR_ARGS(*FnParamGroup->GetSelfParam()));

  // Run steps for the annotations.
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Generate the generic parameters and attributes of the
  // function.
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage3_GenTopLvlAliases(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  // Skip the function scope, as it is already generated.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage4_QualifyTypes(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Skip the function scope, as it is already qualified.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
  GnParamGroup->Stage4_QualifyTypes(sm, meta);
  Impl->Stage4_QualifyTypes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage5_LoadSupScopes(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::utils::type_predicates::IsTypeBorrowed;

  // Analyse the signature before sup scopes are attached.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

  // Ensure overloads have the same visibility by
  // comparing to the master symbol.
  // Todo: Tidy this?
  if (Name and not Name->Val.starts_with("$")) {
    if (const auto *outer_scope = sm->CurrentScope->Parent != nullptr ? sm->CurrentScope->Parent->Parent : nullptr) {
      if (const auto mock_sym = outer_scope->GetVarSymbol(Name.get(), true)) {
        if (mock_sym->Type and mock_sym->Type->IsCompilerGeneratedType()) {
          // Enforce that all overloads have the same
          // visibility.
          RaiseIf<analyse::errors::SppFunctionOverloadVisibilityMismatchError>(
            mock_sym->VisibilityAnnotation != nullptr and mock_sym->Visibility != Visibility.first,
            {sm->CurrentScope}, ERR_ARGS(*mock_sym->VisibilityAnnotation, *this, *Visibility.second));
          mock_sym->Visibility = Visibility.first;
          mock_sym->VisibilityAnnotation = Visibility.second;
        }
      }
    }
  }

  FnParamGroup->Stage7_AnalyseSemantics(sm, meta);
  ReturnType->Stage7_AnalyseSemantics(sm, meta);
  ReturnType = sm->CurrentScope->GetTypeSymbol(ReturnType.get())->FqName()->WithConvention(
    AstClone(ReturnType->GetConvention()));

  // Ensure the function's return type does not have
  // a convention.
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*ReturnType, *sm),
    {sm->CurrentScope}, ERR_ARGS(*ReturnType, *ReturnType, "function return type"));

  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage6_PreAnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::func_utils::CheckForConflictingOverload;
  using analyse::utils::type_utils::ResolveAndSubstituteSelfType;
  using analyse::errors::SppFunctionPrototypeConflictError;
  using generate::common_types_precompiled::SELF_VAR;

  // Perform conflict checking before standard semantic
  // analysis errors due to multiple possible prototypes.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Apply constraints in the types. Force re-analysis
  // post sup-scope attachment, but before stage 7, to remain
  // order agnostic.
  for (auto const &param : FnParamGroup->GetAllParams()) {
    param->Type->ResetCache();
    param->Type->Stage7_AnalyseSemantics(sm, meta);
  }
  ReturnType->ResetCache();
  ReturnType->Stage7_AnalyseSemantics(sm, meta);

  const auto mod_ctx = _Ctx->To<ModulePrototypeAst>();
  const auto type_scope = mod_ctx
    ? sm->CurrentScope->ParentModule()
    : _Ctx->GetAstScope()->GetTypeSymbol(AstName(_Ctx).get())->LinkedScope;

  // Error if there are conflicts. This has to run here rather
  // than in stage 5, because sup scopes are only attached once
  // every module has finished stage 5, so a type's methods
  // aren't reachable from its class scope until now.
  // Todo: Maybe need 2 scopes if the conflict is across
  //  modules (if possible, esp in sup-blocks)?
  const auto conflict = CheckForConflictingOverload(*sm->CurrentScope, type_scope, *this, *sm, meta);
  RaiseIf<SppFunctionPrototypeConflictError>(
    conflict, {sm->CurrentScope}, ERR_ARGS(*conflict, *this));

  // New version
  if (const auto self_param = FnParamGroup->GetSelfParam()) {
    const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
    const auto self_conv = self_param->Conv.get();

    self_sym->Type = ResolveAndSubstituteSelfType(*self_sym->Type, *sm->CurrentScope, *sm, *meta)->WithConvention(
      AstClone(self_conv));

    for (auto const &param : FnParamGroup->GetAllParams()) {
      const auto var_sym = sm->CurrentScope->GetVarSymbol(param->ExtractName().get());
      if (var_sym == nullptr) { continue; } // Destructuring parameters.
      var_sym->Type = ResolveAndSubstituteSelfType(*var_sym->Type, *sm->CurrentScope, *sm, *meta);
    }
  }

  // If this is a !compiler_builtin function, swap in the lowered
  // implementation now (stage 6), so that the lowered (comptime)
  // body is available to any stage 9 call regardless of definition
  // order.
  _InstallLoweredImpl(sm);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::_InstallLoweredImpl(
  ScopeManager *sm)
  -> void {
  if (BuiltinAnnotation) {
    const auto name = BuiltinAnnotation->FnArgGroup->At("name")->Val->ToUnchecked<StringLiteralAst>()->CppVal();

    auto lowered_impl = FunctionImplementationLoweredAst::NewEmpty();
    lowered_impl->SetScopePtr(name);
    lowered_impl->SetProtoPtr(this);
    Impl = std::move(lowered_impl);

    // Clear uninitialized memory (the AstNodes) as original
    // nodes in unique pointers have been destroyed from the
    // "lowered" node swapping.
    const auto forget_body_asts = [](auto const &self, analyse::scopes::Scope const *scope) -> void {
      for (auto const &child : scope->Children) {
        child->AstNode = nullptr;
        self(self, child.get());
      }
    };
    forget_body_asts(forget_body_asts, sm->CurrentScope);

    const auto err1 = "compiler_builtin function '" + name + "' is not registered in kBuiltinFuncs";
    RaiseIf<analyse::errors::SppInternalCompilerError>(
      not analyse::utils::builtins::kBuiltinFuncs.contains(name),
      {sm->CurrentScope}, ERR_ARGS(*Name, err1));

    const auto err2 = "compiler_builtin function '" + name + "' missing builtin comptime implementation";
    RaiseIf<analyse::errors::SppInternalCompilerError>(
      TokCmp != nullptr and analyse::utils::builtins::kBuiltinFuncs.at(name).cmp_fn == nullptr,
      {sm->CurrentScope}, ERR_ARGS(*TokCmp, err2));
  }
}

auto spp::asts::FunctionPrototypeAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::errors::SppEmptyBodyRequiredError;
  using analyse::utils::type_predicates::IsTypeBorrowed;
  using analyse::utils::type_compare::TypeEq;

  // Move into the function scope, as it is now ready for
  // semantic analysis.
  sm->MoveToNextScope();

  // SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }

  // A unit test is entered by the test harness, which has nothing to pass it and nowhere to put a result, so its
  // signature is fixed. Checked here rather than where the annotation binds, because that runs per-annotation and has
  // no view of the rest of the prototype.
  if (TestAnnotation != nullptr) {
    using analyse::errors::SppUnitTestInvalidSignatureError;
    using generate::common_types_precompiled::VOID;
    const auto bad = [&](const StrView requirement) {
      Raise<SppUnitTestInvalidSignatureError>(
        {sm->CurrentScope}, ERR_ARGS(*TestAnnotation, *Name, requirement));
    };
    if (TokCmp != nullptr) { bad("is a 'cmp' function"); }
    if (not FnParamGroup->Params.IsEmpty()) { bad("declares parameters"); }
    if (not GnParamGroup->Params.IsEmpty()) { bad("declares generic parameters"); }
    if (not TypeEq(*ReturnType, *VOID, *sm->CurrentScope, *sm->CurrentScope)) {
      bad("does not return 'Void'");
    }
  }

  // An ffi function body is in C, so there is no body
  // expressible in S++.
  RaiseIf<SppEmptyBodyRequiredError>(
    FfiAnnotation != nullptr and not Impl->Members.IsEmpty(),
    {sm->CurrentScope}, ERR_ARGS(
      *FfiAnnotation, *Impl->Members.Front(), "an '!ffi' function", "the linker resolves the implementation from C"));

  // An abstract function body is never ran (unreachable)
  // so must be empty.
  RaiseIf<SppEmptyBodyRequiredError>(
    AbstractAnnotation != nullptr and not Impl->Members.IsEmpty(),
    {sm->CurrentScope}, ERR_ARGS(
      *AbstractAnnotation, *Impl->Members.Front(), "an '!abstract_method' method", "abstract methods aren't callable"));

  // Repeated convention check for generic substitutions.
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*ReturnType, *sm),
    {sm->CurrentScope}, ERR_ARGS(*ReturnType, *ReturnType, "function return type"));

  // Analyse the generic parameter group, and the parameter
  // group.
  for (auto const &p : FnParamGroup->GetNonSelfParams()) {
    p->Stage6_PreAnalyseSemantics(sm, meta);
  }
  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  // Note: the !compiler_builtin lowered-implementation swap
  // now happens in Stage6_PreAnalyseSemantics, so that the
  // lowered comptime body is available to dependent stage 9
  // calls regardless of definition order.

  // There is no scope exit, as subclasses will call this
  // method, and finish the analysis themselves.
}

auto spp::asts::FunctionPrototypeAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Move into the function scope, as it is now ready for
  // memory checking.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Check the memory for the parameter group and implementation.
  // "EnclosingFunctionScope" is set here as well as in stage 7,
  // because the early-exit linearity check needs to know where to
  // stop walking outwards.
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->EnclosingFunctionScope = sm->CurrentScope;
    FnParamGroup->Stage8_CheckMemory(sm, meta);
    Impl->Stage8_CheckMemory(sm, meta);

    // A function whose body is not written in S++ is exempt: an
    // intrinsic is implemented by code generation, an ffi function by
    // a foreign library, and an abstract method by whoever overrides
    // it. There is no body that could have consumed the parameters,
    // so there is nothing to hold to the rule.
    if (BuiltinAnnotation == nullptr and FfiAnnotation == nullptr and AbstractAnnotation == nullptr
      and not Impl->Terminates()) {
      analyse::utils::linear_utils::CheckScopeExit(
        *sm->CurrentScope, *Impl, "Function end", *sm, meta);
    }
  }

  // Move out of the function scope, as it is now complete.
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage9_CompTimeResolve(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Manual scope skipping.
  sm->MoveToNextScope();
  for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
  sm->ExhaustScope();
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage10_PreCodeGen(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Create the declaration, but not the definition, of the
  // function. This allows for order-agnostic behaviour.
  sm->MoveToNextScope();

  // This walk visits every module with that module's own context,
  // so it is where a prototype learns which module owns it. Read
  // back through "OwnerCtx", including by instantiations minted
  // after this stage, which have no stamp of their own.
  _OwnerCtx = ctx;

  // Handle the main function prototype.
  GenerateLlvmDeclaration(sm, meta, ctx);

  // Handle generic substitutions of a function.
  for (auto const &sub : _GenericSubstitutions) {
    if (sub.Proto == nullptr or not sub.IsConcrete) { continue; }
    sub.Proto->_OwnerCtx = ctx;
    auto tm = ScopeManager(sm->GlobalScope, sub.WalkScope());
    sub.Proto->GenerateLlvmDeclaration(&tm, meta, ctx);
  }

  // Manual scope skipping.
  const auto final_scope = sm->CurrentScope->FinalChildScope();
  while (sm->CurrentScope != final_scope) {
    sm->MoveToNextScope(false);
  }

  return nullptr;
}

auto spp::asts::FunctionPrototypeAst::_CodeGenGenericSubstitutions(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> void {
  // Emit the bodies of this prototype's instantiations. Their own bodies
  // were analysed by the monomorphisation stage, which ran to a fixed
  // point before any code generation: an instantiation reached only from
  // inside another generic body would otherwise be discovered here,
  // after the module owning its template had already been walked past.
  for (auto const &sub : _GenericSubstitutions) {
    if (sub.Proto == nullptr or not sub.IsConcrete) { continue; }
    auto tm = ScopeManager(sm->GlobalScope, sub.WalkScope());
    tm.Reset(tm.CurrentScope);
    GnParamGroup->Stage11_CodeGen(&tm, meta, ctx);
    sub.Proto->Stage11_CodeGen(&tm, meta, ctx);
  }
}

auto spp::asts::FunctionPrototypeAst::AnalysePendingGenericSubstitutions(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Iterating while appending is deliberate, and is why the
  // substitutions are held in a list: analysing one instantiation
  // can instantiate this very prototype again (a generic function
  // that calls itself with different arguments), and a list's end
  // iterator stays valid across the append, so the new entry is
  // picked up by this same loop rather than waiting for the
  // template to come back around the queue.
  for (auto &sub : _GenericSubstitutions) {
    if (sub.BodyAnalysed) { continue; }

    // An overload candidate that failed after its scope was
    // reserved leaves the slot empty. It is never filled in
    // later, so it is left alone rather than marked - there is
    // no prototype to mark anything about.
    if (sub.Proto == nullptr) { continue; }
    sub.BodyAnalysed = true;
    auto tm = ScopeManager(sm->GlobalScope, sub.WalkScope());
    if (not sub.IsConcrete) { continue; }

    // Discard the scopes the template's own body analysis left
    // behind under this clone. They describe the template's body,
    // and the analysis below builds this instantiation's in their
    // place.
    sub.ProtoScope()->Children.Clear();
    sub.WalkScope()->FixChildrenToParentPointer();
    tm.Reset(tm.CurrentScope);

    // The instantiation was built from the signature alone, so the
    // body it holds is still the template's, unanalysed.
    sub.Proto->Impl = std::move(sub.Proto->Source.OriginalImpl);
    sub.Proto->_InstallLoweredImpl(&tm);

    const auto _meta_guard = meta::MetaGuard(meta);
    meta->ResolveBoundCompGenerics = true;
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;

    // Same relaxation, and for the same reason, as the one
    // "AnalyseSubstitutedType" applies to the types a substitution
    // produces: an instantiated body is reached out of the order
    // the writer's own code is, so it may name an abstract type
    // before the implementation satisfying it has been attached.
    // Nothing is lost by allowing it - the template's own body was
    // analysed in written order at stage 7, and that is what holds
    // the author to the rule.
    meta->AllowAbstractType = true;
    sub.Proto->Stage7_AnalyseSemantics(&tm, meta);

    tm.Reset(sub.WalkScope());
    tm.MoveToNextScope();
    meta->EnclosingFunctionScope = tm.CurrentScope;
    sub.Proto->FnParamGroup->Stage8_CheckMemory(&tm, meta);
    sub.Proto->Impl->Stage8_CheckMemory(&tm, meta);
    if (sub.Proto->BuiltinAnnotation == nullptr and sub.Proto->FfiAnnotation == nullptr
      and sub.Proto->AbstractAnnotation == nullptr and not sub.Proto->Impl->Terminates()) {
      analyse::utils::linear_utils::CheckScopeExit(
        *tm.CurrentScope, *sub.Proto->Impl, "Function end", tm, meta);
    }

    _EnsureDropsForBuiltin(*sub.Proto, tm, meta);
  }
}

auto spp::asts::FunctionPrototypeAst::_EnsureDropsForBuiltin(
  FunctionPrototypeAst const &sub_proto,
  ScopeManager &tm,
  CompilerMetaData *meta)
  -> void {
  if (sub_proto.BuiltinAnnotation == nullptr) { return; }

  const auto name_arg = sub_proto.BuiltinAnnotation->FnArgGroup->At("name");
  if (name_arg == nullptr) { return; }
  const auto name = name_arg->Val->ToUnchecked<StringLiteralAst>()->CppVal();
  if (name != "std.mem.ops.drop" and name != "std.mem.ops.drop_in_place") { return; }

  // "T" is what the instantiation bound, and the scope manager
  // is sitting inside the instantiation's own scope, which is
  // where that binding lives.
  const auto t_ast = TypeIdentifierAst::FromString("T");
  const auto t_sym = tm.CurrentScope->GetTypeSymbol(t_ast.get());
  if (t_sym == nullptr) { return; }
  analyse::utils::drop_utils::EnsureDropInstantiated(*t_sym, tm, meta);
}

auto spp::asts::FunctionPrototypeAst::GetFfiSymbolName() const
  -> Str {
  if (FfiAnnotation == nullptr) { return ""; }
  const auto symbol_arg = FfiAnnotation->FnArgGroup->At("symbol");
  if (symbol_arg == nullptr) { return ""; }
  const auto symbol_literal = symbol_arg->Val->To<StringLiteralAst>();
  return symbol_literal != nullptr ? symbol_literal->CppVal() : Str();
}

auto spp::asts::FunctionPrototypeAst::GetLlvmFunc() const
  -> Shared<codegen::LlvmFuncWrapper> {
  return *_LlvmFunc;
}

auto spp::asts::FunctionPrototypeAst::OwnerCtx() const
  -> codegen::LlvmCtx* {
  // Walk to the template this was substituted from, which is
  // what carries the stamp when this is an instantiation minted
  // after Stage10. "_NonGenericImpl" is self-referential on a
  // prototype that is not one, which ends the walk.
  for (auto const *proto = this; proto != nullptr; proto = proto->_NonGenericImpl) {
    if (proto->_OwnerCtx != nullptr) { return proto->_OwnerCtx; }
    if (proto->_NonGenericImpl == proto) { break; }
  }
  return nullptr;
}

auto spp::asts::FunctionPrototypeAst::DetachLlvmFuncSlot()
  -> void {
  // Break the slot shared with the prototype this was cloned
  // from, so this function gets its own llvm target.
  _LlvmFunc = MakeShared<Shared<codegen::LlvmFuncWrapper>>(nullptr);
}

auto spp::asts::FunctionPrototypeAst::PrintSignature(
  Str const &) const
  -> Str {
  SPP_STRING_START.append(Name->Val);
  SPP_STRING_APPEND(GnParamGroup);
  SPP_STRING_APPEND(FnParamGroup).append(" ");
  SPP_STRING_APPEND(TokArrow).append(" ");
  SPP_STRING_APPEND(ReturnType);
  SPP_STRING_END;
}

auto spp::asts::FunctionPrototypeAst::GenericSubstitution::WalkScope() const
  -> analyse::scopes::Scope* {
  return OwnedScope.get();
}

auto spp::asts::FunctionPrototypeAst::GenericSubstitution::ProtoScope() const
  -> analyse::scopes::Scope* {
  return OwnedScope->Children[0].get();
}

auto spp::asts::FunctionPrototypeAst::RegisterGenericSubstitution(
  Unique<analyse::scopes::Scope> &&scope,
  Unique<FunctionPrototypeAst> &&new_ast,
  Unique<GenericArgumentGroupAst> &&gn_args)
  -> void {
  // Store the scope for object persistence (and codegen), keyed
  // by the arguments that produced it.
  _GenericSubstitutions.emplace_back(
    GenericSubstitution{
      .OwnedScope = std::move(scope),
      .Proto = std::move(new_ast),
      .GnArgs = std::move(gn_args)
    });

  // The instantiation's body has not been analysed yet, and
  // nothing walks the ast to find it - a substitution is
  // registered against the template, wherever the template
  // happens to live, from wherever the call that produced it was
  // written. Record the template so the monomorphisation stage
  // comes back for it.
  analyse::utils::instantiation_queue::Enqueue(this);
}

auto spp::asts::FunctionPrototypeAst::FindGenericSubstitution(
  GenericArgumentGroupAst const &gn_args) const
  -> Pair<analyse::scopes::Scope*, FunctionPrototypeAst*> {
  // Get the generic implementation for a given set of generic
  // arguments.
  for (auto const &sub : _GenericSubstitutions) {
    if (sub.Proto == nullptr or sub.GnArgs == nullptr) { continue; }
    if (*sub.GnArgs == gn_args) { return {sub.WalkScope(), sub.Proto.get()}; }
  }

  // If no matches were found then return a pair of nullptr
  // values. This is impossible to reach (I think) but is a
  // failsafe. Todo: std::unreachable()?
  return {nullptr, nullptr};
}

auto spp::asts::FunctionPrototypeAst::RegisteredGenericSubstitutions() const
  -> std::list<Pair<analyse::scopes::Scope*, FunctionPrototypeAst*>> {
  return _GenericSubstitutions
    | genex::views::transform([](auto const &x) { return MakePair(x.WalkScope(), x.Proto.get()); })
    | genex::to<std::list>();
}

auto spp::asts::FunctionPrototypeAst::RegisteredGenericSubstitutions()
  -> std::list<GenericSubstitution>& {
  return _GenericSubstitutions;
}

auto spp::asts::FunctionPrototypeAst::SetNonGenericImpl(
  FunctionPrototypeAst *impl)
  -> void {
  _NonGenericImpl = impl;
}

auto spp::asts::FunctionPrototypeAst::GetNonGenericImpl() const
  -> FunctionPrototypeAst* {
  return _NonGenericImpl;
}

auto spp::asts::FunctionPrototypeAst::MarkAsAnnotation()
  -> void {
  // Mark this function prototype as an annotation, by adding the appropriate annotation to it.
  _AnnotationInfo = MakeUnique<analyse::utils::annotation_utils::AnnotationInfo>();
}

auto spp::asts::FunctionPrototypeAst::GetAnnotationInfo() const
  -> analyse::utils::annotation_utils::AnnotationInfo* {
  return _AnnotationInfo.get();
}

auto spp::asts::FunctionPrototypeAst::_DeduceMockClassType() const
  -> Pair<Shared<TypeAst>, Str> {
  //
  using generate::common_types::FunMovType;
  using generate::common_types::FunMutType;
  using generate::common_types::FunRefType;
  using generate::common_types::TupleType;

  // Extract the parameter types.
  auto param_types = FnParamGroup->Params
    | genex::views::transform([](auto &&x) { return x->Type; })
    | genex::to<Vec>();

  // Module level functions, and static methods, are always FunRef.
  if (_Ctx->To<ModulePrototypeAst>() == nullptr or FnParamGroup->GetSelfParam() == nullptr) {
    return {FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_ref")};
  }

  // Class methods with "self" are the FunMov type.
  if (FnParamGroup->GetSelfParam()->Conv == nullptr) {
    return {FunMovType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_mov")};
  }

  // Class methods with "&mut self" are the FunMut type.
  if (*FnParamGroup->GetSelfParam()->Conv == ConventionTag::MUT) {
    return {FunMutType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_mut")};
  }

  // Class methods with "&self" are the FunRef type.
  if (*FnParamGroup->GetSelfParam()->Conv == ConventionTag::REF) {
    return {FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_ref")};
  }

  std::unreachable();
}

auto spp::asts::FunctionPrototypeAst::_IsPureGeneric(
  analyse::scopes::ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx const *ctx) const
  -> Tup<bool, llvm::Type*, Vec<llvm::Type*>> {
  //
  using analyse::utils::type_utils::ResolveAndSubstituteSelfType;

  // Convert the return and parameter types to LLVM types.
  const auto ret_type = ResolveAndSubstituteSelfType(
    *ReturnType, *sm->CurrentScope, *sm, *meta);
  const auto llvm_ret_type = codegen::GetLlvmTypeOf(
    *ret_type, *sm->CurrentScope, ctx);

  // A variadic parameter declares one element ("..b: T") but
  // receives the whole tuple the call site collapsed its
  // trailing arguments into, so that tuple is what it lowers as.
  // Every argument count is its own instantiation with its own
  // pack type.
  const auto variadic_param = FnParamGroup->GetVariadicParams();
  auto llvm_param_types = FnParamGroup->GetNonSelfParams()
    | genex::views::transform([&](auto const &x) {
      auto const &source_type = (VariadicPackType != nullptr and x == static_cast<FunctionParameterAst*>(
          variadic_param))
        ? VariadicPackType
        : x->Type;
      const auto param_type = ResolveAndSubstituteSelfType(
        *source_type, *sm->CurrentScope, *sm, *meta);
      return codegen::GetLlvmTypeOf(
        *param_type, *sm->CurrentScope, ctx);
    })
    | genex::to<Vec>();

  // For the self param, we add the pointer for "self", if "self"
  // is declared as "&self" or "&mut self". Otherwise, for the
  // value consumed, we add the value type.
  const auto self_param = FnParamGroup->GetSelfParam();
  if (self_param != nullptr) {
    if (self_param->Conv != nullptr) {
      const auto self_ptr_type = llvm::PointerType::get(*ctx->Context, 0);
      llvm_param_types.Insert(llvm_param_types.begin(), self_ptr_type);
    }
    else {
      const auto self_type = ResolveAndSubstituteSelfType(
        *self_param->Type, *sm->CurrentScope, *sm, *meta);
      const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(self_type.get());
      const auto self_val_type = codegen::GetLlvmType(*self_ty_sym, ctx);
      llvm_param_types.Insert(llvm_param_types.begin(), self_val_type);
    }
  }

  // Check if any of the types failed to convert. Any failed
  // conversions indicate a generic.
  const auto all_types_converted = llvm_ret_type != nullptr
    and genex::all_of(llvm_param_types, [](auto const &x) { return x != nullptr; });

  const auto is_pure_generic = not GnParamGroup->Params.IsEmpty() or not all_types_converted;
  return {is_pure_generic, llvm_ret_type, llvm_param_types};
}

SPP_MOD_END
