#include <algorithm>

#include <spp/asts/iter_expression_ast.hpp>
#include <spp/asts/iter_expression_branch_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::IterExpressionAst::IterExpressionAst(
    decltype(tok_iter) &&tok_iter,
    decltype(cond) &&cond,
    decltype(tok_of) &&tok_of,
    decltype(branches) &&branches):
    PrimaryExpressionAst(),
    tok_iter(std::move(tok_iter)),
    cond(std::move(cond)),
    tok_of(std::move(tok_of)),
    branches(std::move(branches)) {
}


spp::asts::IterExpressionAst::~IterExpressionAst() = default;


auto spp::asts::IterExpressionAst::pos_start() const -> std::size_t {
    return tok_iter->pos_start();
}


auto spp::asts::IterExpressionAst::pos_end() const -> std::size_t {
    return tok_of->pos_end();
}


spp::asts::IterExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_iter);
    SPP_STRING_APPEND(cond);
    SPP_STRING_APPEND(tok_of);
    SPP_STRING_EXTEND(branches);
    SPP_STRING_END;
}


auto spp::asts::IterExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_iter);
    SPP_PRINT_APPEND(cond);
    SPP_PRINT_APPEND(tok_of);
    SPP_PRINT_EXTEND(branches);
    SPP_PRINT_END;
}
