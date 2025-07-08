#include <algorithm>

#include <spp/asts/case_expression_branch.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(op) &&op,
    decltype(patterns) &&patterns,
    decltype(guard) &&guard,
    decltype(body) &&body):
    op(std::move(op)),
    patterns(std::move(patterns)),
    guard(std::move(guard)),
    body(std::move(body)) {
}


auto spp::asts::CaseExpressionBranchAst::pos_end() -> std::size_t {
    return body->pos_end();
}


spp::asts::CaseExpressionBranchAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(op);
    SPP_STRING_EXTEND(patterns);
    SPP_STRING_APPEND(guard);
    SPP_STRING_EXTEND(body);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionBranchAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(op);
    SPP_PRINT_EXTEND(patterns);
    SPP_PRINT_APPEND(guard);
    SPP_PRINT_EXTEND(body);
    SPP_PRINT_END;
}
