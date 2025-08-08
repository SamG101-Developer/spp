#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>


spp::asts::TypeStatementAst::TypeStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_type) &&tok_type,
    decltype(new_type) &&new_type,
    decltype(generic_param_group) &&generic_param_group,
    decltype(tok_assign) &&tok_assign,
    decltype(old_type) &&old_type):
    annotations(std::move(annotations)),
    tok_type(std::move(tok_type)),
    new_type(std::move(new_type)),
    generic_param_group(std::move(generic_param_group)),
    tok_assign(std::move(tok_assign)),
    old_type(std::move(old_type)) {
}


auto spp::asts::TypeStatementAst::pos_start() const -> std::size_t {
    return tok_type->pos_start();
}


auto spp::asts::TypeStatementAst::pos_end() const -> std::size_t {
    return old_type->pos_end();
}


spp::asts::TypeStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_type);
    SPP_STRING_APPEND(new_type);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(old_type);
    SPP_STRING_END;
}


auto spp::asts::TypeStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_type);
    SPP_PRINT_APPEND(new_type);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(old_type);
    SPP_PRINT_END;
}
