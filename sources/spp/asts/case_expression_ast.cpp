#include <algorithm>

#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CaseExpressionAst::CaseExpressionAst(
        decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches):
    Ast(pos),
    tok_case(std::move(tok_case)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
}


auto spp::asts::CaseExpressionAst::pos_end() -> std::size_t {
    return branches->members.back()->pos_end();
}


spp::asts::CaseExpressionAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_case);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_case);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}
