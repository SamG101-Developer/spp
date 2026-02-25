module;
#include <spp/macros.hpp>

export module spp.asts.class_implementation_ast;
import spp.asts.inner_scope_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClassImplementationAst;
    SPP_EXP_CLS struct ClassMemberAst;
}


SPP_EXP_CLS struct spp::asts::ClassImplementationAst final : InnerScopeAst<std::unique_ptr<ClassMemberAst>> {
    using InnerScopeAst::InnerScopeAst;
    ~ClassImplementationAst() override;
};
