#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::AnnotationAst::AnnotationAst(
    decltype(tok_at_sign) &&tok_at_sign,
    decltype(name) &&name) :
    tok_at_sign(std::move(tok_at_sign)),
    name(std::move(name)) {
}


auto spp::asts::AnnotationAst::operator==(AnnotationAst const &that) const -> bool {
    return *name == *that.name;
}


auto spp::asts::AnnotationAst::pos_end() -> std::size_t {
    return name->pos_end();
}


spp::asts::AnnotationAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_at_sign);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::AnnotationAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_at_sign);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}
