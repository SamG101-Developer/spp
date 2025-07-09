#include <algorithm>

#include <spp/asts/convention_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionCallArgumentGroupAst::FunctionCallArgumentGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(args) &&args,
    decltype(tok_r) &&tok_r):
    tok_l(std::move(tok_l)),
    args(std::move(args)),
    tok_r(std::move(tok_r)) {
}


auto spp::asts::FunctionCallArgumentGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::FunctionCallArgumentGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::FunctionCallArgumentGroupAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(args);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::FunctionCallArgumentGroupAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(args);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
