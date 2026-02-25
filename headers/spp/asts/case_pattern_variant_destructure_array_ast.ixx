module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureArrayAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureArrayAst final : CasePatternVariantAst {
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    explicit CasePatternVariantDestructureArrayAst(
        decltype(elems) &&elems);
    ~CasePatternVariantDestructureArrayAst() override;
    auto to_rust() const -> std::string override;
};
