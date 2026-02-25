module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureObjectAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureObjectAst final : CasePatternVariantAst {
    std::unique_ptr<TypeAst> type;
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    explicit CasePatternVariantDestructureObjectAst(
        decltype(type) type,
        decltype(elems) &&elems);
    ~CasePatternVariantDestructureObjectAst() override;
    auto to_rust() const -> std::string override;
};
