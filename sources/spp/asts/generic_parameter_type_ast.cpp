module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.annotation_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;

SPP_MOD_BEGIN
spp::asts::GenericParameterTypeAst::GenericParameterTypeAst(
    decltype(Name) name,
    decltype(Constraints) &&constraints,
    const utils::OrderableTag order_tag) :
    GenericParameterAst(std::move(name), order_tag),
    Constraints(std::move(constraints)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Constraints);
}

spp::asts::GenericParameterTypeAst::~GenericParameterTypeAst() = default;

auto spp::asts::GenericParameterTypeAst::Stage2_GenTopLvlScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *)
    -> void {
    //
    using utils::Visibility;

    // Create a dummy scope for the generic type.
    auto dummy_scope_name = analyse::scopes::ScopeBlockName::FromParts(
        "generic-parameter-type", {Name->TypeParts().Back()}, PosStart());
    _DummyAst = MakeUnique<ClassPrototypeAst>(SPP_NO_ANNOTATIONS, nullptr, nullptr, nullptr, nullptr);
    auto dummy_scope = MakeUnique<analyse::scopes::Scope>(
        dummy_scope_name, sm->CurrentScope, _DummyAst.Get());

    // Create the type symbol for the generic parameter.
    auto sym = MakeUnique<analyse::scopes::TypeSymbol>(
        Name->TypeParts().Back(), nullptr, dummy_scope.Get(), sm->CurrentScope, nullptr, true, false,
        Visibility::kPublic, nullptr, Constraints->GetAllConstraints());
    dummy_scope->TySym = sym.Get();
    sm->CurrentScope->AddTypeSymbol(std::move(sym));

    _DummyScopes.EmplaceBack(dummy_scope.Get());
    analyse::scopes::ScopeManager::temp_scopes.EmplaceBack(std::move(dummy_scope));
}

auto spp::asts::GenericParameterTypeAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Qualify the name.
    Name->Stage4_QualifyTypes(sm, meta);
}

auto spp::asts::GenericParameterTypeAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Analyse the name.
    Name->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::GenericParameterTypeAst::GetDummyScopes() const
    -> std::span<analyse::scopes::Scope* const> {
    // View the dummy scope vector.
    return _DummyScopes.ToView();
}

SPP_MOD_END
