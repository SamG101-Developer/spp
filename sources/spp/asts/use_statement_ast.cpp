#include <algorithm>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>
#include <spp/asts/use_statement_ast.hpp>


spp::asts::UseStatementAst::UseStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_use) &&tok_use,
    decltype(old_type) &&old_type):
    annotations(std::move(annotations)),
    tok_use(std::move(tok_use)),
    old_type(std::move(old_type)),
    m_conversion(nullptr) {
}


auto spp::asts::UseStatementAst::pos_start() const -> std::size_t {
    return tok_use->pos_start();
}


auto spp::asts::UseStatementAst::pos_end() const -> std::size_t {
    return old_type->pos_end();
}


spp::asts::UseStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_use);
    SPP_STRING_APPEND(old_type);
    SPP_STRING_END;
}


auto spp::asts::UseStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_use);
    SPP_PRINT_APPEND(old_type);
    SPP_PRINT_END;
}
