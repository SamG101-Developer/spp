#include <spp/asts/mixins/visbility_enabled_ast.hpp>


spp::asts::mixins::VisibilityEnabledAst::VisibilityEnabledAst() :
    m_visibility(std::make_pair(utils::Visibility::PRIVATE, nullptr)) {
}


spp::asts::mixins::VisibilityEnabledAst::~VisibilityEnabledAst() = default;
