module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <utility>

module spp.asts.sup_prototype_functions_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
    decltype(TokSup) &&tok_sup,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(Name) name,
    decltype(Impl) &&impl) :
    TokSup(std::move(tok_sup)),
    GnParamGroup(std::move(generic_param_group)),
    Name(std::move(name)),
    Impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSup, lex::SppTokenType::KW_SUP, "sup");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
    Source.OriginalName = AstClone(Name);
}

spp::asts::SupPrototypeFunctionsAst::~SupPrototypeFunctionsAst() = default;

auto spp::asts::SupPrototypeFunctionsAst::PosStart() const
    -> std::size_t {
    // Use the "sup" token.
    return TokSup->PosStart();
}

auto spp::asts::SupPrototypeFunctionsAst::PosEnd() const
    -> std::size_t {
    // Use the name.
    return Source.OriginalName->PosEnd();
}

auto spp::asts::SupPrototypeFunctionsAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<SupPrototypeFunctionsAst>(
        AstClone(TokSup), AstClone(GnParamGroup), AstClone(Name), AstClone(Impl));
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    return ast;
}

auto spp::asts::SupPrototypeFunctionsAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokSup).append(" ");
    SPP_STRING_APPEND(GnParamGroup).append(GnParamGroup->Params.IsEmpty() ? "" : " ");
    SPP_STRING_APPEND(Name).append(" ");
    SPP_STRING_APPEND(Impl);
    SPP_STRING_END;
}

auto spp::asts::SupPrototypeFunctionsAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing the implementation.
    Ast::Stage1_PreProcess(ctx);
    Impl->Stage1_PreProcess(this);
}

auto spp::asts::SupPrototypeFunctionsAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSuperimpositionOptionalGenericParameterError;
    using analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError;

    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "sup-prototype-functions", {Name.get()}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
    Ast::Stage2_GenTopLvlScopes(sm, meta);

    // Check there are optional generic parameters.
    const auto optional = GnParamGroup->GetOptionalParams();
    RaiseIf<SppSuperimpositionOptionalGenericParameterError>(
        not optional.IsEmpty(), {sm->CurrentScope}, ERR_ARGS(*optional[0]));

    // Check every generic parameter is constrained by the type.
    const auto unconstrained = GnParamGroup->GetAllParams()
        | genex::views::filter([this](auto const &x) { return not Name->ContainsGenerics(*x); })
        | genex::to<Vec>();
    RaiseIf<SppSuperimpositionUnconstrainedGenericParameterError>(
        not unconstrained.IsEmpty(), {sm->CurrentScope}, ERR_ARGS(*unconstrained[0]));

    // Generate symbols for the generic parameter group, and the self type.
    GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    Impl->Stage2_GenTopLvlScopes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage3_GenTopLvlAliases(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage4_QualifyTypes(
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

auto spp::asts::SupPrototypeFunctionsAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::type_utils::IsTypeBorrowed;

    // Move into the superimposition scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Analyse the type being superimposed over.
    Name->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Name, *sm),
        {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalName, "superimposition type"));
    Name = sm->CurrentScope->GetTypeSymbol(Name)->FqName();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->CurrentScope->GetTypeSymbol(Name->WithoutGenerics());
    if (sm->CurrentScope->Parent == sm->CurrentScope->ParentModule()) {
        if (not base_cls_sym->IsGeneric) {
            ScopeManager::normal_sup_blocks[base_cls_sym.get()].EmplaceBack(sm->CurrentScope);
        }
        else {
            ScopeManager::generic_sup_blocks.EmplaceBack(sm->CurrentScope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (not Name->IsCompilerGeneratedType()) {
        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(Name);
        const auto self_sym = MakeShared<analyse::scopes::TypeSymbol>(
            MakeUnique<TypeIdentifierAst>(Name->PosStart(), "Self", nullptr),
            cls_sym->Type, cls_sym->LinkedScope, sm->CurrentScope);
        self_sym->AliasStmt = MakeUnique<TypeStatementAst>(
            SPP_NO_ANNOTATIONS, nullptr,
            TypeIdentifierAst::FromString("Self"), nullptr, nullptr, Name);
        sm->CurrentScope->AddTypeSymbol(self_sym);
        sm->CurrentScope->GetTypeSymbol(Name)->AliasedBySyms.EmplaceBack(self_sym);
    }

    // Load the implementation and move out of the scope.
    Impl->Stage5_LoadSupScopes(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Name->Stage7_AnalyseSemantics(sm, meta);
    Impl->Stage6_PreAnalyseSemantics(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    GnParamGroup->Stage7_AnalyseSemantics(sm, meta);
    Name->Stage7_AnalyseSemantics(sm, meta);
    Impl->Stage7_AnalyseSemantics(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage8_CheckMemory(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    Impl->Stage9_CompTimeResolve(sm, meta);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::SupPrototypeFunctionsAst::Stage10_PreCodeGen(
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

auto spp::asts::SupPrototypeFunctionsAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check if this block is purely generic.
    const auto is_generic_scope =
        genex::any_of(sm->CurrentScope->AllTypeSymbols(true), [](auto const &x) { return x->IsGeneric; }) or
        genex::any_of(sm->CurrentScope->AllVarSymbols(true), [](auto const &x) { return x->MemInfo->AstCompTime == nullptr; });

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

SPP_MOD_END
