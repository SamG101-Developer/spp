#include <algorithm>

#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::GenericParameterGroupAst::GenericParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::GenericParameterGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericParameterGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::GenericParameterGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(params);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
