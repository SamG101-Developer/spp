#include <algorithm>

#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>

#include <spp/asts/token_ast.hpp>


spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(tok_comparison) &&tok_comparison,
    decltype(patterns) &&patterns,
    decltype(guard) &&guard,
    decltype(body) &&body):
    tok_comparison(std::move(tok_comparison)),
    patterns(std::move(patterns)),
    guard(std::move(guard)),
    body(std::move(body)) {
}


auto spp::asts::CaseExpressionBranchAst::pos_end() -> std::size_t {
    return body->pos_end();
}


spp::asts::CaseExpressionBranchAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_comparison);
    SPP_STRING_EXTEND(patterns);
    SPP_STRING_APPEND(guard);
    SPP_STRING_APPEND(body);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionBranchAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_comparison);
    SPP_PRINT_EXTEND(patterns);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_APPEND(body);
    SPP_PRINT_END;
}
