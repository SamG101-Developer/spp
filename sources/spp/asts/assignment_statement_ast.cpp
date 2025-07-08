#include <algorithm>

#include <spp/asts/assignment_statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::AssignmentStatementAst::AssignmentStatementAst(
        decltype(lhs) &&lhs,
    decltype(tok_assign) &&tok_assign,
    decltype(rhs) &&rhs):
    Ast(pos),
    lhs(std::move(lhs)),
    tok_assign(std::move(tok_assign)),
    rhs(std::move(rhs)) {
}


auto spp::asts::AssignmentStatementAst::pos_end() -> std::size_t {
    return rhs.back()->pos_end();
}


spp::asts::AssignmentStatementAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(lhs);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_EXTEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::AssignmentStatementAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(lhs);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_EXTEND(rhs);
    SPP_PRINT_END;
}
