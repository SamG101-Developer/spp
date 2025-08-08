#include <algorithm>

#include <spp/asts/boolean_literal_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::CaseExpressionAst::CaseExpressionAst(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches):
    tok_case(std::move(tok_case)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
}


auto spp::asts::CaseExpressionAst::new_non_pattern_match(
    decltype(tok_case) &&tok_case,
    decltype(cond) &&cond,
    std::unique_ptr<InnerScopeExpressionAst<std::unique_ptr<StatementAst>>> &&first,
    decltype(branches) &&branches) -> std::unique_ptr<CaseExpressionAst> {

    auto patterns = std::vector<std::unique_ptr<CasePatternVariantAst>>(1);
    patterns.push_back(std::make_unique<CasePatternVariantExpressionAst>(BooleanLiteralAst::True(tok_case->pos_start())));
    auto first_branch = std::make_unique<CaseExpressionBranchAst>(nullptr, std::move(patterns), nullptr, std::move(first));
    branches.insert(branches.begin(), std::move(first_branch));

    auto out = std::make_unique<CaseExpressionAst>(std::move(tok_case), std::move(cond), nullptr, std::move(branches));
    return out;
}



auto spp::asts::CaseExpressionAst::pos_start() const -> std::size_t {
    return tok_case->pos_start();
}


auto spp::asts::CaseExpressionAst::pos_end() const -> std::size_t {
    return branches.back()->pos_end();
}


spp::asts::CaseExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_case);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::CaseExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_case);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}
