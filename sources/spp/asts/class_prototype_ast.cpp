module spp.asts.class_prototype_ast;
import spp.asts.annotation_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_member_ast;
import spp.asts.class_implementation_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.type_ast;


spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(name) name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(impl) &&impl) :
    annotations(std::move(annotations)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    impl(std::move(impl)) {
}


spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;


auto spp::asts::ClassPrototypeAst::to_rust() const
    -> std::string {
    // Create the struct definition for the class.
    auto out = std::string();
    for (const auto &annotation : annotations) {
        out.append(annotation->to_rust()).append("\n");
    }
    out.append("struct ").append(name->to_rust());
    if (generic_param_group != nullptr) {
        out.append(generic_param_group->to_rust());
    }
    out.append(impl->to_rust());

    auto has_default_val = [](ClassMemberAst &field) {
        const auto attr = dynamic_cast<ClassAttributeAst*>(&field);
        return attr ? attr->default_val.get() : nullptr;
    };

    // If any of the variables have a default value, we need to implement the Default trait for the struct.
    auto default_map = std::vector<std::pair<std::string, std::string>>{};
    for (const auto &member : impl->members) {
        if (const auto default_val = has_default_val(*member)) {
            default_map.emplace_back(member->to_rust(), default_val->to_rust());
        }
    }

    if (not default_map.empty()) {
        out.append("\n\nimpl Default for ").append(name->to_rust()).append(" {\n");
        out.append("    fn default() -> Self {\n");
        out.append("        Self {\n");
        for (const auto &[field, default_val] : default_map) {
            out.append("            ").append(field).append(": ").append(default_val).append(",\n");
        }
        out.append("        }\n");
        out.append("    }\n");
        out.append("}");
    }
}
