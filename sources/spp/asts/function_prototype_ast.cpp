module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.builtins;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.class_prototype_ast;
import spp.asts.class_implementation_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_implementation_lowered_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.compiler_stages;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
    decltype(Annotations) &&annotations,
    decltype(TokCmp) &&tok_cmp,
    decltype(TokFun) &&tok_fun,
    decltype(Name) &&name,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(FnParamGroup) &&param_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) &&return_type,
    decltype(Impl) &&impl) :
    AbstractAnnotation(nullptr),
    VirtualAnnotation(nullptr),
    TemperatureAnnotation(nullptr),
    FfiAnnotation(nullptr),
    BuiltinAnnotation(nullptr),
    InlineAnnotation(nullptr),
    Annotations(std::move(annotations)),
    TokCmp(std::move(tok_cmp)),
    TokFun(std::move(tok_fun)),
    Name(std::move(name)),
    GnParamGroup(std::move(generic_param_group)),
    FnParamGroup(std::move(param_group)),
    TokArrow(std::move(tok_arrow)),
    ReturnType(std::move(return_type)),
    Impl(std::move(impl)),
    _LlvmFunc(nullptr),
    _AnnotationInfo(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_FUN, "fun"); // TODO <- "cor"?
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
    // SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnParamGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokArrow, lex::SppTokenType::TK_ARROW_RIGHT, "->");
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
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokCmp).append(TokCmp ? " " : "");
    SPP_STRING_APPEND(TokFun).append(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(GnParamGroup);
    SPP_STRING_APPEND(FnParamGroup).append(" ");
    SPP_STRING_APPEND(TokArrow).append(" ");
    SPP_STRING_APPEND(ReturnType).append(" ");
    SPP_STRING_APPEND(Impl);
    SPP_STRING_END;
}

