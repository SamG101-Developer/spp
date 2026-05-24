module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_comp_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::GenericParameterCompAst::GenericParameterCompAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    const utils::OrderableTag order_tag) :
    GenericParameterAst(std::move(name), order_tag),
    TokCmp(std::move(tok_cmp)),
    TokColon(std::move(tok_colon)),
    Type(std::move(type)) {
    // Default the "cmp" and ":" tokens if they are null.
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCmp, lex::SppTokenType::KW_CMP, "cmp");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":");
    Source.OriginalType = AstClone(Type);
}

spp::asts::GenericParameterCompAst::~GenericParameterCompAst() = default;

auto spp::asts::GenericParameterCompAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Create a variable symbol for this constant in the current scope (class / function).
    auto sym = MakeUnique<analyse::scopes::VariableSymbol>(
        IdentifierAst::FromType(*Name), Type, sm->CurrentScope,
        false, true, utils::Visibility::kPublic);
    // sym->MemInfo->AstPins.EmplaceBack(Name.get()); TODO
    sym->MemInfo->AstCompTime = AstClone(this);
    sym->MemInfo->InitializedBy(*this, sm->CurrentScope);
    sym->CompTimeValue = AstClone(this); // TODO: this or name?
    sm->CurrentScope->AddVarSymbol(std::move(sym));
}

auto spp::asts::GenericParameterCompAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppSecondClassBorrowViolationError;
    using analyse::utils::type_utils::IsTypeBorrowed;

    // Qualify the type on the generic parameter.
    meta->Save();
    meta->IgnoreCmpGeneric = Name;

    // Ensure that the convention type doesn't have a convention.
    // Todo: an we safely allow this? I don't really see why not?
    RaiseIf<SppSecondClassBorrowViolationError>(
        IsTypeBorrowed(*Type, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Type, *Type, "function return type"));

    // Check the type exists and qualify.
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type)->FqName();
    const auto sym = sm->CurrentScope->GetVarSymbol(
        IdentifierAst::FromType(*Name));
    sym->Type = Type;
    meta->Restore();
}

auto spp::asts::GenericParameterCompAst::Stage7_AnalyseSemantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Analyse the type.
    // type->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::GenericParameterCompAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Return the identifier.
    meta->CmpResult = IdentifierAst::FromType(*Name);
}

auto spp::asts::GenericParameterCompAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // The compile time constants' symbols need to be allocated into the function.
    // Todo: need to be done as a "constant" (see GlobalConstantAst)
    auto cast_name = IdentifierAst::FromType(*Name);
    const auto is_opt = To<GenericParameterCompOptionalAst>();

    meta->Save();
    meta->LetStatementExplicitType = Type;
    meta->LetStatementFromUninitialized = not is_opt;
    meta->LetStatementValue = is_opt ? is_opt->DefaultVal.get() : nullptr;
    const auto var = MakeUnique<LocalVariableSingleIdentifierAst>(nullptr, std::move(cast_name), nullptr);
    const auto alloca = var->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();

    return alloca;
}

SPP_MOD_END
