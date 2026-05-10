module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.type_utils;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_required_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
auto spp::asts::GenericParameterGroupAst::NewEmpty()
    -> Unique<GenericParameterGroupAst> {
    return MakeUnique<GenericParameterGroupAst>(
        nullptr, decltype(Params)(), nullptr);
}

auto spp::asts::GenericParameterGroupAst::NewEmptyShared()
    -> Shared<GenericParameterGroupAst> {
    return MakeShared<GenericParameterGroupAst>(
        nullptr, decltype(Params)(), nullptr);
}

spp::asts::GenericParameterGroupAst::GenericParameterGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Params) &&params,
    decltype(TokR) &&tok_r) :
    TokL(std::move(tok_l)),
    Params(std::move(params)),
    TokR(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}

spp::asts::GenericParameterGroupAst::~GenericParameterGroupAst() = default;

auto spp::asts::GenericParameterGroupAst::operator+(
    GenericParameterGroupAst const &other) const
    -> Unique<GenericParameterGroupAst> {
    auto new_params = AstClone(this);
    *new_params += other;
    return new_params;
}

auto spp::asts::GenericParameterGroupAst::operator+=(
    GenericParameterGroupAst const &other)
    -> GenericParameterGroupAst& {
    MergeGenerics(AstCloneVec(other.Params));
    return *this;
}

auto spp::asts::GenericParameterGroupAst::PosStart() const
    -> std::size_t {
    // Use the "[" token.
    return TokL->PosStart();
}

auto spp::asts::GenericParameterGroupAst::PosEnd() const
    -> std::size_t {
    // Use the "]" token.
    return TokR->PosEnd();
}

auto spp::asts::GenericParameterGroupAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterGroupAst>(
        AstClone(TokL), AstCloneVec(Params), AstClone(TokR));
}

auto spp::asts::GenericParameterGroupAst::ToString() const
    -> Str {
    SPP_STRING_START;
    if (not Params.IsEmpty()) {
        SPP_STRING_APPEND(TokL);
        SPP_STRING_EXTEND(Params, ", ");
        SPP_STRING_APPEND(TokR);
    }
    SPP_STRING_END;
}

auto spp::asts::GenericParameterGroupAst::MergeGenerics(
    decltype(Params) &&other_params) -> void {
    // Merge the given parameters into this generic parameter group, ensuring no duplicates by name.
    auto existing_names = Vec<Str>();
    for (auto const &p : Params) {
        existing_names.EmplaceBack(p->Name->ToString());
    }
    for (auto &&p : std::move(other_params)) {
        // Don't add duplicate named parameters.
        auto new_name = p->Name->ToString();
        if (genex::contains(existing_names, new_name)) { continue; }
        Params.EmplaceBack(std::move(p));
        existing_names.EmplaceBack(new_name);
    }
}

auto spp::asts::GenericParameterGroupAst::GetRequiredParams() const
    -> Vec<GenericParameterAst*> {
    // Filter by casting.
    auto out = Vec<GenericParameterAst*>();
    for (auto const &p : Params) {
        if (const auto req_type = p->To<GenericParameterTypeRequiredAst>()) {
            out.EmplaceBack(req_type->To<GenericParameterAst>());
        }
        else if (const auto req_comp = p->To<GenericParameterCompRequiredAst>()) {
            out.EmplaceBack(req_comp->To<GenericParameterAst>());
        }
    }
    return out;
}

auto spp::asts::GenericParameterGroupAst::GetOptionalParams() const
    -> Vec<GenericParameterAst*> {
    // Filter by casting.
    auto out = Vec<GenericParameterAst*>();
    for (auto const &p : Params) {
        if (const auto opt_type = p->To<GenericParameterTypeOptionalAst>()) {
            out.EmplaceBack(opt_type->To<GenericParameterAst>());
        }
        else if (const auto opt_comp = p->To<GenericParameterCompOptionalAst>()) {
            out.EmplaceBack(opt_comp->To<GenericParameterAst>());
        }
    }
    return out;
}

auto spp::asts::GenericParameterGroupAst::GetVariadicParams() const
    -> GenericParameterAst* {
    auto variadics = Vec<GenericParameterAst*>();
    for (auto const &p : Params) {
        if (const auto var_type = p->To<GenericParameterTypeVariadicAst>()) {
            variadics.EmplaceBack(var_type->To<GenericParameterAst>());
        }
        else if (const auto var_comp = p->To<GenericParameterCompVariadicAst>()) {
            variadics.EmplaceBack(var_comp->To<GenericParameterAst>());
        }
    }

    return variadics.IsEmpty() ? nullptr : variadics[0];
}

auto spp::asts::GenericParameterGroupAst::GetCompParams() const
    -> Vec<GenericParameterCompAst*> {
    // Filter by casting.
    auto out = Vec<GenericParameterCompAst*>();
    for (auto const &p : Params) {
        if (const auto comp_param = p->To<GenericParameterCompAst>()) {
            out.EmplaceBack(comp_param);
        }
    }
    return out;
}

