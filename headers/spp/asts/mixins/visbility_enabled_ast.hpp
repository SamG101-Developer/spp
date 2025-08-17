#pragma once

#include <cstdint>
#include <spp/asts/_fwd.hpp>
#include <spp/asts/utils/visbility.hpp>


namespace spp::asts::mixins {
    class VisibilityEnabledAst;
    using VisibilityPair = std::pair<utils::Visibility, AnnotationAst*>;
}


class spp::asts::mixins::VisibilityEnabledAst {
    friend struct AnnotationAst;

protected:
    VisibilityPair m_visibility;

public:
    VisibilityEnabledAst();
};