auto spp::asts::FunctionPrototypeAst::GenerateLlvmDeclaration(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> Shared<codegen::LlvmFuncWrapper> {
    // Generate the return and parameter types.
    auto [is_generic, llvm_ret_type, llvm_param_types] = _IsPureGeneric(sm, ctx);

    if (not is_generic) {
        // Create the LLVM function type.
        const auto is_var_arg = FnParamGroup->GetVariadicParams() != nullptr;
        const auto llvm_fun_type = llvm::FunctionType::get(llvm_ret_type, llvm_param_types.ToStdVector(), is_var_arg);

        // Create the LLVM function and add it to the context.
        const auto created_llvm_func = llvm::Function::Create(
            llvm_fun_type, llvm::Function::ExternalLinkage, codegen::mangle::mangle_fun_name(*sm->CurrentScope, *this),
            ctx->Module.get());
        const auto func = MakeShared<codegen::LlvmFuncWrapper>(created_llvm_func);

        // Apply standard optimization flags.
        func->Target->addFnAttr(llvm::Attribute::NoUnwind);
        func->Target->addFnAttr(llvm::Attribute::NoInline);
        // llvm_func->addFnAttr(analyse::utils::type_utils::is_type_never(*return_type, *sm->CurrentScope)
        //                          ? llvm::Attribute::NoReturn
        //                          : llvm::Attribute::WillReturn);

        // for (const auto i : genex::views::iota(param_group->params.Len() as U32)) {
        //     if (param_group->params[i]->Type->GetConvention() == nullptr) {
        //         llvm_func->addParamAttr(i, llvm::Attribute::NoUndef);
        //     }
        //     else if (param_group->params[i]->Type->GetConvention()->To<ConventionRefAst>()) {
        //         llvm_func->addParamAttr(i, llvm::Attribute::NonNull);
        //         llvm_func->addParamAttr(i, llvm::Attribute::NoUndef);
        //         llvm_func->addParamAttr(i, llvm::Attribute::ReadOnly);
        //         llvm_func->addParamAttr(i, llvm::Attribute::Dereferenceable);
        //     }
        //     else if (param_group->params[i]->Type->GetConvention()->To<ConventionMutAst>()) {
        //         llvm_func->addParamAttr(i, llvm::Attribute::NonNull);
        //         llvm_func->addParamAttr(i, llvm::Attribute::NoUndef);
        //         llvm_func->addParamAttr(i, llvm::Attribute::NoAlias);
        //         llvm_func->addParamAttr(i, llvm::Attribute::Dereferenceable);
        //     }
        // }

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
        | genex::views::filter([&mock_class_name](auto const &x) { return x->Name->WithoutGenerics() == mock_class_name->WithoutGenerics(); })
        | genex::to<Vec>());

    if (needs_generation) {
        auto mock_class_ast = MakeUnique<ClassPrototypeAst>(SPP_NO_ANNOTATIONS, nullptr, AstClone(mock_class_name), nullptr, nullptr);
        auto mock_constant_value = MakeUnique<ObjectInitializerAst>(AstClone(mock_class_name), nullptr);
        auto mock_constant_ast = MakeUnique<CmpStatementAst>(SPP_NO_ANNOTATIONS, nullptr, AstClone(Name), nullptr, AstClone(mock_class_name), nullptr, std::move(mock_constant_value));

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
    // clone->Name = MakeShared<IdentifierAst>(Name->PosStart(), function_call_name);

    for (auto const &a : clone->Annotations) { a->Stage1_PreProcess(clone.get()); }
    sup_ext_impl_members.EmplaceBack(std::move(clone));
    auto mock_sup_ext_impl = MakeUnique<SupImplementationAst>(nullptr, std::move(sup_ext_impl_members), nullptr);
    auto mock_sup_ext = MakeUnique<SupPrototypeExtensionAst>(
        nullptr, GnParamGroup->OptToReq(), std::move(mock_class_name), nullptr, std::move(function_type),
        std::move(mock_sup_ext_impl));
    mock_sup_ext->SetAstCtx(_Ctx);

    // Manipulate the context body with the new mock superimposition extension.
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

    // Create a new scope for the function prototype, and move into it.
    auto scope_name = ScopeBlockName::FromParts(
        "function", {Name.get()}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    // If there is a self parameter in a free function, throw as error.
    RaiseIf<SppSelfIdentifierInvalidContextError>(
        _Ctx->To<ModulePrototypeAst>() and FnParamGroup->GetSelfParam() != nullptr,
        {sm->CurrentScope}, ERR_ARGS(*FnParamGroup->GetSelfParam()));

    // Run steps for the annotations.
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Generate the generic parameters and attributes of the function.
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
    using analyse::utils::type_utils::IsTypeBorrowed;

    // Analyse the parameter and return types before sup scopes are attached.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

    // Ensure overloads have the same visibility by comparing to the master symbol.
    // Todo: Tidy this?
    if (Visibility.Second != nullptr and Name and not Name->Val.starts_with("$")) {
        if (const auto *outer_scope = sm->CurrentScope->Parent != nullptr ? sm->CurrentScope->Parent->Parent : nullptr) {
            if (const auto mock_sym = outer_scope->GetVarSymbol(Name, true)) {
                if (mock_sym->Type and mock_sym->Type->IsCompilerGeneratedType()) {
                    // Enforce that all overloads have the same visibility.
                    if (mock_sym->VisibilityAnnotation != nullptr and mock_sym->Visibility != Visibility.First) {
                        Raise<analyse::errors::SppFunctionOverloadVisibilityMismatchError>(
                            {sm->CurrentScope},
                            ERR_ARGS(*mock_sym->VisibilityAnnotation, *this));
                    }
                    mock_sym->Visibility = Visibility.First;
                    mock_sym->VisibilityAnnotation = Visibility.Second;
                }
            }
        }
    }

    // Carry the convention for error purposes.
    FnParamGroup->Stage7_AnalyseSemantics(sm, meta);
    ReturnType->Stage7_AnalyseSemantics(sm, meta);
    ReturnType = sm->CurrentScope->GetTypeSymbol(ReturnType)->FqName()->WithConvention(AstClone(ReturnType->GetConvention()));

    // Ensure the function's return type does not have a convention.
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*ReturnType, *sm),
        {sm->CurrentScope}, ERR_ARGS(*ReturnType, *ReturnType, "function return type"));

    using analyse::utils::func_utils::CheckForConflictingOverload;
    using analyse::errors::SppFunctionPrototypeConflictError;

    const auto mod_ctx = _Ctx->To<ModulePrototypeAst>();
    const auto type_scope = mod_ctx
        ? sm->CurrentScope->ParentModule()
        : _Ctx->GetAstScope()->GetTypeSymbol(AstName(_Ctx))->LinkedScope;

    // Error if there are conflicts.
    // Todo: Maybe need 2 scopes if the conflict is across modules (if possible, esp in sup-blocks)?
    const auto conflict = CheckForConflictingOverload(*sm->CurrentScope, type_scope, *this, *sm, meta);
    RaiseIf<SppFunctionPrototypeConflictError>(
        conflict, {sm->CurrentScope}, ERR_ARGS(*conflict, *this));

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
    using generate::common_types::SelfType;

    // Perform conflict checking before standard semantic analysis errors due to multiple possible prototypes.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Apply constraints in the types.
    for (auto const &param : FnParamGroup->GetAllParams()) {
        param->Type->ResetCache();
        param->Type->Stage7_AnalyseSemantics(sm, meta);
    }
    ReturnType->ResetCache();
    ReturnType->Stage7_AnalyseSemantics(sm, meta);

    // New version
    if (const auto self_param = FnParamGroup->GetSelfParam()) {
        const auto self_sym = sm->CurrentScope->GetVarSymbol(MakeShared<IdentifierAst>(0, "self"));
        const auto self_conv = self_param->Conv.get();

        self_sym->Type = ResolveAndSubstituteSelfType(*self_sym->Type, *sm->CurrentScope, *sm, *meta)->WithConvention(AstClone(self_conv));

        for (auto const &param : FnParamGroup->GetAllParams()) {
            const auto var_sym = sm->CurrentScope->GetVarSymbol(param->ExtractName());
            if (var_sym == nullptr) { continue; } // Destructuring parameters.
            var_sym->Type = ResolveAndSubstituteSelfType(*var_sym->Type, *sm->CurrentScope, *sm, *meta);
        }
    }

    // If this is a !compiler_builtin function, swap in the lowered implementation now (stage 6), so that the lowered
    // (comptime) body is available to any dependent Stage9_CompTimeResolve regardless of definition order. Doing this in
    // Stage7 made it order-dependent: a comptime call site (eg `[a; 1_uz + 2_uz]` lowering to `intrinsics::add`) could be
    // resolved before the target prototype's Stage7 had run, leaving no lowered impl to evaluate.
    if (BuiltinAnnotation) {
        auto scope_vec = sm->CurrentScope->ParentModule()->Ancestors()
            | genex::views::transform([](auto const &x) { return x->NameAsString(); })
            | genex::to<Vec>()
            | genex::views::reverse
            | genex::views::drop(1)
            | genex::views::intersperse(Str("::"))
            | genex::to<Vec>();
        auto scope_str = genex::fold_left_first(scope_vec, std::plus<Str>{});
        scope_str.append("::").append(Name->Val);

        auto lowered_impl = FunctionImplementationLoweredAst::NewEmpty();
        lowered_impl->SetScopePtr(scope_str);
        Impl = std::move(lowered_impl);
    }

    // if (not analyse::utils::builtins::BUILTIN_FUNCS.contains(scope_str)) {
    //     analyse::errors::SemanticErrorBuilder<analyse::errors::SppInternalCompilerError>()
    //         .with_args(*no_impl_annotation, "compiler_builtin function '" + scope_str + "' is not registered in BUILTIN_FUNCS")
    //         .raises_from(sm->CurrentScope);
    // }

    // if (tok_cmp != nullptr and analyse::utils::builtins::BUILTIN_FUNCS.at(scope_str).cmp_fn == nullptr) {
    //     analyse::errors::SemanticErrorBuilder<analyse::errors::SppInternalCompilerError>()
    //         .with_args(*tok_cmp, "compiler_builtin function '" + scope_str + "' missing builtin comptime implementation")
    //         .raises_from(sm->CurrentScope);
    // }

    // Move out of the function scope, as it is now complete.
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::FunctionPrototypeAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::type_utils::IsTypeBorrowed;

    // Move into the function scope, as it is now ready for semantic analysis.
    sm->MoveToNextScope();

    // SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }

    // Repeated convention check for generic substitutions.
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*ReturnType, *sm),
        {sm->CurrentScope}, ERR_ARGS(*ReturnType, *ReturnType, "function return type"));

    // Analyse the generic parameter group, and the parameter group.
    GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

    // Note: the !compiler_builtin lowered-implementation swap now happens in Stage6_PreAnalyseSemantics, so that the
    // lowered comptime body is available to dependent Stage9_CompTimeResolve calls regardless of definition order.

    // There is no scope exit, as subclasses will call this method, and finish the analysis themselves.
}

