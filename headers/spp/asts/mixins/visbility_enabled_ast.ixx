module;
#include <spp/macros.hpp>

export module spp.asts.mixins.visibility_enabled_ast;
import spp.asts._fwd;
import spp.utils.visibility;

import std;

/// @cond
namespace spp::asts::mixins {
    SPP_EXP struct VisibilityEnabledAst;
    SPP_EXP using VisibilityPair = std::pair<utils::Visibility, AnnotationAst*>;
}
/// @endcond


SPP_EXP struct spp::asts::mixins::VisibilityEnabledAst {
    friend struct asts::AnnotationAst;
    friend struct asts::TypeStatementAst;

protected:
    VisibilityPair m_visibility;

public:
    VisibilityEnabledAst();

    virtual ~VisibilityEnabledAst();
};
