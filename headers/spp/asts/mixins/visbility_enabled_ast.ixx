module;
#include <spp/macros.hpp>

export module spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.visibility;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct VisibilityEnabledAst;
    SPP_EXP_CLS using VisibilityPair = std::pair<utils::Visibility, AnnotationAst*>;
}


SPP_EXP_CLS struct spp::asts::mixins::VisibilityEnabledAst {
    friend struct spp::asts::AnnotationAst;
    friend struct spp::asts::TypeStatementAst;

protected:
    VisibilityPair m_visibility;

public:
    VisibilityEnabledAst();

    virtual ~VisibilityEnabledAst();
};
