#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_array_shorthand_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <spp/asts/generate/common_types.hpp>


spp::asts::TypeArrayShorthandAst::TypeArrayShorthandAst(
    decltype(tok_l) &&tok_l,
    decltype(element_type) &&element_type,
    decltype(tok_semicolon) &&tok_semicolon,
    decltype(size) &&size,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    element_type(std::move(element_type)),
    tok_semicolon(std::move(tok_semicolon)),
    size(std::move(size)),
    tok_r(std::move(tok_r)) {
}


spp::asts::TypeArrayShorthandAst::~TypeArrayShorthandAst() = default;


auto spp::asts::TypeArrayShorthandAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::TypeArrayShorthandAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::TypeArrayShorthandAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypeArrayShorthandAst>(
        ast_clone(tok_l),
        ast_clone(element_type),
        ast_clone(tok_semicolon),
        ast_clone(size),
        ast_clone(tok_r));
}


spp::asts::TypeArrayShorthandAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(element_type);
    SPP_STRING_APPEND(tok_semicolon);
    SPP_STRING_APPEND(size);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::TypeArrayShorthandAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(element_type);
    SPP_PRINT_APPEND(tok_semicolon);
    SPP_PRINT_APPEND(size);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::TypeArrayShorthandAst::convert() -> std::unique_ptr<TypeAst> {
    return generate::common_types::array_type(pos_start(), std::move(element_type), std::move(size));
}
