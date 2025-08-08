#include <algorithm>

#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::GenericArgumentGroupAst::GenericArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::GenericArgumentGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericArgumentGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::GenericArgumentGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(args);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::GenericArgumentGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(args);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