auto spp::asts::FunctionPrototypeAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the function scope, as it is now ready for memory checking.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check the memory for the parameter group and implementation.
    FnParamGroup->Stage8_CheckMemory(sm, meta);
    Impl->Stage8_CheckMemory(sm, meta);

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
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the declaration, but not the definition, of the function. This allows for order-agnostic behaviour.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    // Handle the main function prototype.
    GenerateLlvmDeclaration(sm, meta, ctx);

    // Handle generic substitutions of a function.
    for (auto const &[generic_scope, generic_ast] : _GenericSubstitutions) {
        auto tm = ScopeManager(sm->GlobalScope, generic_scope.get());
        generic_ast->GenerateLlvmDeclaration(&tm, meta, ctx);
    }

    // Manual scope skipping.
    const auto final_scope = sm->CurrentScope->FinalChildScope();
    while (sm->CurrentScope != final_scope) {
        sm->MoveToNextScope(false);
    }

    return nullptr;
}

auto spp::asts::FunctionPrototypeAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the function body.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    // Add the entry block to the function.
    const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry", GetLlvmFunc()->Target);
    ctx->Builder.SetInsertPoint(entry_bb);

    // Generate the parameters as variables.
    if (GetLlvmFunc()->Target != nullptr) {
        FnParamGroup->Stage11_CodeGen(sm, meta, ctx);
        GnParamGroup->Stage11_CodeGen(sm, meta, ctx);
    }

    const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType);
    meta->Save();
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;

    // If there is an implementation, generate its code.
    const auto is_extern = FfiAnnotation || AbstractAnnotation;
    if (GetLlvmFunc()->Target == nullptr) {
        // Generic base function so not generating for it.
        // Manual scope skipping.
        const auto final_scope = sm->CurrentScope->FinalChildScope();
        while (sm->CurrentScope != final_scope) {
            sm->MoveToNextScope(false);
        }
    }
    else if (BuiltinAnnotation or FfiAnnotation) {
        // Get manual IR from a codegen module.
        Impl->Stage11_CodeGen(sm, meta, ctx);
    }
    else if (not is_extern) {
        // Generate the function implementation.
        Impl->Stage11_CodeGen(sm, meta, ctx);

        // The body is an expression scope, so it never emits its own terminator. Return with void if there is no return
        // statement, otherwise we have already returned to emit an "unreachable" instruction.
        if (ctx->Builder.GetInsertBlock()->getTerminator() == nullptr) {
            GetLlvmFunc()->Target->getReturnType()->isVoidTy()
                ? static_cast<llvm::Instruction*>(ctx->Builder.CreateRetVoid())
                : static_cast<llvm::Instruction*>(ctx->Builder.CreateUnreachable());
        }
    }
    else {
        // Skip the scope, linker will provide implementation for ffi.
        Impl->Stage11_CodeGen(sm, meta, ctx);
    }

    meta->Restore();
    sm->MoveOutOfCurrentScope();

    // Analyse to make a new scope in the correct place.
    // TODO: Remove this? func_call() generates generic target when needed,
    //  Do same with generic classes & object instantiation?
    for (auto const &[generic_scope, generic_proto] : _GenericSubstitutions) {
        auto tm = ScopeManager(sm->GlobalScope, generic_scope.get());
        if (std::get<0>(generic_proto->_IsPureGeneric(&tm, ctx))) { continue; }

        auto cloned_scope_set = MakeUnique<analyse::scopes::Scope>(*_Scope->Children[0]);
        generic_scope->Children[0]->Children.push_back(std::move(cloned_scope_set));
        generic_scope->FixChildrenToParentPointer();

        tm.Reset(tm.CurrentScope);
        const auto current_scope = tm.CurrentScope;
        const auto current_iter = tm.CurrentIterator();

        generic_proto->Impl = std::move(generic_proto->Source.OriginalImpl);
        generic_proto->Stage7_AnalyseSemantics(&tm, meta);

        tm.Reset(current_scope, current_iter);
        generic_proto->Stage11_CodeGen(&tm, meta, ctx);
    }

    return nullptr;
}

