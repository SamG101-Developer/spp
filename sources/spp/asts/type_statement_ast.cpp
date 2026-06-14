module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_statement_ast;
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
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::TypeStatementAst::TypeStatementAst(
    decltype(Annotations) &&annotations,
    decltype(TokType) &&tok_type,
    decltype(NewType) new_type,
    decltype(GnParamGroup) &&generic_param_group,
    decltype(TokAssign) &&tok_assign,
    decltype(OldType) old_type) :
    MappedOldType(old_type),
    Annotations(std::move(annotations)),
    TokType(std::move(tok_type)),
    NewType(std::move(new_type)),
    GnParamGroup(std::move(generic_param_group)),
    TokAssign(std::move(tok_assign)),
    OldType(std::move(old_type)),
    _AliasSym(nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokType, lex::SppTokenType::KW_TYPE, "type");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
    Source.OriginalOldType = AstClone(OldType);
}

spp::asts::TypeStatementAst::~TypeStatementAst() {
    CleanUp();
}

auto spp::asts::TypeStatementAst::PosStart() const
    -> std::size_t {
    // Use the "type" token.
    return TokType->PosStart();
}

auto spp::asts::TypeStatementAst::PosEnd() const
    -> std::size_t {
    // Use the old type.
    return Source.OriginalOldType->PosEnd();
}

auto spp::asts::TypeStatementAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<TypeStatementAst>(
        AstCloneVec(Annotations), AstClone(TokType), NewType, AstClone(GnParamGroup), AstClone(TokAssign), OldType);
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    ast->_TrackingScope = _TrackingScope;
    ast->_FromUseStatement = _FromUseStatement;
    ast->MappedOldType = MappedOldType;
    ast->Visibility = Visibility;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::TypeStatementAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(TokType).append(" ");
    SPP_STRING_APPEND(NewType);
    SPP_STRING_APPEND(GnParamGroup).append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_APPEND(OldType);
    SPP_STRING_END;
}

auto spp::asts::TypeStatementAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::Stage1_PreProcess(ctx);
    for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
}

auto spp::asts::TypeStatementAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run top level scope generation for the annotations.
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::type_utils::IsTypeBorrowed;
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Check there are no conventions on the new type. Todo: Move to later stage? nothing is loaded in atm
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*NewType, *sm, false),
        {sm->CurrentScope}, ERR_ARGS(*this, *NewType, "type statement new type"));

    // Create the type symbol for this type, that will point to the old type.
    _AliasSym = MakeShared<analyse::scopes::TypeSymbol>(
        NewType, nullptr, nullptr, sm->CurrentScope, sm->CurrentScope->ParentModule());
    _AliasSym->AliasStmt = Unique<TypeStatementAst>(this); // This is BAD but "cleanup" handles mem error.
    sm->CurrentScope->AddTypeSymbolCheckConflict(_AliasSym);

    // Create a new scope for the type statement.
    auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "type-stmt", {NewType.get()}, PosStart());
    sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);

    Ast::Stage2_GenTopLvlScopes(sm, meta);
    GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    sm->MoveOutOfCurrentScope();

    _Generated = true;
}

auto spp::asts::TypeStatementAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);

    // Check the "old type" exists (non-generic).
    meta->Save();
    meta->SkipTypeAnalysisGenericChecks = true;
    OldType->WithoutGenerics()->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();

    // Recursively discover the actual type being mapped to.
    auto [mapped_old_type, attach_generics, tracking_scope] = analyse::utils::type_utils::RecursiveAliasSearch(
        *this, _FromUseStatement, sm->CurrentScope->Parent, sm, meta);

    const auto final_sym = sm->CurrentScope->GetTypeSymbol(mapped_old_type->WithoutGenerics());
    _AliasSym->Type = final_sym->Type;
    _AliasSym->LinkedScope = final_sym->LinkedScope;
    _AliasSym->IsCopyable = [final_sym] { return final_sym->IsCopyable(); };
    _TrackingScope = tracking_scope;
    MappedOldType = mapped_old_type;

    if (attach_generics != nullptr and not attach_generics->Params.IsEmpty()) {
        GnParamGroup = attach_generics;
        GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
    }
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
    OldType = MappedOldType;

    // Get the old type's symbol, without generics.
    const auto stripped_old_sym = sm->CurrentScope->GetTypeSymbol(OldType->WithoutGenerics(), false);
    if (not stripped_old_sym->IsGeneric) {
        auto tm = analyse::scopes::ScopeManager(sm->GlobalScope, _TrackingScope);
        GnParamGroup->Stage4_QualifyTypes(&tm, meta);
        OldType->Stage4_QualifyTypes(&tm, meta); // Qualify from scope of lowest level alias
        OldType->Stage7_AnalyseSemantics(sm, meta); // Analyse in this scope (generics are in this scope)

        const auto old_sym = sm->CurrentScope->GetTypeSymbol(OldType);
        _AliasSym->Type = old_sym->Type;
        _AliasSym->LinkedScope = old_sym->LinkedScope;
        old_sym->AliasedBySyms.EmplaceBack(_AliasSym);
    }
    MappedOldType = OldType;
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::func_utils::EnforceGenericConstraintsAllArgs;
    for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }

    // If this is a pre-generated AST (mod/sup context), skip any generation steps.
    if (_Generated) {
        sm->MoveToNextScope();
        SPP_ASSERT(sm->CurrentScope == _Scope);
        OldType->ResetCache();
        OldType->Stage7_AnalyseSemantics(sm, meta);

        const auto cls_sym = sm->CurrentScope->GetTypeSymbol(OldType);
        if (cls_sym->Type)
            EnforceGenericConstraintsAllArgs(
                *cls_sym->Type->GnParamGroup, *GenericArgumentGroupAst::FromParams(*GnParamGroup),
                *sm->CurrentScope, *sm, *meta);

        sm->MoveOutOfCurrentScope();
        return;
    }

    // Otherwise, run all generation steps.
    const auto current_scope = sm->CurrentScope;
    auto iter_copy = sm->CurrentIterator();

    sm->Reset(current_scope, iter_copy);
    iter_copy = sm->CurrentIterator();
    Stage2_GenTopLvlScopes(sm, meta);

    sm->Reset(current_scope, iter_copy);
    iter_copy = sm->CurrentIterator();
    Stage3_GenTopLvlAliases(sm, meta);

    // sm->Reset(current_scope, iter_copy);
    // Stage4_QualifyTypes(sm, meta);
}

auto spp::asts::TypeStatementAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
    sm->MoveOutOfCurrentScope();
}

auto spp::asts::TypeStatementAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::TypeStatementAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    sm->MoveToNextScope();
    // SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->MoveOutOfCurrentScope();
    return nullptr;
}

auto spp::asts::TypeStatementAst::MarkFromUseStatement()
    -> void {
    _FromUseStatement = true;
}

auto spp::asts::TypeStatementAst::IsFromUseStatement() const
    -> bool {
    return _FromUseStatement;
}

auto spp::asts::TypeStatementAst::CleanUp()
    -> void {
    // Because the TypeStatementAst is passed as a unique pointer to the TypeSymbol, we need to clear it from the type
    // symbol without destroying it, otherwise there is a use after free because of a double destruction; the unique
    // pointer destroying the type statement, then the type statement destroying itself (via the destructor). Releasing
    // it here prevents the type symbol from destroying it.
    if (_AliasSym != nullptr) {
        (void)_AliasSym->AliasStmt.release();
        _AliasSym = nullptr;
    }

    // Now this pointer has been released from the type symbol, we can safely destroy the type statement.
}

SPP_MOD_END
