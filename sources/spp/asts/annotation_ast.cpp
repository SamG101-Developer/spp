module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.asts.annotation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.module_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.parse.parser_spp;
import spp.lex.lexer;

SPP_MOD_BEGIN
spp::asts::AnnotationAst::AnnotationAst(
    decltype(TokExclamationMark) &&tok_exclamation_mark,
    decltype(Name) &&name,
    decltype(GnArgGroup) &&gn_arg_group,
    decltype(FnArgGroup) &&fn_arg_group) :
    TokExclamationMark(std::move(tok_exclamation_mark)),
    Name(std::move(name)),
    GnArgGroup(std::move(gn_arg_group)),
    FnArgGroup(std::move(fn_arg_group)),
    _Target(nullptr) {
    // Default the two optional argument groups.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

spp::asts::AnnotationAst::~AnnotationAst() = default;

auto spp::asts::AnnotationAst::PosStart() const
    -> std::size_t {
    // Use the "!" token.
    return TokExclamationMark->PosStart();
}

auto spp::asts::AnnotationAst::PosEnd() const
    -> std::size_t {
    // Use the "name" token.
    return Name->PosEnd();
}

auto spp::asts::AnnotationAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<AnnotationAst>(
        AstClone(TokExclamationMark), AstClone(Name), AstClone(GnArgGroup), AstClone(FnArgGroup));
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    ast->_Target = _Target;
    return ast;
}

auto spp::asts::AnnotationAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokExclamationMark);
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(GnArgGroup);
    SPP_STRING_APPEND(FnArgGroup);
    SPP_STRING_END;
}

auto spp::asts::AnnotationAst::operator==(
    AnnotationAst const &that) const
    -> bool {
    // Annotation equality is based on the name.
    return *Name == *that.Name;
}

auto spp::asts::AnnotationAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Default AST processing (sets context).
    Ast::Stage1_PreProcess(ctx);
}

auto spp::asts::AnnotationAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Default AST processing (sets scope).
    Ast::Stage2_GenTopLvlScopes(sm, meta);
}

auto spp::asts::AnnotationAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Special annotation handling.
    const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*Name).First;
    if (sym == nullptr) { return; }

    const auto fq_name = sym->FqName()->ToString();
    const auto func_ctx = _Ctx->To<FunctionPrototypeAst>();

    // If this is a "!annotation" annotation, mark it.
    if (fq_name == "std::annotations::annotation") {
        RaiseIf<analyse::errors::SppAnnotationTargetNotACmpFunctionError>(
            not(func_ctx and func_ctx->TokCmp), {_Scope},
            ERR_ARGS(*this, *_Ctx));
        func_ctx->MarkAsAnnotation();
        func_ctx->GetAnnotationInfo()->Definition = this;
    }

    // If this is a "!compiler_builtin" annotation, mark it.
    // if (fq_name == "std::annotations::compiler_builtin") {
    //     RaiseIf<analyse::errors::SppAnnotationTargetNotACmpFunctionError>(
    //         not func_ctx, {m_scope},
    //         ERR_ARGS(*this, *_Ctx));
    //     func_ctx->builtin_annotation = true;
    // }
}

auto spp::asts::AnnotationAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Handle builtin annotations.
    using A = analyse::utils::annotation_utils::BuiltinAnnotations;

    meta->Save();
    meta->IgnoreAccessModifierViolations = true;
    if (const auto pf = Name->To<PostfixExpressionAst>(); pf and pf->Op->To<PostfixExpressionOperatorFunctionCallAst>()) {
        pf->Lhs->Stage7_AnalyseSemantics(sm, meta);
    }
    else {
        Name->Stage7_AnalyseSemantics(sm, meta);
    }
    meta->Restore();

    const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*Name).First;
    const auto fq_name = sym->FqName()->ToString();

    // For the known builtin annotations, they will attempt to modify their contextual objects if possible, for required
    // behaviour like virtual and abstract tags on function prototypes. If the context is incorrect, nothing happens,
    // and a later stage will pickup the error, as there is a unified target-checking mechanism (custom and builtin).
    if (fq_name == A::kIntrinsic) {
        const auto func_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (func_ctx) { func_ctx->BuiltinAnnotation = this; }
    }

    // Mark a visibility-enabled ast as having "public" visibility.
    else if (fq_name == A::kPublic) {
        const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->Visibility = MakePair(utils::Visibility::kPublic, this); }
    }

    // Mark a visibility-enabled ast as having "package" visibility.
    else if (fq_name == A::kPackage) {
        const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->Visibility = MakePair(utils::Visibility::kPackage, this); }
    }

    // Mark a visibility-enabled ast as having "protected" visibility.
    else if (fq_name == A::kProtected) {
        const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->Visibility = MakePair(utils::Visibility::kProtected, this); }
    }

    // Mark a visibility-enabled ast as having "private" visibility.
    else if (fq_name == A::kPrivate) {
        const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>();
        if (vis_ctx) { vis_ctx->Visibility = MakePair(utils::Visibility::kPrivate, this); }
    }

    // Mark a method ast as being "virtual", enabling overriding.
    else if (fq_name == A::kVirtualMethod) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->VirtualAnnotation = this; }
    }

    // Mark a method ast as being "abstract", requiring overriding.
    else if (fq_name == A::kAbstractMethod) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->AbstractAnnotation = this; }
    }

    // Mark a function ast as being "ffi", providing a linkage name.
    else if (fq_name == A::kFfi) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->FfiAnnotation = this; }
        if (fun_ctx) { fun_ctx->Visibility = MakePair(utils::Visibility::kPublic, this); }
    }

    // Mark a type symbol as being "zero type".
    else if (fq_name == A::kZeroType and _Ctx->To<ClassPrototypeAst>()) {
        const auto cls_ctx = _Ctx->To<ClassPrototypeAst>();
        const auto type_sym = sm->CurrentScope->GetTypeSymbol(cls_ctx->Name->WithoutGenerics());
        type_sym->IsDirectlyZeroType = true;
    }
    else if (fq_name == A::kZeroType and _Ctx->To<TypeStatementAst>()) {
        const auto cls_ctx = _Ctx->To<TypeStatementAst>();
        sm->CurrentScope->GetTypeSymbol(cls_ctx->NewType->WithoutGenerics())->IsDirectlyZeroType = true;
        sm->CurrentScope->GetTypeSymbol(cls_ctx->OldType)->IsDirectlyZeroType = true;
    }

    // Mark a function as being inlinable via llvm.
    else if (fq_name == A::kLlvmInline) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->InlineAnnotation = this; }
    }

    // Mark a function as being always inlined via llvm.
    else if (fq_name == A::kLlvmAlwaysInline) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->InlineAnnotation = this; }
    }

    // Mark a function as being never inlined via llvm.
    else if (fq_name == A::kLlvmNoInline) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->InlineAnnotation = this; }
    }

    // Mark a function as being "hot" via llvm.
    else if (fq_name == A::kLlvmHot) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->TemperatureAnnotation = this; }
    }

    // Mark a function as being "cold" via llvm.
    else if (fq_name == A::kLlvmCold) {
        const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
        if (fun_ctx) { fun_ctx->TemperatureAnnotation = this; }
    }

    // TODO: "extends", "variant_default"
}

