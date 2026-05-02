module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.class_attribute_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
spp::asts::ClassAttributeAst::ClassAttributeAst(
    decltype(Annotations) &&annotations,
    decltype(Name) &&name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type,
    decltype(DefaultVal) &&default_val) :
    Annotations(std::move(annotations)),
    Name(std::move(name)),
    TokColon(std::move(tok_colon)),
    Type(std::move(type)),
    DefaultVal(std::move(default_val)) {
}

spp::asts::ClassAttributeAst::~ClassAttributeAst() = default;

auto spp::asts::ClassAttributeAst::PosStart() const
    -> std::size_t {
    // Use the "name".
    return Name->PosStart();
}

auto spp::asts::ClassAttributeAst::PosEnd() const
    -> std::size_t {
    // Use the "type".
    return Type->PosEnd();
}

auto spp::asts::ClassAttributeAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    auto ast = MakeUnique<ClassAttributeAst>(
        AstCloneVec(Annotations), AstClone(Name), AstClone(TokColon), AstClone(Type), AstClone(DefaultVal));
    ast->_Ctx = _Ctx;
    ast->_Scope = _Scope;
    for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
    return ast;
}

auto spp::asts::ClassAttributeAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Annotations, "\n").append(not Annotations.IsEmpty() ? "\n" : "");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type);
    SPP_STRING_APPEND(DefaultVal);
    SPP_STRING_END;
}

auto spp::asts::ClassAttributeAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations.
    Ast::Stage1_PreProcess(ctx);
    for (auto const &a : Annotations) { a->SetAstCtx(this); }
}

auto spp::asts::ClassAttributeAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the generation steps for the annotations.
    for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

    // Create a variable symbol for this attribute in the current scope (class scope).
    auto sym = MakeShared<analyse::scopes::VariableSymbol>(
        Name, Type, sm->CurrentScope, false, false, Visibility.First);
    sm->CurrentScope->AddVarSymbol(std::move(sym));
}

auto spp::asts::ClassAttributeAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
}

auto spp::asts::ClassAttributeAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::type_utils::IsTypeBorrowed;
    for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }

    // Ensure that the convention type doesn't have a convention.
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Type, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Type, *Type, "attribute type"));

    // Check the type is valid before scopes are attached.
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName();
    sm->CurrentScope->GetVarSymbol(Name)->Type = Type;
}

auto spp::asts::ClassAttributeAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // This can be reached via stage 4 generic substitution, so prevent that.
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::IsTypeBorrowed;
    using analyse::utils::type_utils::TypeEq;

    if (meta->CurrentStage == 9) {
        for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
    }

    // Repeated convention check for generic substitutions.
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Type, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Type, *Type, "substituted function return type"));

    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName();
    const auto var_sym = sm->CurrentScope->GetVarSymbol(Name);
    var_sym->Type = Type;

    if (meta->CurrentStage == 9 and DefaultVal != nullptr) {
        DefaultVal->Stage7_AnalyseSemantics(sm, meta);
        const auto default_type = DefaultVal->InferType(sm, meta);

        // Make sure the default's inferred type matches the attribute's type.
        RaiseIf<SppTypeMismatchError>(
            not TypeEq(*Type, *default_type, *sm->CurrentScope, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*this, *Type, *DefaultVal, *default_type));
    }
}

auto spp::asts::ClassAttributeAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is a default value, check it for memory errors.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    if (DefaultVal == nullptr) { return; }

    DefaultVal->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(
        *DefaultVal, *DefaultVal, *sm, true, true, true, true, true, meta);
}

auto spp::asts::ClassAttributeAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
}

SPP_MOD_END
