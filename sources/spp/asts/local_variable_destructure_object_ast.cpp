#include <algorithm>

#include <spp/asts/local_variable_destructure_object_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LocalVariableDestructureObjectAst::LocalVariableDestructureObjectAst(
    decltype(type) &&type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r):
    LocalVariableAst(),
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::LocalVariableDestructureObjectAst::~LocalVariableDestructureObjectAst() = default;


auto spp::asts::LocalVariableDestructureObjectAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureObjectAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::LocalVariableDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureObjectAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
