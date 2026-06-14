module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_extension_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
// import genex;

SPP_MOD_BEGIN
spp::asts::SupPrototypeExtensionAst::SupPrototypeExtensionAst(
    decltype(TokSup) &&tok_sup,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Name) name,
    decltype(TokExt) &&tok_ext,
    decltype(SuperClass) super_class,
    decltype(Impl) &&impl) :
    TokSup(std::move(tok_sup)),
    GnParamGroup(std::move(generic_param_group)),
    Name(std::move(name)),
    TokExt(std::move(tok_ext)),
    SuperClass(std::move(super_class)),
    Impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSup, lex::SppTokenType::KW_SUP, "sup");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokExt, lex::SppTokenType::KW_EXT, "ext");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
    Source.OriginalName = AstClone(Name);
    Source.OriginalSuperClass = AstClone(SuperClass);
}

spp::asts::SupPrototypeExtensionAst::~SupPrototypeExtensionAst() = default;

auto spp::asts::SupPrototypeExtensionAst::PosStart() const
    -> std::size_t {
    // Use the "sup" token.
    return TokSup->PosStart();
}

auto spp::asts::SupPrototypeExtensionAst::PosEnd() const
    -> std::size_t {
    // Use the superclass.
    return Source.OriginalSuperClass->PosEnd();
}

auto spp::asts::SupPrototypeExtensionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<SupPrototypeExtensionAst>(
        AstClone(TokSup), AstClone(GnParamGroup), AstClone(Name), AstClone(TokExt), AstClone(SuperClass),
        AstClone(Impl));
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    return ast;
}

auto spp::asts::SupPrototypeExtensionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokSup).append(" ");
    SPP_STRING_APPEND(GnParamGroup).append(GnParamGroup->Params.IsEmpty() ? "" : " ");
    SPP_STRING_APPEND(Name).append(" ");
    SPP_STRING_APPEND(TokExt).append(" ");
    SPP_STRING_APPEND(SuperClass).append(" ");
    SPP_STRING_APPEND(Impl);
    SPP_STRING_END;
}

auto spp::asts::SupPrototypeExtensionAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Don't need to pre-process function-classes -- they are introduced from preprocessing functions.
    if (Name->IsCompilerGeneratedType()) { return; }
    Ast::Stage1_PreProcess(ctx);

    // Preprocess the implementation.
    Impl->Stage1_PreProcess(this);

    // Todo: some sort of check that prevents something like "sup [T] T ext Borrow[T]", because this is infinite generation
}

