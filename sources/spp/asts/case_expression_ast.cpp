module spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;


spp::asts::CaseExpressionAst::CaseExpressionAst(
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches) :
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
}


spp::asts::CaseExpressionAst::~CaseExpressionAst() = default;


auto spp::asts::CaseExpressionAst::to_rust() const
    -> std::string {
    auto out = std::string(tok_of == nullptr ? "if " : "match ");
    out.append(cond->to_rust()).append(" {\n");
    for (const auto &branch : branches) {
        out.append(branch->to_rust()).append("\n");
    }
    out.append("}");
    return out;
}
