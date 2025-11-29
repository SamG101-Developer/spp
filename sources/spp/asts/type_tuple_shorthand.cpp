module;
#include <spp/macros.hpp>

module spp.asts.type_tuple_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::TypeTupleShorthandAst::TypeTupleShorthandAst(
    decltype(tok_l) &&tok_l,
    decltype(element_types) &&element_types,
    decltype(tok_r) &&tok_r) :
    TempTypeAst(),
    tok_l(std::move(tok_l)),
    element_types(std::move(element_types)),
    tok_r(std::move(tok_r)) {
}


spp::asts::TypeTupleShorthandAst::~TypeTupleShorthandAst() = default;


auto spp::asts::TypeTupleShorthandAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::TypeTupleShorthandAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::TypeTupleShorthandAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<TypeTupleShorthandAst>(
        ast_clone(tok_l),
        ast_clone_vec_shared(element_types),
        ast_clone(tok_r));
}


spp::asts::TypeTupleShorthandAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(element_types);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::TypeTupleShorthandAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(element_types);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::TypeTupleShorthandAst::convert()
    -> std::unique_ptr<TypeAst> {
    const auto type = generate::common_types::tuple_type(pos_start(), std::move(element_types));
    return ast_clone(type);
}
