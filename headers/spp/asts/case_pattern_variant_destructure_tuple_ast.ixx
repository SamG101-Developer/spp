module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureTupleAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureTupleAst final : CasePatternVariantAst {
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    explicit CasePatternVariantDestructureTupleAst(
        decltype(elems) &&elems);
    ~CasePatternVariantDestructureTupleAst() override;
    auto to_rust() const -> std::string override;
};
