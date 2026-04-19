module spp.asts;
import spp.asts.utils;


spp::asts::mixins::VisibilityAst::VisibilityAst() :
    visibility(std::make_pair(utils::Visibility::PRIVATE, nullptr)) {
}


spp::asts::mixins::VisibilityAst::~VisibilityAst() = default;
