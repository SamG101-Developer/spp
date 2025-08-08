#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::CmpStatementAst::CmpStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(value) &&value):
    annotations(std::move(annotations)),
    tok_cmp(std::move(tok_cmp)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    value(std::move(value)) {
}


auto spp::asts::CmpStatementAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::CmpStatementAst::pos_end() const -> std::size_t {
    return value->pos_end();
}


spp::asts::CmpStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cmp);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(value);
    SPP_STRING_END;
}


auto spp::asts::CmpStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cmp);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(value);
    SPP_PRINT_END;
}
