module;
#include <spp/macros.hpp>

export module spp.asts.sup_implementation_ast;
import spp.asts.inner_scope_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupMemberAst;
}


SPP_EXP_CLS struct spp::asts::SupImplementationAst final : InnerScopeAst<std::unique_ptr<SupMemberAst>> {
    using InnerScopeAst::InnerScopeAst;
    ~SupImplementationAst() override;
};
