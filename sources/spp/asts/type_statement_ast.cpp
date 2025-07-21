#include <spp/asts/type_statement_ast.hpp>


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