auto spp::asts::GenericParameterGroupAst::GetTypeParams() const
    -> Vec<GenericParameterTypeAst*> {
    // Filter by casting.
    auto out = Vec<GenericParameterTypeAst*>();
    for (auto const &p : Params) {
        if (const auto type_param = p->To<GenericParameterTypeAst>()) {
            out.EmplaceBack(type_param);
        }
    }
    return out;
}

auto spp::asts::GenericParameterGroupAst::GetAllParams() const
    -> Vec<GenericParameterAst*> {
    // Return all parameters.
    auto out = Vec<GenericParameterAst*>();
    for (auto const &p : Params) {
        out.EmplaceBack(p.get());
    }
    return out;
}

auto spp::asts::GenericParameterGroupAst::OptToReq() const
    -> Unique<GenericParameterGroupAst> {
    // Convert all optional parameters to required parameters.
    auto new_params = Vec<Unique<GenericParameterAst>>();
    for (auto const &p : Params) {
        if (const auto opt_type = p->To<GenericParameterTypeOptionalAst>(); opt_type != nullptr) {
            auto param = MakeUnique<GenericParameterTypeRequiredAst>(AstClone(opt_type->Name), AstClone(opt_type->Constraints));
            auto cast_param = dynamic_unique_cast<GenericParameterAst>(std::move(param));
            new_params.EmplaceBack(std::move(cast_param));
        }
        else if (const auto opt_comp = p->To<GenericParameterCompOptionalAst>(); opt_comp != nullptr) {
            auto param = MakeUnique<GenericParameterCompRequiredAst>(nullptr, AstClone(opt_comp->Name), nullptr, AstClone(opt_comp->Type));
            auto cast_param = dynamic_unique_cast<GenericParameterAst>(std::move(param));
            new_params.EmplaceBack(std::move(cast_param));
        }
        else {
            new_params.EmplaceBack(AstClone(p));
        }
    }

    return MakeUnique<GenericParameterGroupAst>(nullptr, std::move(new_params), nullptr);
}

auto spp::asts::GenericParameterGroupAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the generation steps on the parameters in the group.
    for (auto const &p : Params) { p->Stage2_GenTopLvlScopes(sm, meta); }
}

auto spp::asts::GenericParameterGroupAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the type qualifier steps on each parameter in the group.
    for (auto const &p : Params) { p->Stage4_QualifyTypes(sm, meta); }

    // Do the constraints after all the parameters are qualified. This is because of external generic symbols using
    // non-qualified types when analysing generically substituted constraint types.
    for (auto const &p : GetTypeParams()) {
        p->Constraints->Stage7_AnalyseSemantics(sm, meta);

        // Attach the scopes of the constraint types as sup-scopes to the generic scope.
        for (auto const &constraint : p->Constraints->Constraints) {
            auto constraint_scope = sm->CurrentScope->GetTypeSymbol(constraint)->LinkedScope;
            for (auto const &dummy_scope : p->GetDummyScopes()) {
                dummy_scope->DirectSupScopes.EmplaceBack(constraint_scope);
            }
        }
    }
}

auto spp::asts::GenericParameterGroupAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppIdentifierDuplicateError;
    using analyse::errors::SppOrderInvalidError;

    //
    const auto param_names = Params
        | genex::views::transform([](auto const &x) { return x->Name.get(); })
        | genex::to<Vec>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();

    const auto unordered_params = analyse::utils::order_utils::DoOrderParams(Params
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<Vec>());

    // Mark copyable generics.
    for (auto const &param : GetTypeParams()) {
        for (auto const &constraint : param->Constraints->Constraints) {
            if (analyse::utils::type_utils::IsTypeCopyable(*constraint, *sm)) {
                const auto generic_sym = sm->CurrentScope->GetTypeSymbol(param->Name);
                generic_sym->IsDirectlyCopyable = true;
            }
        }
    }

    // Check there are no duplicate parameter names.
    RaiseIf<SppIdentifierDuplicateError>(
        not param_names.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*param_names[0], *param_names[1], "keyword function-argument"));

    // Check the parameters are in the correct order.
    RaiseIf<SppOrderInvalidError>(
        not unordered_params.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(unordered_params[0].First, *unordered_params[0].Second, unordered_params[1].First, *unordered_params[1].Second));

    // Run the semantic analysis steps on each parameter in the group.
    for (auto const &p : Params) { p->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::GenericParameterGroupAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the memory checks on each parameter in the group.
    for (auto const &p : Params) { p->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::GenericParameterGroupAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Run the code generation steps on each parameter in the group.
    for (auto const &p : Params) { p->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

SPP_MOD_END
