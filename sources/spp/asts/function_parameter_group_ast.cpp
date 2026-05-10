module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.order_utils;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.utils.types;
import genex;

SPP_MOD_BEGIN
spp::asts::FunctionParameterGroupAst::FunctionParameterGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Params) &&params,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Params(std::move(params)),
    TokR(std::move(tok_r)) {
}

spp::asts::FunctionParameterGroupAst::~FunctionParameterGroupAst() = default;

auto spp::asts::FunctionParameterGroupAst::PosStart() const
    -> std::size_t {
    // Use the "(" token.
    return TokL->PosStart();
}

auto spp::asts::FunctionParameterGroupAst::PosEnd() const
    -> std::size_t {
    // Use the ")" token.
    return TokR->PosEnd();
}

auto spp::asts::FunctionParameterGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FunctionParameterGroupAst>(
        AstClone(TokL), AstCloneVec(Params), AstClone(TokR));
}

auto spp::asts::FunctionParameterGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokL);
    SPP_STRING_EXTEND(Params, ", ");
    SPP_STRING_APPEND(TokR);
    SPP_STRING_END;
}

auto spp::asts::FunctionParameterGroupAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppMultipleSelfParametersError;
    using analyse::errors::SppMultipleVariadicParametersError;
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppOrderInvalidError;

    // Create sets of parameters based on conditions.
    const auto self_params = Params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterSelfAst*>()
        | genex::to<Vec>();

    const auto variadic_params = Params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterVariadicAst*>()
        | genex::to<Vec>();

    const auto param_names = Params
        | genex::views::transform([](auto const &x) { return x->ExtractNames(); })
        | genex::views::join
        | genex::to<Vec>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();

    const auto unordered_params = analyse::utils::order_utils::DoOrderParams(Params
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<Vec>());

    // Check there is only 1 "self" parameter.
    RaiseIf<SppMultipleSelfParametersError>(
        self_params.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*self_params[0], *self_params[1]));

    // Check there is only 1 variadic parameter, and it is last.
    RaiseIf<SppMultipleVariadicParametersError>(
        variadic_params.Len() > 1, {sm->CurrentScope},
        ERR_ARGS(*variadic_params[0], *variadic_params[1]));

    // Check there are no duplicate parameter names.
    RaiseIf<SppIdentifierDuplicateError>(
        not param_names.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*param_names[0], *param_names[1], "keyword function-argument"));

    // Check the parameters are in the correct order.
    RaiseIf<SppOrderInvalidError>(
        not unordered_params.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(unordered_params[1].First, *unordered_params[1].Second, unordered_params[0].First, *unordered_params[0].Second));

    // Analyse the parameters.
    for (auto const &param : Params) { param->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::FunctionParameterGroupAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check each parameter's memory.
    for (auto const &param : Params) { param->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::FunctionParameterGroupAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Code generate each parameter.
    for (auto const &param : Params) { param->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

auto spp::asts::FunctionParameterGroupAst::GetAllParams() const
    -> Vec<FunctionParameterAst*> {
    // Filter by casting.
    auto out = Vec<FunctionParameterAst*>();
    for (auto const &param : Params) { out.push_back(param.get()); }
    return out;
}

auto spp::asts::FunctionParameterGroupAst::GetSelfParam() const
    -> FunctionParameterSelfAst* {
    // Filter by casting.
    auto out = Vec<FunctionParameterSelfAst*>();
    for (auto const &param : Params) {
        if (auto *self_param = dynamic_cast<FunctionParameterSelfAst*>(param.get())) { out.push_back(self_param); }
    }
    return out.IsEmpty() ? nullptr : out[0];
}

auto spp::asts::FunctionParameterGroupAst::GetRequiredParams() const
    -> Vec<FunctionParameterRequiredAst*> {
    // Filter by casting.
    auto out = Vec<FunctionParameterRequiredAst*>();
    for (auto const &param : Params) {
        if (auto *req_param = dynamic_cast<FunctionParameterRequiredAst*>(param.get())) { out.push_back(req_param); }
    }
    return out;
}

auto spp::asts::FunctionParameterGroupAst::GetOptionalParams() const
    -> Vec<FunctionParameterOptionalAst*> {
    // Filter by casting.
    auto out = Vec<FunctionParameterOptionalAst*>();
    for (auto const &param : Params) {
        if (auto *opt_param = dynamic_cast<FunctionParameterOptionalAst*>(param.get())) { out.push_back(opt_param); }
    }
    return out;
}

auto spp::asts::FunctionParameterGroupAst::GetVariadicParams() const
    -> FunctionParameterVariadicAst* {
    // Filter by casting.
    auto out = Vec<FunctionParameterVariadicAst*>();
    for (auto const &param : Params) {
        if (auto *var_param = dynamic_cast<FunctionParameterVariadicAst*>(param.get())) { out.push_back(var_param); }
    }
    return out.IsEmpty() ? nullptr : out[0];
}

auto spp::asts::FunctionParameterGroupAst::GetNonSelfParams() const
    -> Vec<FunctionParameterAst*> {
    // Filter by casting.
    auto out = Vec<FunctionParameterAst*>();
    for (auto const &param : Params) {
        if (dynamic_cast<FunctionParameterSelfAst*>(param.get()) == nullptr) { out.push_back(param.get()); }
    }
    return out;
}

SPP_MOD_END