auto spp::asts::AnnotationAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Todo: Validate "Void" return type on annotation + test.

    // Convert the target into a function call to ensure it exists.
    auto fn = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
        std::move(GnArgGroup), std::move(FnArgGroup), nullptr);
    fn->Source.OriginalExpr = this;
    const auto fn_ptr = fn.get();
    const auto pf = MakeUnique<PostfixExpressionAst>(AstClone(Name), std::move(fn));
    pf->Stage7_AnalyseSemantics(sm, meta);

    // Restore the function and generic arguments.
    GnArgGroup = std::move(fn_ptr->GnArgGroup);
    FnArgGroup = std::move(fn_ptr->FnArgGroup);

    // Check the target function is an annotation (via "!annotation").
    const auto overload = fn_ptr->Target();
    RaiseIf<analyse::errors::SppAnnotationTargetNotAnAnnotationError>(
        not overload->GetAnnotationInfo(),
        {_Scope}, ERR_ARGS(*this, *overload));

    _Target = overload;
}

auto spp::asts::AnnotationAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load up different construct casts that an annotation may apply to.
    using analyse::utils::annotation_utils::AnnotationInfo;
    using analyse::errors::SppCalledAnnotationAppliedToInvalidAstError;

    const auto annotation_info = _Target->GetAnnotationInfo();
    const auto outer_mod_ctx = _Ctx->GetAstCtx()->To<ModulePrototypeAst>();
    const auto outer_sup_ctx = _Ctx->GetAstCtx()->To<SupPrototypeFunctionsAst>();
    const auto outer_ext_ctx = _Ctx->GetAstCtx()->To<SupPrototypeExtensionAst>();

    // Evaluate the context that this annotation can be applied to.
    meta->Save();
    const auto annotation_scope_name = INJECT_CODE("std::annotations", parse_expression);
    const auto annotation_scope = const_cast<analyse::scopes::Scope*>(
        sm->CurrentScope->ConvertPostfixToNestedScope(annotation_scope_name.get()));
    auto tm = ScopeManager(sm->GlobalScope, annotation_scope);
    annotation_info->Definition->FnArgGroup->At("target")->Val->Stage9_CompTimeResolve(&tm, meta);
    const auto result = std::move(meta->CmpResult);
    const auto allowed_ctx = result->To<IntegerLiteralAst>()->CppVal<std::uint64_t>();
    meta->Restore();

    auto target = annotation_info->Definition->FnArgGroup->At("target");

    // Error for incompatible asts when classes are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<ClassPrototypeAst>() and not(allowed_ctx & AnnotationInfo::kClassContext),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

    // Error for incompatible asts when free functions are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<FunctionPrototypeAst>() and outer_mod_ctx and not(allowed_ctx & AnnotationInfo::kFunctionCtx),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

    // Error for incompatible asts when methods are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<FunctionPrototypeAst>() and outer_sup_ctx and not(allowed_ctx & AnnotationInfo::kMethodCtx),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

    // Error for incompatible asts when overriding methods are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<FunctionPrototypeAst>() and outer_ext_ctx and not(allowed_ctx & AnnotationInfo::kExtensionContext),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

    // Error for incompatible asts when type statements are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<TypeStatementAst>() and not(allowed_ctx & AnnotationInfo::kTypeStmtCtx),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

    // Error for incompatible asts when cmp statements are not valid targets.
    RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
        _Ctx->To<CmpStatementAst>() and not(allowed_ctx & AnnotationInfo::kCmpStmtCtx),
        {sm->CurrentScope, annotation_scope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));
}

SPP_MOD_END
