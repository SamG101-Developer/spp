#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/iter_expression_branch_ast.hpp>
#include <spp/asts/iter_pattern_variant_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IterExpressionBranchAst::IterExpressionBranchAst(
    decltype(patterns) &&patterns,
    decltype(body) &&body,
    decltype(guard) &&guard):
    Ast(),
    patterns(std::move(patterns)),
    body(std::move(body)),
    guard(std::move(guard)) {
}


spp::asts::IterExpressionBranchAst::~IterExpressionBranchAst() = default;


auto spp::asts::IterExpressionBranchAst::pos_start() const -> std::size_t {
    return patterns->pos_start();
}


auto spp::asts::IterExpressionBranchAst::pos_end() const -> std::size_t {
    return body->pos_end();
}


spp::asts::IterExpressionBranchAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(patterns);
    SPP_STRING_APPEND(body);
    SPP_STRING_APPEND(guard);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionBranchAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(patterns);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_END;
}
