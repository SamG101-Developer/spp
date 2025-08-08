#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_cls) &&tok_cls,
    decltype(name) &&name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(body) &&body):
    annotations(std::move(annotations)),
    tok_cls(std::move(tok_cls)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    body(std::move(body)) {
}


auto spp::asts::ClassPrototypeAst::pos_start() const -> std::size_t {
    return tok_cls->pos_start();
}


auto spp::asts::ClassPrototypeAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


spp::asts::ClassPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cls);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::ClassPrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cls);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}
