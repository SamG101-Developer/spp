module;
#include <spp/macros.hpp>

module spp.asts.generic_argument_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.generic_bindings;
import spp.asts.convention_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
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
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  Val->Stage4_QualifyTypes(sm, meta);
  const auto sym = sm->CurrentScope->GetTypeSymbol(Val.get(), true);
  if (sym and not sym->Alias) {
    auto fq = sym->FqName();
    if (Val->LastTypePart()->GnArgGroup->Args.IsEmpty()) {
      fq = analyse::utils::generic_bindings::WithoutSelfBindingGenerics(fq);
    }
    Val = std::move(fq);
    return;
  }

  const auto sym2 = sm->CurrentScope->GetTypeSymbol(Val->WithoutGenerics().get(), true);
  if (sym2 && !sym2->Alias) {
    const auto fq = sym2->FqName();
    Val = fq->WithGenerics(std::move(Val->LastTypePart()->GnArgGroup))->WithConvention(AstClone(Val->GetConvention()));
  }
}

SPP_MOD_END
