module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.subroutine_prototype_ast;
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
import spp.asts.generic_parameter_group_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::SubroutinePrototypeAst::SubroutinePrototypeAst(
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
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_FUN, "fun");
}

spp::asts::SubroutinePrototypeAst::~SubroutinePrototypeAst() = default;

auto spp::asts::SubroutinePrototypeAst::Clone() const
    -> Unique<Ast> {
    auto ast = MakeUnique<SubroutinePrototypeAst>(
        AstCloneVec(Annotations), AstClone(TokCmp), AstClone(TokFun), AstClone(Name), AstClone(GnParamGroup),
        AstClone(FnParamGroup), AstClone(TokArrow), AstClone(ReturnType), AstClone(Impl));
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

auto spp::asts::SubroutinePrototypeAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types_precompiled::VOID;
    using generate::common_types_precompiled::NEVER;

    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
    const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType);

    // Update the meta information for enclosing function information.
    meta->Save();
    meta->EnclosingFunctionFlavour = this->TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;
    meta->EnclosingFunctionCmp = TokCmp.get();
    Impl->Stage7_AnalyseSemantics(sm, meta);

    // Handle the "!" never type.
    auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope->Children[0].get());
    meta->Save();
    meta->IgnoreMissingElseBranchForInference = true;
    const auto is_never = not Impl->Members.IsEmpty() and TypeEq(
        *Impl->FinalMember()->To<StatementAst>()->InferType(&tm, meta), *NEVER,
        *tm.CurrentScope, *sm->CurrentScope);
    meta->Restore();

    // Check there is a return statement at the end (for non-void functions).
    const auto is_void = TypeEq(
        *ReturnType, *VOID, *sm->CurrentScope, *sm->CurrentScope);

    const auto final_member = Impl->FinalMember();
    RaiseUnless<analyse::errors::SppFunctionSubroutineMissingReturnStatementError>(
        is_void or is_never or FfiAnnotation or BuiltinAnnotation or AbstractAnnotation or (not Impl->Members.IsEmpty() and Impl->Members.Back()->To<RetStatementAst>()),
        {sm->CurrentScope}, ERR_ARGS(*final_member, *Source.OriginalReturnType, *ReturnType));

    sm->MoveOutOfCurrentScope();
    meta->Restore(true);
    meta->LoopReturnTypes->clear();
}

auto spp::asts::SubroutinePrototypeAst::IsCoroutine() const
    -> bool {
    return false;
}

SPP_MOD_END
