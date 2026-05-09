module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::GenericArgumentTypeAst::GenericArgumentTypeAst(
    decltype(Val) val,
    const utils::OrderableTag order_tag) :
    GenericArgumentAst(order_tag),
    Val(std::move(val)) {
    Source.OriginalVal = AstClone(Val);
}

spp::asts::GenericArgumentTypeAst::~GenericArgumentTypeAst() = default;

auto spp::asts::GenericArgumentTypeAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If the generic arg is generic itself, from type aliasing, do nothing.
    // if (meta->alias_qualifier_scope != nullptr) {
    //     const auto sym = meta->alias_qualifier_scope->GetTypeSymbol(val, true);
    //     if (sym and sym->IsGeneric) { return; }
    // }

    // Qualify the type value without generics, then re-add the generics.
    Val->Stage4_QualifyTypes(sm, meta);

    // The value could be a generic from another scope, so only modify if it exists.
    const auto raw = Val->WithoutGenerics();
    const auto sym = sm->CurrentScope->GetTypeSymbol(raw);
    if (sym and sym->AliasStmt != nullptr) {
        auto temp = sym->FqName()->WithConvention(AstClone(Val->GetConvention()));
        Val = std::move(temp);
    }
    else if (sym != nullptr) {
        auto temp = sym->FqName()->WithConvention(AstClone(Val->GetConvention()));
        temp = temp->WithGenerics(std::move(Val->TypeParts().Back()->GnArgGroup));
        Val = std::move(temp);
    }
}

SPP_MOD_END
