#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ClassAttributeAst::ClassAttributeAst(
        decltype(annotations) &&annotations,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(default_value) &&default_value):
    Ast(pos),
    annotations(std::move(annotations)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)),
    default_value(std::move(default_value)) {
}


auto spp::asts::ClassAttributeAst::pos_end() -> std::size_t {
    return default_value != nullptr ? default_value->pos_end() : type->pos_end();
}


spp::asts::ClassAttributeAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(default_value);
    SPP_STRING_END;
}


auto spp::asts::ClassAttributeAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(default_value);
    SPP_PRINT_END;
}
