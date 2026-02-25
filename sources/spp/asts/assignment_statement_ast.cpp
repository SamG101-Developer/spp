module spp.asts.assignment_statement_ast;
import spp.asts.expression_ast;


spp::asts::AssignmentStatementAst::AssignmentStatementAst(
    decltype(lhs) &&lhs,
    decltype(rhs) &&rhs) :
    lhs(std::move(lhs)),
    rhs(std::move(rhs)) {
}


spp::asts::AssignmentStatementAst::~AssignmentStatementAst() = default;


auto spp::asts::AssignmentStatementAst::to_rust() const
    -> std::string {
    auto out = std::string();
    for (const auto &[lhs_part, rhs_part] : std::views::zip(lhs, rhs)) {
        out.append(lhs_part->to_rust()).append(" = ").append(rhs_part->to_rust()).append(";\n");
    }
    return out;
}
