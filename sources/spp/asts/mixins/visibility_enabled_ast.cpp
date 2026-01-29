module spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.visibility;


spp::asts::mixins::VisibilityEnabledAst::VisibilityEnabledAst() :
    visibility(std::make_pair(utils::Visibility::PRIVATE, nullptr)) {
}


spp::asts::mixins::VisibilityEnabledAst::~VisibilityEnabledAst() = default;
