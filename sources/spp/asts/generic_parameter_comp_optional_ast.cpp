module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_comp_optional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericParameterCompOptionalAst::GenericParameterCompOptionalAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) &&name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type), utils::OrderableTag::kOptionalParam),
    TokAssign(std::move(tok_assign)),
    DefaultVal(std::move(default_val)) {
}

spp::asts::GenericParameterCompOptionalAst::~GenericParameterCompOptionalAst() = default;

auto spp::asts::GenericParameterCompOptionalAst::PosStart() const
    -> std::size_t {
    // Use the "cmp" token.
    return TokCmp->PosStart();
}

auto spp::asts::GenericParameterCompOptionalAst::PosEnd() const
    -> std::size_t {
    // Use the default val.
    return DefaultVal->PosEnd();
}

auto spp::asts::GenericParameterCompOptionalAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterCompOptionalAst>(
        AstClone(TokCmp), AstClone(Name), AstClone(TokColon), AstClone(Type), AstClone(TokAssign),
        AstClone(DefaultVal));
}

auto spp::asts::GenericParameterCompOptionalAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokCmp).append(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type).append(" ");
    SPP_STRING_APPEND(TokAssign).append(" ");
    SPP_STRING_APPEND(DefaultVal);
    SPP_STRING_END;
}

auto spp::asts::GenericParameterCompOptionalAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the default value.
    using analyse::errors::SppTypeMismatchError;
    using analyse::utils::type_utils::TypeEq;
    GenericParameterCompAst::Stage7_AnalyseSemantics(sm, meta);
    DefaultVal->Stage7_AnalyseSemantics(sm, meta);

    // Make sure the default expression is of the correct type.
    const auto default_type = DefaultVal->InferType(sm, meta);
    RaiseIf<SppTypeMismatchError>(
        not TypeEq(*Type, *default_type, *sm->CurrentScope, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *Type, *DefaultVal, *default_type));
}

auto spp::asts::GenericParameterCompOptionalAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the default value for memory issues.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    DefaultVal->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(
        *DefaultVal, *DefaultVal, *sm, true, true, true, true, true, meta);
}

SPP_MOD_END
