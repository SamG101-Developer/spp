#include <algorithm>

#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterGroupAst::FunctionParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::FunctionParameterGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::FunctionParameterGroupAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterGroupAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(params);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
