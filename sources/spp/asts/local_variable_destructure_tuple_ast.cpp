#include <algorithm>

#include <spp/asts/local_variable_destructure_tuple_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::LocalVariableDestructureTupleAst::LocalVariableDestructureTupleAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r):
    LocalVariableAst(),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::LocalVariableDestructureTupleAst::~LocalVariableDestructureTupleAst() = default;


auto spp::asts::LocalVariableDestructureTupleAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureTupleAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::LocalVariableDestructureTupleAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureTupleAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