auto spp::asts::SupPrototypeExtensionAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSuperimpositionOptionalGenericParameterError;
    using analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError;

    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "sup-prototype-extension", {Name.get(), SuperClass.get()}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    // Check there are optional generic parameters.
    const auto optional = GnParamGroup->GetOptionalParams();
    RaiseIf<SppSuperimpositionOptionalGenericParameterError>(
        not optional.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*optional[0]));

    // Check every generic parameter is constrained by the type.
    if (not Name->IsCompilerGeneratedType()) {
        const auto unconstrained = GnParamGroup->GetAllParams()
            | std::ranges::views::filter([this](auto const &x) { return not(Name->ContainsGenerics(*x) or SuperClass->ContainsGenerics(*x)); })
            | std::ranges::to<Vec>();
        RaiseIf<SppSuperimpositionUnconstrainedGenericParameterError>(
            not unconstrained.IsEmpty(), {sm->CurrentScope},
            ERR_ARGS(*unconstrained[0]));
    }

    // Generate symbols for the generic parameter group, and the self type.
    GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    Impl->Stage2_GenTopLvlScopes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage3_GenTopLvlAliases(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    GnParamGroup->Stage4_QualifyTypes(sm, meta);
    Impl->Stage4_QualifyTypes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::IsTypeBorrowed;
    using analyse::errors::SppGenericTypeInvalidUsageError;
    using analyse::errors::SppSecondClassBorrowViolationError;

    // Move into the superimposition scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Analyse the type being superimposed over.
    Name->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Name, *sm),
        {sm->CurrentScope}, ERR_ARGS(*this, *Name, "superimposition type"));
    Name = sm->CurrentScope->GetTypeSymbol(Name)->FqName();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics());
    if (sm->CurrentScope->Parent == sm->CurrentScope->ParentModule()) {
        if (not base_cls_sym->IsGeneric) {
            analyse::scopes::ScopeManager::normal_sup_blocks[base_cls_sym.get()].EmplaceBack(sm->CurrentScope);
        }
        else {
            analyse::scopes::ScopeManager::generic_sup_blocks.EmplaceBack(sm->CurrentScope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (not Name->IsCompilerGeneratedType()) {
        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
        const auto self_sym = MakeShared<analyse::scopes::TypeSymbol>(
            MakeUnique<TypeIdentifierAst>(Name->PosStart(), "Self", nullptr),
            sm->SelfProto(), cls_sym->LinkedScope, sm->CurrentScope);
        sm->CurrentScope->AddTypeSymbol(self_sym);
    }

    // Analyse the supertype after Self has been added (allows use in generic arguments to the superclass).
    SuperClass->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*SuperClass, *sm),
        {sm->CurrentScope}, ERR_ARGS(*this, *SuperClass, "superimposition supertype"));
    SuperClass = sm->CurrentScope->GetTypeSymbol(SuperClass)->FqName();

    // Check the supertype is not generic.
    const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass);
    RaiseIf<SppGenericTypeInvalidUsageError>(
        sup_sym->IsGeneric, {sm->CurrentScope},
        ERR_ARGS(*SuperClass, *Source.OriginalSuperClass, "superimposition supertype"));

    // Load the implementation and move out of the scope.
    Impl->Stage5_LoadSupScopes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::func_utils::CheckForConflictingOverride;
    using analyse::utils::type_utils::TypeEq;
    using analyse::errors::SppSuperimpositionExtensionMethodInvalidError;
    using analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError;
    using analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError;
    using analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError;
    using generate::common_types_precompiled::COPY;

    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Get the symbols.
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
    const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass);

    auto sup_scopes = sm->CurrentScope->GetTypeSymbol(SuperClass)->LinkedScope->SupScopes();
    sup_scopes.Insert(sup_scopes.begin(), sup_sym->LinkedScope);
    sup_scopes.Erase(
        std::ranges::remove_if(sup_scopes, [](auto const &x) { return x->AstNode->template To<ClassPrototypeAst>() == nullptr; }).begin(),
        sup_scopes.end());

    // Mark the class as copyable if the "Copy" type is the supertype.
    for (const auto sup_scope : sup_scopes) {
        const auto fq_name = sup_scope->TySym->FqName();
        if (TypeEq(*fq_name, *COPY, *sup_scope, *sm->CurrentScope)) {
            sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics())->IsDirectlyCopyable = true;
            cls_sym->IsDirectlyCopyable = true;
            break;
        }
    }

    // Check every member on the superimposition exists on the supertype.
    for (auto const &member : Impl->Members) {
        if (const auto ext_member = member->To<SupPrototypeExtensionAst>()) {
            // Get the method and identify the base method it is overriding.
            const auto this_method = ext_member->Impl->FinalMember()->To<FunctionPrototypeAst>();
            const auto base_method = CheckForConflictingOverride(
                *member->GetAstScope(), sup_sym->LinkedScope, *this_method, *sm, meta);

            // Check the base method exists.
            RaiseIf<SppSuperimpositionExtensionMethodInvalidError>(
                base_method == nullptr, {sm->CurrentScope},
                ERR_ARGS(*this_method->Name, *Source.OriginalSuperClass));

            // Check the base method is virtual or abstract.
            RaiseIf<SppSuperimpositionExtensionNonVirtualMethodOverriddenError>(
                not(base_method->AbstractAnnotation or base_method->VirtualAnnotation), {sm->CurrentScope},
                ERR_ARGS(*this_method->Name, *base_method->Name, *Source.OriginalSuperClass));

            // Sync up the annotations from the base function.
            // Todo: Once "inheriting" annotations is supported at definition, do it dynamically.
            this_method->Visibility = base_method->Visibility;
            const auto func_sym = sm->CurrentScope->GetVarSymbol(this_method->Name, true);
            func_sym->Visibility = base_method->Visibility.First;
            func_sym->VisibilityAnnotation = this_method->Visibility.Second;
        }

        else if (const auto type_member = member->To<TypeStatementAst>()) {
            // Get the associated type from the supertype directly.
            const auto this_type = type_member->NewType;
            const auto base_type = sup_sym->LinkedScope->GetTypeSymbol(this_type, true);

            // Check to see if the base type exists.
            RaiseIf<SppSuperimpositionExtensionTypeStatementInvalidError>(
                base_type == nullptr, {member->GetAstScope(), sm->CurrentScope},
                ERR_ARGS(*type_member, *Source.OriginalSuperClass));
        }

        else if (const auto cmp_member = member->To<CmpStatementAst>()) {
            // Get the associated cmp from the supertype directly.
            const auto this_const = cmp_member->Name;
            const auto base_const = sup_sym->LinkedScope->GetVarSymbol(this_const, true);

            // Check to see if the base cmp exists.
            RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
                base_const == nullptr, {sm->CurrentScope},
                ERR_ARGS(*cmp_member, *Source.OriginalSuperClass));

            // Check to see if the base cmp's type is the same as this one.
            RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
                not TypeEq(*base_const->Type, *cmp_member->Type, *member->GetAstScope(), *sm->CurrentScope, false),
                {member->GetAstScope(), sm->CurrentScope},
                ERR_ARGS(*cmp_member, *Source.OriginalSuperClass));
        }
    }

    // Pre-analyse the implementation.
    Impl->Stage6_PreAnalyseSemantics(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::func_utils::EnforceGenericConstraintsAllArgs;

    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Re-map "Self" to the true type.
    if (not Name->IsCompilerGeneratedType()) {
        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
        const auto self_sym = sm->CurrentScope->GetTypeSymbol(MakeUnique<TypeIdentifierAst>(Name->PosStart(), "Self", nullptr), true);
        self_sym->Type = cls_sym->Type;
        cls_sym->AliasedBySyms.EmplaceBack(self_sym);
    }

    GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

    Name->ResetCache();
    Name->Stage7_AnalyseSemantics(sm, meta);
    const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
    if (cls_sym->Type) EnforceGenericConstraintsAllArgs(
        *cls_sym->Type->GnParamGroup, *GenericArgumentGroupAst::FromParams(*GnParamGroup), *sm->CurrentScope, *sm, *meta);

    SuperClass->ResetCache();
    SuperClass->Stage7_AnalyseSemantics(sm, meta);
    const auto sup_sym = sm->CurrentScope->GetTypeSymbol(SuperClass);
    if (cls_sym->Type) EnforceGenericConstraintsAllArgs(
        *sup_sym->Type->GnParamGroup, *GenericArgumentGroupAst::FromParams(*GnParamGroup), *sm->CurrentScope, *sm, *meta);

    Impl->Stage7_AnalyseSemantics(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage8_CheckMemory(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage9_CompTimeResolve(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeExtensionAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage10_PreCodeGen(sm, meta, ctx);
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::SupPrototypeExtensionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check if this block is purely generic.
    const auto is_generic_scope =
        std::ranges::any_of(sm->CurrentScope->AllTypeSymbols(true), [](auto const &x) { return x->IsGeneric; }) or
        std::ranges::any_of(sm->CurrentScope->AllVarSymbols(true), [](auto const &x) { return x->MemInfo->AstCompTime == nullptr; });

    // Generate the implementation if not a generic scope.
    if (not is_generic_scope) {
        Impl->Stage11_CodeGen(sm, meta, ctx);
    }

    // Generic sup block so not generating for it.
    // Manual scope skipping.
    else {
        const auto final_scope = sm->CurrentScope->FinalChildScope();
        while (sm->CurrentScope != final_scope) {
            sm->MoveToNextScope(false);
        }
    }

    sm->MoveOutOfCurrentScope();

    return nullptr;
}

auto spp::asts::SupPrototypeExtensionAst::CheckCyclicExtension(
    analyse::scopes::TypeSymbol const &sup_sym,
    analyse::scopes::Scope &check_scope) const
    -> void {
    //
    using analyse::errors::SppSuperimpositionCyclicExtensionError;
    using analyse::utils::type_utils::GenericInferenceMap;
    using analyse::utils::type_utils::RelaxedTypeEq;
    using analyse::utils::type_utils::TypeEq;

    //
    auto check_cycle = [this, &check_scope](analyse::scopes::Scope const *sc) {
        auto dummy = GenericInferenceMap();
        const auto ext = sc->AstNode->To<SupPrototypeExtensionAst>();
        return ext and
            RelaxedTypeEq(*ext->Name, *SuperClass, *sc, check_scope, dummy, false) and
            TypeEq(*ext->SuperClass, *Name, *sc, check_scope, false);
    };

    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    const auto existing_sup_scopes = sup_sym.LinkedScope->SupScopes()
        | std::ranges::views::filter(check_cycle)
        | std::ranges::views::transform([](auto *x) { return MakePair(x, x->AstNode->template To<SupPrototypeExtensionAst>()); })
        | std::ranges::to<Vec>();

    RaiseIf<SppSuperimpositionCyclicExtensionError>(
        not existing_sup_scopes.IsEmpty(), {&check_scope},
        ERR_ARGS(*existing_sup_scopes[0].Second->Source.OriginalSuperClass, *Source.OriginalSuperClass));
}

auto spp::asts::SupPrototypeExtensionAst::CheckDoubleExtension(
    analyse::scopes::TypeSymbol const &cls_sym,
    analyse::scopes::Scope &check_scope) const
    -> void {
    // Early return for function-classes.
    using analyse::utils::type_utils::GenericInferenceMap;
    using analyse::utils::type_utils::RelaxedTypeEq;
    using analyse::utils::type_utils::TypeEq;
    using analyse::errors::SppSuperimpositionDoubleExtensionError;
    if (cls_sym.Name->IsCompilerGeneratedType()) { return; }

    auto check_double = [this, &check_scope](analyse::scopes::Scope const *sc) {
        auto dummy = GenericInferenceMap();
        const auto ext = sc->AstNode->To<SupPrototypeExtensionAst>();
        return ext != nullptr and
            RelaxedTypeEq(*ext->Name, *Name, *sc, check_scope, dummy, false, false) and
            TypeEq(*ext->SuperClass, *SuperClass, *sc, check_scope, false);
    };

    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    auto all_sup_scopes = cls_sym.LinkedScope->SupScopes();
    const auto existing_sup_scopes = all_sup_scopes
        | std::ranges::views::filter(check_double)
        | std::ranges::views::transform([](auto *x) { return MakePair(x, x->AstNode->template To<SupPrototypeExtensionAst>()); })
        | std::ranges::to<Vec>();

    RaiseIf<SppSuperimpositionDoubleExtensionError>(
        not existing_sup_scopes.IsEmpty(), {&check_scope},
        ERR_ARGS(*existing_sup_scopes[0].Second->Source.OriginalSuperClass, *Source.OriginalSuperClass));
}

auto spp::asts::SupPrototypeExtensionAst::CheckSelfExtension(
    analyse::scopes::Scope &check_scope) const
    -> void {
    //
    using analyse::errors::SppSuperimpositionSelfExtensionError;
    using analyse::utils::type_utils::TypeEq;

    // Check if the superimposition is extending itself.
    RaiseIf<SppSuperimpositionSelfExtensionError>(
        TypeEq(*Name, *SuperClass, check_scope, check_scope), {&check_scope},
        ERR_ARGS(*Source.OriginalName, *Source.OriginalSuperClass));
}

SPP_MOD_END
