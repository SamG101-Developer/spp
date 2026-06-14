module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.identifier_ast;
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
    Source.OriginalValPosStart = Val ? Val->PosStart() : 0;
    Source.OriginalValPosEnd = Val ? Val->PosEnd() : 0;
}

spp::asts::GenericArgumentTypeAst::~GenericArgumentTypeAst() = default;

auto spp::asts::GenericArgumentTypeAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    Val->Stage4_QualifyTypes(sm, meta);
    const auto sym = sm->CurrentScope->GetTypeSymbol(Val, true);
    if (sym and not sym->AliasStmt) {
        const auto fq = sym ? sym->FqName() : nullptr;
        Val = fq ? fq : Val;
        return;
    }

    const auto sym2 = sm->CurrentScope->GetTypeSymbol(Val->WithoutGenerics(), true);
    if (sym2 && !sym2->AliasStmt) {
        const auto fq = sym2->FqName();
        Val = fq->WithGenerics(std::move(Val->TypeParts().Back()->GnArgGroup))->WithConvention(AstClone(Val->GetConvention()));
    }
}

SPP_MOD_END
