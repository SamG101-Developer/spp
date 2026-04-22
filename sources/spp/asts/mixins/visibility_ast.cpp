module;
#include <spp/macros.hpp>

module spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.visibility;


SPP_MOD_BEGIN
spp::asts::mixins::VisibilityAst::VisibilityAst() :
    visibility(std::make_pair(utils::Visibility::PRIVATE, nullptr)) {
}


spp::asts::mixins::VisibilityAst::~VisibilityAst() = default;

SPP_MOD_END
