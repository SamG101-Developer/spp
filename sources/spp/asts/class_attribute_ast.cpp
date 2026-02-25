module spp.asts.class_attribute_ast;
import spp.asts.annotation_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;


spp::asts::ClassAttributeAst::ClassAttributeAst(
    decltype(annotations) &&annotations,
    decltype(name) &&name,
    decltype(type) &&type,
    decltype(default_val) &&default_val) :
    annotations(std::move(annotations)),
    name(std::move(name)),
    type(std::move(type)),
    default_val(std::move(default_val)) {
}


spp::asts::ClassAttributeAst::~ClassAttributeAst() = default;


auto spp::asts::ClassAttributeAst::to_rust() const
    -> std::string {
    auto out = std::string();
    for (const auto &annotation : annotations) {
        out.append(annotation->to_rust()).append("\n");
    }
    return out.append(std::format("{}: {}", name->to_rust(), type->to_rust()));
}
