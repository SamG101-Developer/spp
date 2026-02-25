module spp.asts.annotation_ast;
import spp.asts.identifier_ast;


spp::asts::AnnotationAst::AnnotationAst(
    decltype(name) &&name) :
    name(std::move(name)) {
}


auto spp::asts::AnnotationAst::to_rust() const
    -> std::string {
    return std::format("#[{}]", name->to_rust());
}
