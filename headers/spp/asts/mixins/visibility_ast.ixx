module;
#include <spp/macros.hpp>

export module spp.asts:visibility_ast;
import spp.asts.utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct VisibilityAst;
    SPP_EXP_CLS using VisibilityPair = std::pair<utils::Visibility, AnnotationAst*>;
}


SPP_EXP_CLS struct spp::asts::mixins::VisibilityAst {
    VisibilityPair visibility;

    VisibilityAst();

    virtual ~VisibilityAst();
};