auto spp::asts::FunctionPrototypeAst::GetLlvmFunc() const
    -> Shared<codegen::LlvmFuncWrapper> {
    return *_LlvmFunc;
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

auto spp::asts::FunctionPrototypeAst::RegisterGenericSubstitution(
    Unique<analyse::scopes::Scope> &&scope,
    Unique<FunctionPrototypeAst> &&new_ast)
    -> void {
    // Store the scope for object persistence (and codegen).
    _GenericSubstitutions.emplace_back(std::move(scope), std::move(new_ast));
}

auto spp::asts::FunctionPrototypeAst::RegisteredGenericSubstitutions() const
    -> std::list<Pair<analyse::scopes::Scope*, FunctionPrototypeAst*>> {
    return _GenericSubstitutions
        | genex::views::transform([](auto const &x) { return MakePair(x.First.get(), x.Second.get()); })
        | genex::to<std::list>();
}

auto spp::asts::FunctionPrototypeAst::RegisteredGenericSubstitutions()
    -> std::list<Pair<Unique<analyse::scopes::Scope>, Unique<FunctionPrototypeAst>>>& {
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
        return MakePair(FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_ref"));
    }

    // Class methods with "self" are the FunMov type.
    if (FnParamGroup->GetSelfParam()->Conv == nullptr) {
        return MakePair(FunMovType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_mov"));
    }

    // Class methods with "&mut self" are the FunMut type.
    if (*FnParamGroup->GetSelfParam()->Conv == ConventionTag::MUT) {
        return MakePair(FunMutType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_mut"));
    }

    // Class methods with "&self" are the FunRef type.
    if (*FnParamGroup->GetSelfParam()->Conv == ConventionTag::REF) {
        return MakePair(FunRefType(PosStart(), TupleType(PosStart(), std::move(param_types)), ReturnType), Str("call_ref"));
    }

    std::unreachable();
}

auto spp::asts::FunctionPrototypeAst::_IsPureGeneric(
    analyse::scopes::ScopeManager *sm,
    codegen::LLvmCtx *ctx) const
    -> std::tuple<bool, llvm::Type*, Vec<llvm::Type*>> {
    // Convert the return and parameter types to LLVM types.
    const auto llvm_ret_type = codegen::GetLlvmType(*sm->CurrentScope->GetTypeSymbol(ReturnType), ctx);
    const auto llvm_param_types = FnParamGroup->GetNonSelfParams()
        | genex::views::transform([&](auto const &x) { return codegen::GetLlvmType(*sm->CurrentScope->GetTypeSymbol(x->Type), ctx); })
        | genex::to<Vec>();

    // Check if any of the types failed to convert.
    const auto is_generic = llvm_ret_type != nullptr and genex::all_of(llvm_param_types, [](auto const &x) { return x != nullptr; });
    return std::make_tuple(not is_generic, llvm_ret_type, llvm_param_types);
}

SPP_MOD_END
