module spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_expression_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.inner_scope_expression_ast;


spp::asts::CaseExpressionBranchAst::CaseExpressionBranchAst(
    decltype(op) &&op,
    decltype(patterns) &&patterns,
    decltype(guard) &&guard,
    decltype(body) &&body) :
    op(std::move(op)),
    patterns(std::move(patterns)),
    guard(std::move(guard)),
    body(std::move(body)) {
}


spp::asts::CaseExpressionBranchAst::~CaseExpressionBranchAst() = default;


auto spp::asts::CaseExpressionBranchAst::to_rust() const
    -> std::string {
    auto out = std::string();
    if (op == nullptr) {
        out.append(patterns[0]->to_rust());
        return out;
    }

    auto rust_pats = patterns | std::views::transform([](const auto &pat) { return pat->to_rust(); });
    out.append(std::ranges::views::join(rust_pats, " | "));
    if (guard != nullptr) {
        out.append(" if ").append(guard->to_rust());
    }
    out.append(" => ").append(body->to_rust());
    return out;
}
