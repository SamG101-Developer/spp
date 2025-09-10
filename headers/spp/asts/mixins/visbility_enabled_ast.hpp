#pragma once

#include <spp/asts/_fwd.hpp>
#include <spp/asts/utils/visbility.hpp>
#include <spp/pch.hpp>


namespace spp::asts::mixins {
    struct VisibilityEnabledAst;
    using VisibilityPair = std::pair<utils::Visibility, AnnotationAst*>;
}


struct spp::asts::mixins::VisibilityEnabledAst {
    friend struct asts::AnnotationAst;
    friend struct asts::TypeStatementAst;

protected:
    VisibilityPair m_visibility;

public:
    VisibilityEnabledAst();

    virtual ~VisibilityEnabledAst();
};
